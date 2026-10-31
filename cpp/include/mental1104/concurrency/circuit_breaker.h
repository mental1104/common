#ifndef MENTAL1104_CONCURRENCY_CIRCUIT_BREAKER_H
#define MENTAL1104_CONCURRENCY_CIRCUIT_BREAKER_H

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mental1104 {

enum class CircuitState { Closed, Open, HalfOpen };

enum class CircuitOutcome { Success, Failure, Ignored };

enum class StateChangeReason {
  FailureRate,
  SlowCallRate,
  CooldownElapsed,
  HalfOpenSucceeded,
  HalfOpenFailed
};

struct CircuitBreakerConfig {
  std::chrono::milliseconds window;
  std::size_t minimum_requests;
  double failure_rate_threshold;
  std::chrono::milliseconds slow_call_duration;
  double slow_call_rate_threshold;
  std::chrono::milliseconds open_duration;
  std::size_t half_open_max_probes;
  std::size_t half_open_successes_to_close;

  CircuitBreakerConfig()
      : window(10000), minimum_requests(20), failure_rate_threshold(0.5),
        slow_call_duration(800), slow_call_rate_threshold(0.6),
        open_duration(5000), half_open_max_probes(3),
        half_open_successes_to_close(3) {}
};

class CircuitOpenError : public std::runtime_error {
public:
  explicit CircuitOpenError(std::chrono::milliseconds retry_after)
      : std::runtime_error("circuit breaker is open"),
        retry_after_(retry_after.count() < 0 ? std::chrono::milliseconds(0)
                                             : retry_after) {}

  std::chrono::milliseconds retry_after() const { return retry_after_; }

private:
  std::chrono::milliseconds retry_after_;
};

struct StateChange {
  CircuitState previous_state;
  CircuitState new_state;
  StateChangeReason reason;
  std::chrono::steady_clock::time_point at;
  std::uint64_t generation;
};

typedef std::function<void(const StateChange &)> StateChangeListener;
typedef std::function<bool(const std::exception_ptr &)> ExceptionClassifier;

struct CircuitBreakerSnapshot {
  CircuitState state;
  std::uint64_t generation;
  std::size_t window_requests;
  std::size_t window_failures;
  std::size_t window_slow_calls;
  double failure_rate;
  double slow_call_rate;
  std::size_t half_open_issued;
  std::size_t half_open_in_flight;
  std::size_t half_open_successes;
  std::chrono::milliseconds retry_after;
};

namespace circuit_breaker_detail {

struct Event {
  std::chrono::steady_clock::time_point at;
  bool failure;
  bool slow;
};

struct Impl {
  typedef std::chrono::steady_clock Clock;
  typedef Clock::time_point TimePoint;
  typedef std::function<TimePoint()> ClockFunction;

  CircuitBreakerConfig config;
  ClockFunction clock;
  StateChangeListener listener;
  mutable std::mutex mutex;
  CircuitState state;
  std::uint64_t generation;
  TimePoint opened_at;
  std::deque<Event> events;
  std::size_t half_open_issued;
  std::size_t half_open_in_flight;
  std::size_t half_open_successes;

  Impl(const CircuitBreakerConfig &value, const StateChangeListener &callback,
       const ClockFunction &clock_function)
      : config(value), clock(clock_function), listener(callback),
        state(CircuitState::Closed), generation(0), opened_at(), events(),
        half_open_issued(0), half_open_in_flight(0),
        half_open_successes(0) {}
};

inline void validate_config(const CircuitBreakerConfig &config) {
  if (config.window.count() <= 0) {
    throw std::invalid_argument("circuit breaker: window must be positive");
  }
  if (config.minimum_requests == 0) {
    throw std::invalid_argument(
        "circuit breaker: minimum requests must be positive");
  }
  if (!std::isfinite(config.failure_rate_threshold) ||
      config.failure_rate_threshold < 0.0 ||
      config.failure_rate_threshold > 1.0) {
    throw std::invalid_argument(
        "circuit breaker: failure rate threshold must be in [0, 1]");
  }
  if (config.slow_call_duration.count() <= 0) {
    throw std::invalid_argument(
        "circuit breaker: slow call duration must be positive");
  }
  if (!std::isfinite(config.slow_call_rate_threshold) ||
      config.slow_call_rate_threshold < 0.0 ||
      config.slow_call_rate_threshold > 1.0) {
    throw std::invalid_argument(
        "circuit breaker: slow call rate threshold must be in [0, 1]");
  }
  if (config.open_duration.count() <= 0) {
    throw std::invalid_argument(
        "circuit breaker: open duration must be positive");
  }
  if (config.half_open_max_probes == 0) {
    throw std::invalid_argument(
        "circuit breaker: half-open max probes must be positive");
  }
  if (config.half_open_successes_to_close == 0 ||
      config.half_open_successes_to_close > config.half_open_max_probes) {
    throw std::invalid_argument(
        "circuit breaker: invalid half-open success condition");
  }
}

inline void prune_locked(Impl &impl, const Impl::TimePoint &now) {
  const Impl::TimePoint cutoff = now - impl.config.window;
  while (!impl.events.empty() && impl.events.front().at < cutoff) {
    impl.events.pop_front();
  }
}

inline void counts_locked(const Impl &impl, std::size_t &requests,
                          std::size_t &failures, std::size_t &slow_calls) {
  requests = impl.events.size();
  failures = 0;
  slow_calls = 0;
  for (std::deque<Event>::const_iterator it = impl.events.begin();
       it != impl.events.end(); ++it) {
    if (it->failure) {
      ++failures;
    }
    if (it->slow) {
      ++slow_calls;
    }
  }
}

inline std::chrono::milliseconds retry_after_locked(
    const Impl &impl, const Impl::TimePoint &now) {
  if (impl.state != CircuitState::Open) {
    return std::chrono::milliseconds(0);
  }
  const Impl::TimePoint retry_at = impl.opened_at + impl.config.open_duration;
  if (retry_at <= now) {
    return std::chrono::milliseconds(0);
  }
  std::chrono::milliseconds remaining =
      std::chrono::duration_cast<std::chrono::milliseconds>(retry_at - now);
  if (remaining.count() == 0) {
    return std::chrono::milliseconds(1);
  }
  return remaining;
}

inline StateChange transition_locked(Impl &impl, CircuitState new_state,
                                     StateChangeReason reason,
                                     const Impl::TimePoint &now) {
  StateChange change;
  change.previous_state = impl.state;
  change.new_state = new_state;
  change.reason = reason;
  change.at = now;
  change.generation = impl.generation + 1;

  impl.state = new_state;
  ++impl.generation;
  impl.events.clear();
  impl.half_open_issued = 0;
  impl.half_open_in_flight = 0;
  impl.half_open_successes = 0;
  impl.opened_at = new_state == CircuitState::Open ? now : Impl::TimePoint();
  return change;
}

inline void notify(const std::shared_ptr<Impl> &impl,
                   const std::unique_ptr<StateChange> &change) {
  if (!change.get() || !impl->listener) {
    return;
  }
  try {
    impl->listener(*change);
  } catch (...) {
    // Observability hooks must not affect state-machine correctness.
  }
}

inline std::unique_ptr<StateChange>
evaluate_closed_locked(Impl &impl, const Impl::TimePoint &now) {
  std::size_t requests = 0;
  std::size_t failures = 0;
  std::size_t slow_calls = 0;
  counts_locked(impl, requests, failures, slow_calls);
  if (requests < impl.config.minimum_requests) {
    return std::unique_ptr<StateChange>();
  }

  if (static_cast<double>(failures) / static_cast<double>(requests) >=
      impl.config.failure_rate_threshold) {
    return std::unique_ptr<StateChange>(new StateChange(transition_locked(
        impl, CircuitState::Open, StateChangeReason::FailureRate, now)));
  }
  if (static_cast<double>(slow_calls) / static_cast<double>(requests) >=
      impl.config.slow_call_rate_threshold) {
    return std::unique_ptr<StateChange>(new StateChange(transition_locked(
        impl, CircuitState::Open, StateChangeReason::SlowCallRate, now)));
  }
  return std::unique_ptr<StateChange>();
}

inline bool classify_exception(const ExceptionClassifier &classifier,
                               const std::exception_ptr &error) {
  return classifier ? classifier(error) : true;
}

} // namespace circuit_breaker_detail

class CircuitPermit {
public:
  CircuitPermit() : generation_(0), state_(CircuitState::Closed), started_at_() {}

  CircuitPermit(CircuitPermit &&other) noexcept
      : impl_(std::move(other.impl_)), generation_(other.generation_),
        state_(other.state_), started_at_(other.started_at_),
        completed_(std::move(other.completed_)) {}

  CircuitPermit &operator=(CircuitPermit &&other) noexcept {
    if (this != &other) {
      impl_ = std::move(other.impl_);
      generation_ = other.generation_;
      state_ = other.state_;
      started_at_ = other.started_at_;
      completed_ = std::move(other.completed_);
    }
    return *this;
  }

  CircuitPermit(const CircuitPermit &) = delete;
  CircuitPermit &operator=(const CircuitPermit &) = delete;

  CircuitState state() const { return state_; }

  bool complete(CircuitOutcome outcome) {
    if (!impl_.get() || !completed_.get()) {
      return false;
    }
    bool expected = false;
    if (!completed_->compare_exchange_strong(expected, true)) {
      return false;
    }

    std::unique_ptr<StateChange> change;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      const circuit_breaker_detail::Impl::TimePoint now = impl_->clock();
      if (generation_ != impl_->generation || state_ != impl_->state) {
        return true;
      }

      std::chrono::steady_clock::duration duration = now - started_at_;
      if (duration < std::chrono::steady_clock::duration::zero()) {
        duration = std::chrono::steady_clock::duration::zero();
      }
      const bool slow = duration >= impl_->config.slow_call_duration;

      if (impl_->state == CircuitState::Closed) {
        if (outcome != CircuitOutcome::Ignored) {
          circuit_breaker_detail::Event event;
          event.at = now;
          event.failure = outcome == CircuitOutcome::Failure;
          event.slow = slow;
          impl_->events.push_back(event);
          circuit_breaker_detail::prune_locked(*impl_, now);
          change = circuit_breaker_detail::evaluate_closed_locked(*impl_, now);
        }
      } else if (impl_->state == CircuitState::HalfOpen) {
        if (impl_->half_open_in_flight > 0) {
          --impl_->half_open_in_flight;
        }
        if (outcome == CircuitOutcome::Failure || slow) {
          change.reset(new StateChange(circuit_breaker_detail::transition_locked(
              *impl_, CircuitState::Open, StateChangeReason::HalfOpenFailed,
              now)));
        } else {
          ++impl_->half_open_successes;
          if (impl_->half_open_successes >=
                  impl_->config.half_open_successes_to_close &&
              impl_->half_open_in_flight == 0) {
            change.reset(new StateChange(
                circuit_breaker_detail::transition_locked(
                    *impl_, CircuitState::Closed,
                    StateChangeReason::HalfOpenSucceeded, now)));
          }
        }
      }
    }
    circuit_breaker_detail::notify(impl_, change);
    return true;
  }

  bool record_success() { return complete(CircuitOutcome::Success); }
  bool record_failure() { return complete(CircuitOutcome::Failure); }
  bool record_ignored() { return complete(CircuitOutcome::Ignored); }

private:
  friend class CircuitBreaker;

  CircuitPermit(
      const std::shared_ptr<circuit_breaker_detail::Impl> &impl,
      std::uint64_t generation, CircuitState state,
      const std::chrono::steady_clock::time_point &started_at)
      : impl_(impl), generation_(generation), state_(state),
        started_at_(started_at), completed_(new std::atomic<bool>(false)) {}

  std::shared_ptr<circuit_breaker_detail::Impl> impl_;
  std::uint64_t generation_;
  CircuitState state_;
  std::chrono::steady_clock::time_point started_at_;
  std::shared_ptr<std::atomic<bool> > completed_;
};

