#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include "mental1104/redis_singleflight.h"

namespace {

struct FakeLockState {
  FakeLockState() : acquired(false), try_calls(0), unlock_calls(0) {}

  bool acquired;
  std::atomic<int> try_calls;
  std::atomic<int> unlock_calls;
};

class FakeLock : public mental1104::SingleFlightLock {
public:
  explicit FakeLock(const std::shared_ptr<FakeLockState> &state)
      : state_(state) {}

  bool try_lock(std::chrono::milliseconds) override {
    ++this->state_->try_calls;
    return this->state_->acquired;
  }

  void unlock() override { ++this->state_->unlock_calls; }

private:
  std::shared_ptr<FakeLockState> state_;
};

mental1104::RedisSingleFlight<std::string>::LockFactory
make_lock_factory(const std::shared_ptr<FakeLockState> &state,
                  std::string *observed_key = NULL) {
  return [state, observed_key](const std::string &key)
             -> std::unique_ptr<mental1104::SingleFlightLock> {
    if (observed_key != NULL) {
      *observed_key = key;
    }
    return std::unique_ptr<mental1104::SingleFlightLock>(new FakeLock(state));
  };
}

} // namespace

TEST(RedisSingleFlightTest, ReturnsInitialCacheHitWithoutLockOrLoader) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  bool loader_called = false;
  bool cache_set_called = false;

  mental1104::RedisSingleFlight<std::string> singleflight(
      [](const std::string &) {
        return mental1104::CacheLookup<std::string>::hit("cached");
      },
      [&](const std::string &, const std::string &,
          std::chrono::milliseconds) { cache_set_called = true; },
      make_lock_factory(state));

  mental1104::RedisSingleFlightResult<std::string> result =
      singleflight.get_or_load("product:123", [&]() {
        loader_called = true;
        return std::string("loaded");
      });

  EXPECT_EQ(result.value, "cached");
  EXPECT_FALSE(result.shared);
  EXPECT_FALSE(result.stale);
  EXPECT_FALSE(loader_called);
  EXPECT_FALSE(cache_set_called);
  EXPECT_EQ(state->try_calls.load(), 0);
}

TEST(RedisSingleFlightTest, LockOwnerDoubleChecksLoadsAndWritesCache) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  state->acquired = true;
  std::map<std::string, std::string> cache;
  std::mutex cache_mutex;
  int reads = 0;
  int writes = 0;
  std::chrono::milliseconds written_ttl(0);
  std::string observed_lock_key;
  mental1104::RedisSingleFlightOptions options;

  mental1104::RedisSingleFlight<std::string> singleflight(
      [&](const std::string &key) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        ++reads;
        std::map<std::string, std::string>::const_iterator found =
            cache.find(key);
        if (found == cache.end()) {
          return mental1104::CacheLookup<std::string>::miss();
        }
        return mental1104::CacheLookup<std::string>::hit(found->second);
      },
      [&](const std::string &key, const std::string &value,
          std::chrono::milliseconds ttl) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        ++writes;
        written_ttl = ttl;
        cache[key] = value;
      },
      make_lock_factory(state, &observed_lock_key),
      mental1104::RedisSingleFlight<std::string>::CacheGet(), options);

  mental1104::RedisSingleFlightResult<std::string> result =
      singleflight.get_or_load("product:123",
                               []() { return std::string("rebuilt"); });

  EXPECT_EQ(result.value, "rebuilt");
  EXPECT_EQ(reads, 3);
  EXPECT_EQ(writes, 1);
  EXPECT_EQ(written_ttl, options.cache_ttl);
  EXPECT_EQ(observed_lock_key, options.lock_prefix + "product:123");
  EXPECT_EQ(state->try_calls.load(), 1);
  EXPECT_EQ(state->unlock_calls.load(), 1);
}

