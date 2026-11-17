#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "mental1104/concurrency/circuit_breaker.h"

namespace {

class ManualClock {
public:
  typedef std::chrono::steady_clock Clock;
  ManualClock() : now_(Clock::time_point()) {}

  Clock::time_point now() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return now_;
  }

  void advance(std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    now_ += duration;
  }

private:
  mutable std::mutex mutex_;
  Clock::time_point now_;
};

mental1104::CircuitBreakerConfig test_config() {
  mental1104::CircuitBreakerConfig config;
  config.window = std::chrono::milliseconds(10000);
  config.minimum_requests = 3;
  config.failure_rate_threshold = 0.5;
  config.slow_call_duration = std::chrono::milliseconds(1000);
  config.slow_call_rate_threshold = 0.5;
  config.open_duration = std::chrono::milliseconds(5000);
  config.half_open_max_probes = 3;
  config.half_open_successes_to_close = 3;
  return config;
}

std::unique_ptr<mental1104::CircuitBreaker>
make_breaker(const std::shared_ptr<ManualClock> &clock,
             const mental1104::StateChangeListener &listener =
                 mental1104::StateChangeListener()) {
  return std::unique_ptr<mental1104::CircuitBreaker>(
      new mental1104::CircuitBreaker(
          test_config(), listener,
          [clock]() { return clock->now(); }));
}

void open_breaker(mental1104::CircuitBreaker &breaker) {
  for (int i = 0; i < 3; ++i) {
    breaker.try_acquire().record_failure();
  }
  ASSERT_EQ(mental1104::CircuitState::Open, breaker.state());
}

} // namespace

TEST(CircuitBreakerTest, RejectsInvalidConfiguration) {
  mental1104::CircuitBreakerConfig config = test_config();
  config.window = std::chrono::milliseconds(0);
  EXPECT_THROW(mental1104::CircuitBreaker breaker(config),
               std::invalid_argument);
  config = test_config();
  config.minimum_requests = 0;
  EXPECT_THROW(mental1104::CircuitBreaker breaker(config),
               std::invalid_argument);
  config = test_config();
  config.failure_rate_threshold = 1.1;
  EXPECT_THROW(mental1104::CircuitBreaker breaker(config),
               std::invalid_argument);
  config = test_config();
  config.half_open_successes_to_close = 4;
  EXPECT_THROW(mental1104::CircuitBreaker breaker(config),
               std::invalid_argument);
}

TEST(CircuitBreakerTest, MinimumRequestsAndFailureRateOpenCircuit) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  breaker->try_acquire().record_failure();
  breaker->try_acquire().record_success();
  EXPECT_EQ(mental1104::CircuitState::Closed, breaker->state());
  breaker->try_acquire().record_failure();
  EXPECT_EQ(mental1104::CircuitState::Open, breaker->state());
}

TEST(CircuitBreakerTest, SlowSuccessfulCallsCanOpenCircuit) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  mental1104::CircuitBreakerConfig config = test_config();
  config.minimum_requests = 2;
  config.failure_rate_threshold = 1.0;
  mental1104::CircuitBreaker breaker(config, mental1104::StateChangeListener(),
                                     [clock]() { return clock->now(); });
  breaker.try_acquire().record_success();
  mental1104::CircuitPermit permit = breaker.try_acquire();
  clock->advance(std::chrono::milliseconds(1000));
  permit.record_success();
  EXPECT_EQ(mental1104::CircuitState::Open, breaker.state());
}

TEST(CircuitBreakerTest, IgnoredErrorsAreExcludedFromWindow) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  breaker->try_acquire().record_ignored();
  breaker->try_acquire().record_failure();
  const mental1104::CircuitBreakerSnapshot snapshot = breaker->snapshot();
  EXPECT_EQ(1U, snapshot.window_requests);
  EXPECT_EQ(1U, snapshot.window_failures);
  EXPECT_EQ(mental1104::CircuitState::Closed, snapshot.state);
}

TEST(CircuitBreakerTest, SlidingWindowPrunesExpiredEvents) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  mental1104::CircuitBreakerConfig config = test_config();
  config.window = std::chrono::milliseconds(2000);
  mental1104::CircuitBreaker breaker(config, mental1104::StateChangeListener(),
                                     [clock]() { return clock->now(); });
  breaker.try_acquire().record_failure();
  clock->advance(std::chrono::milliseconds(2100));
  breaker.try_acquire().record_success();
  const mental1104::CircuitBreakerSnapshot snapshot = breaker.snapshot();
  EXPECT_EQ(1U, snapshot.window_requests);
  EXPECT_EQ(0U, snapshot.window_failures);
}

TEST(CircuitBreakerTest, OpenFastFailsAndFallbackDoesNotCallDownstream) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  open_breaker(*breaker);
  std::atomic<int> calls(0);
  EXPECT_THROW(mental1104::execute(*breaker, [&calls]() {
                 ++calls;
                 return 1;
               }),
               mental1104::CircuitOpenError);
  const int value = mental1104::execute_or_fallback(
      *breaker,
      [&calls]() {
        ++calls;
        return 1;
      },
      [](const mental1104::CircuitOpenError &) { return 7; });
  EXPECT_EQ(7, value);
  EXPECT_EQ(0, calls.load());
}