class CircuitBreaker {
public:
  typedef circuit_breaker_detail::Impl::Clock Clock;
  typedef circuit_breaker_detail::Impl::ClockFunction ClockFunction;

  explicit CircuitBreaker(
      const CircuitBreakerConfig &config = CircuitBreakerConfig(),
      const StateChangeListener &listener = StateChangeListener(),
      const ClockFunction &clock = ClockFunction()) {
    circuit_breaker_detail::validate_config(config);
    ClockFunction effective_clock = clock;
    if (!effective_clock) {
      effective_clock = []() { return Clock::now(); };
    }
    impl_.reset(new circuit_breaker_detail::Impl(config, listener,
                                                  effective_clock));
  }

  CircuitBreaker(const CircuitBreaker &) = delete;
  CircuitBreaker &operator=(const CircuitBreaker &) = delete;

  CircuitPermit try_acquire() {
    std::unique_ptr<StateChange> change;
    CircuitPermit permit;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      const Clock::time_point now = impl_->clock();
      if (impl_->state == CircuitState::Open) {
        const std::chrono::milliseconds retry_after =
            circuit_breaker_detail::retry_after_locked(*impl_, now);
        if (retry_after.count() > 0) {
          throw CircuitOpenError(retry_after);
        }
        change.reset(new StateChange(circuit_breaker_detail::transition_locked(
            *impl_, CircuitState::HalfOpen,
            StateChangeReason::CooldownElapsed, now)));
      }

      if (impl_->state == CircuitState::HalfOpen) {
        if (impl_->half_open_issued >=
            impl_->config.half_open_max_probes) {
          throw CircuitOpenError(std::chrono::milliseconds(0));
        }
        ++impl_->half_open_issued;
        ++impl_->half_open_in_flight;
      }

      permit = CircuitPermit(impl_, impl_->generation, impl_->state, now);
    }
    circuit_breaker_detail::notify(impl_, change);
    return permit;
  }

  CircuitState state() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state;
  }

  CircuitBreakerSnapshot snapshot() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const Clock::time_point now = impl_->clock();
    if (impl_->state == CircuitState::Closed) {
      circuit_breaker_detail::prune_locked(*impl_, now);
    }

    std::size_t requests = 0;
    std::size_t failures = 0;
    std::size_t slow_calls = 0;
    circuit_breaker_detail::counts_locked(*impl_, requests, failures,
                                           slow_calls);

    CircuitBreakerSnapshot result;
    result.state = impl_->state;
    result.generation = impl_->generation;
    result.window_requests = requests;
    result.window_failures = failures;
    result.window_slow_calls = slow_calls;
    result.failure_rate = requests == 0
                              ? 0.0
                              : static_cast<double>(failures) /
                                    static_cast<double>(requests);
    result.slow_call_rate = requests == 0
                                ? 0.0
                                : static_cast<double>(slow_calls) /
                                      static_cast<double>(requests);
    result.half_open_issued = impl_->half_open_issued;
    result.half_open_in_flight = impl_->half_open_in_flight;
    result.half_open_successes = impl_->half_open_successes;
    result.retry_after =
        circuit_breaker_detail::retry_after_locked(*impl_, now);
    return result;
  }