TEST(RedisSingleFlightTest, NonOwnerPollsUntilRebuiltValueAppears) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  std::atomic<int> reads(0);
  bool loader_called = false;
  bool cache_set_called = false;
  mental1104::RedisSingleFlightOptions options;
  options.wait_timeout = std::chrono::milliseconds(100);
  options.poll_min = std::chrono::milliseconds(1);
  options.poll_max = std::chrono::milliseconds(1);

  mental1104::RedisSingleFlight<std::string> singleflight(
      [&](const std::string &) {
        if (++reads >= 3) {
          return mental1104::CacheLookup<std::string>::hit("rebuilt");
        }
        return mental1104::CacheLookup<std::string>::miss();
      },
      [&](const std::string &, const std::string &,
          std::chrono::milliseconds) { cache_set_called = true; },
      make_lock_factory(state),
      mental1104::RedisSingleFlight<std::string>::CacheGet(), options,
      [](std::chrono::milliseconds) {},
      [](std::chrono::milliseconds minimum, std::chrono::milliseconds) {
        return minimum;
      });

  mental1104::RedisSingleFlightResult<std::string> result =
      singleflight.get_or_load("product:123", [&]() {
        loader_called = true;
        return std::string("loaded");
      });

  EXPECT_EQ(result.value, "rebuilt");
  EXPECT_FALSE(loader_called);
  EXPECT_FALSE(cache_set_called);
  EXPECT_EQ(state->try_calls.load(), 1);
  EXPECT_EQ(state->unlock_calls.load(), 0);
}

TEST(RedisSingleFlightTest, ReturnsStaleValueAfterWaitTimeout) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  mental1104::RedisSingleFlightOptions options;
  options.wait_timeout = std::chrono::milliseconds(0);

  mental1104::RedisSingleFlight<std::string> singleflight(
      [](const std::string &) {
        return mental1104::CacheLookup<std::string>::miss();
      },
      [](const std::string &, const std::string &,
         std::chrono::milliseconds) {},
      make_lock_factory(state),
      [](const std::string &) {
        return mental1104::CacheLookup<std::string>::hit("stale");
      },
      options);

  mental1104::RedisSingleFlightResult<std::string> result =
      singleflight.get_or_load("product:123",
                               []() { return std::string("unused"); });

  EXPECT_EQ(result.value, "stale");
  EXPECT_TRUE(result.stale);
}

TEST(RedisSingleFlightTest, ThrowsStableTimeoutWithoutStaleValue) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  mental1104::RedisSingleFlightOptions options;
  options.wait_timeout = std::chrono::milliseconds(0);

  mental1104::RedisSingleFlight<std::string> singleflight(
      [](const std::string &) {
        return mental1104::CacheLookup<std::string>::miss();
      },
      [](const std::string &, const std::string &,
         std::chrono::milliseconds) {},
      make_lock_factory(state),
      mental1104::RedisSingleFlight<std::string>::CacheGet(), options);

  EXPECT_THROW(singleflight.get_or_load(
                   "product:123", []() { return std::string("unused"); }),
               mental1104::RebuildTimeoutError);
}

TEST(RedisSingleFlightTest, UnlocksWhenOwnerLoaderThrows) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  state->acquired = true;

  mental1104::RedisSingleFlight<std::string> singleflight(
      [](const std::string &) {
        return mental1104::CacheLookup<std::string>::miss();
      },
      [](const std::string &, const std::string &,
         std::chrono::milliseconds) {},
      make_lock_factory(state));

  EXPECT_THROW(singleflight.get_or_load("product:123", []() -> std::string {
                 throw std::runtime_error("boom");
               }),
               std::runtime_error);
  EXPECT_EQ(state->unlock_calls.load(), 1);
}

TEST(RedisSingleFlightTest, ValidatesOptions) {
  std::shared_ptr<FakeLockState> state = std::make_shared<FakeLockState>();
  mental1104::RedisSingleFlightOptions options;
  options.poll_max = std::chrono::milliseconds(0);

  EXPECT_THROW(
      mental1104::RedisSingleFlight<std::string>(
          [](const std::string &) {
            return mental1104::CacheLookup<std::string>::miss();
          },
          [](const std::string &, const std::string &,
             std::chrono::milliseconds) {},
          make_lock_factory(state),
          mental1104::RedisSingleFlight<std::string>::CacheGet(), options),
      std::invalid_argument);
}