TEST(CircuitBreakerTest, HalfOpenLimitsProbeRoundAndCloses) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  open_breaker(*breaker);
  clock->advance(std::chrono::milliseconds(5000));

  std::vector<mental1104::CircuitPermit> permits;
  for (int i = 0; i < 3; ++i) {
    permits.push_back(breaker->try_acquire());
  }
  EXPECT_THROW(breaker->try_acquire(), mental1104::CircuitOpenError);
  permits[0].record_success();
  permits[1].record_ignored();
  permits[2].record_success();
  EXPECT_EQ(mental1104::CircuitState::Closed, breaker->state());
}

TEST(CircuitBreakerTest, FailedOrSlowHalfOpenProbeReopens) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  open_breaker(*breaker);
  clock->advance(std::chrono::milliseconds(5000));
  breaker->try_acquire().record_failure();
  EXPECT_EQ(mental1104::CircuitState::Open, breaker->state());

  clock->advance(std::chrono::milliseconds(5000));
  mental1104::CircuitPermit permit = breaker->try_acquire();
  clock->advance(std::chrono::milliseconds(1000));
  permit.record_success();
  EXPECT_EQ(mental1104::CircuitState::Open, breaker->state());
}

TEST(CircuitBreakerTest, DuplicateAndStaleCompletionsAreIgnored) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  mental1104::CircuitPermit stale = breaker->try_acquire();
  open_breaker(*breaker);
  EXPECT_TRUE(stale.record_success());
  EXPECT_FALSE(stale.record_failure());
  EXPECT_EQ(0U, breaker->snapshot().window_requests);
}

TEST(CircuitBreakerTest, ConcurrentHalfOpenAcquisitionRespectsLimit) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::unique_ptr<mental1104::CircuitBreaker> breaker = make_breaker(clock);
  open_breaker(*breaker);
  clock->advance(std::chrono::milliseconds(5000));

  std::atomic<int> accepted(0);
  std::atomic<int> rejected(0);
  std::mutex permits_mutex;
  std::vector<mental1104::CircuitPermit> permits;
  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.push_back(std::thread([&]() {
      try {
        mental1104::CircuitPermit permit = breaker->try_acquire();
        {
          std::lock_guard<std::mutex> lock(permits_mutex);
          permits.push_back(std::move(permit));
        }
        ++accepted;
      } catch (const mental1104::CircuitOpenError &) {
        ++rejected;
      }
    }));
  }
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }
  EXPECT_EQ(3, accepted.load());
  EXPECT_EQ(7, rejected.load());
  for (std::size_t i = 0; i < permits.size(); ++i) {
    permits[i].record_success();
  }
  EXPECT_EQ(mental1104::CircuitState::Closed, breaker->state());
}

TEST(CircuitBreakerTest, ListenerFailuresDoNotBreakStateMachine) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  std::vector<mental1104::StateChangeReason> reasons;
  std::mutex reasons_mutex;
  mental1104::StateChangeListener listener = [&](const mental1104::StateChange &change) {
    {
      std::lock_guard<std::mutex> lock(reasons_mutex);
      reasons.push_back(change.reason);
    }
    if (change.reason == mental1104::StateChangeReason::CooldownElapsed) {
      throw std::runtime_error("observer failed");
    }
  };
  std::unique_ptr<mental1104::CircuitBreaker> breaker =
      make_breaker(clock, listener);
  open_breaker(*breaker);
  clock->advance(std::chrono::milliseconds(5000));
  std::vector<mental1104::CircuitPermit> permits;
  for (int i = 0; i < 3; ++i) {
    permits.push_back(breaker->try_acquire());
  }
  for (std::size_t i = 0; i < permits.size(); ++i) {
    permits[i].record_success();
  }
  EXPECT_EQ(mental1104::CircuitState::Closed, breaker->state());
  EXPECT_EQ(3U, reasons.size());
}

TEST(CircuitBreakerTest, ClassifierCanIgnoreBusinessErrorsAndVoidWorks) {
  std::shared_ptr<ManualClock> clock(new ManualClock());
  mental1104::CircuitBreakerConfig config = test_config();
  config.minimum_requests = 1;
  mental1104::CircuitBreaker breaker(config, mental1104::StateChangeListener(),
                                     [clock]() { return clock->now(); });
  mental1104::ExceptionClassifier classifier =
      [](const std::exception_ptr &) { return false; };
  EXPECT_THROW(mental1104::execute(
                   breaker,
                   []() -> int { throw std::runtime_error("out of stock"); },
                   classifier),
               std::runtime_error);
  EXPECT_EQ(0U, breaker.snapshot().window_requests);
  EXPECT_NO_THROW(mental1104::execute(breaker, []() {}));
}