private:
  std::shared_ptr<circuit_breaker_detail::Impl> impl_;
};

namespace circuit_breaker_detail {

template <typename Result> struct Invocation {
  template <typename Operation>
  static Result run(CircuitPermit permit, Operation operation,
                    const ExceptionClassifier &classifier) {
    try {
      Result result = operation();
      permit.record_success();
      return result;
    } catch (...) {
      const std::exception_ptr error = std::current_exception();
      bool failure = true;
      try {
        failure = classify_exception(classifier, error);
      } catch (...) {
        permit.record_failure();
        throw;
      }
      if (failure) {
        permit.record_failure();
      } else {
        permit.record_ignored();
      }
      std::rethrow_exception(error);
    }
  }
};

template <> struct Invocation<void> {
  template <typename Operation>
  static void run(CircuitPermit permit, Operation operation,
                  const ExceptionClassifier &classifier) {
    try {
      operation();
      permit.record_success();
    } catch (...) {
      const std::exception_ptr error = std::current_exception();
      bool failure = true;
      try {
        failure = classify_exception(classifier, error);
      } catch (...) {
        permit.record_failure();
        throw;
      }
      if (failure) {
        permit.record_failure();
      } else {
        permit.record_ignored();
      }
      std::rethrow_exception(error);
    }
  }
};

template <typename Result> struct FallbackInvocation {
  template <typename Fallback>
  static Result run(Fallback fallback, const CircuitOpenError &error) {
    return fallback(error);
  }
};

template <> struct FallbackInvocation<void> {
  template <typename Fallback>
  static void run(Fallback fallback, const CircuitOpenError &error) {
    fallback(error);
  }
};

} // namespace circuit_breaker_detail

template <typename Operation>
decltype(std::declval<Operation>()())
execute(CircuitBreaker &breaker, Operation operation,
        const ExceptionClassifier &classifier = ExceptionClassifier()) {
  typedef decltype(std::declval<Operation>()()) Result;
  CircuitPermit permit = breaker.try_acquire();
  return circuit_breaker_detail::Invocation<Result>::run(
      std::move(permit), operation, classifier);
}

template <typename Operation, typename Fallback>
decltype(std::declval<Operation>()()) execute_or_fallback(
    CircuitBreaker &breaker, Operation operation, Fallback fallback,
    const ExceptionClassifier &classifier = ExceptionClassifier()) {
  typedef decltype(std::declval<Operation>()()) Result;
  CircuitPermit permit;
  try {
    permit = breaker.try_acquire();
  } catch (const CircuitOpenError &error) {
    return circuit_breaker_detail::FallbackInvocation<Result>::run(fallback,
                                                                    error);
  }
  return circuit_breaker_detail::Invocation<Result>::run(
      std::move(permit), operation, classifier);
}

} // namespace mental1104

#endif // MENTAL1104_CONCURRENCY_CIRCUIT_BREAKER_H
