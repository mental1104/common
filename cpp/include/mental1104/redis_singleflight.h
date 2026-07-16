#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

#include "mental1104/concurrency/singleflight.h"
#include "mental1104/redis_lock.h"

namespace mental1104 {

template <typename Value> class CacheLookup {
public:
  static CacheLookup hit(const Value &value) {
    return CacheLookup(true, std::make_shared<Value>(value));
  }

  static CacheLookup miss() {
    return CacheLookup(false, std::shared_ptr<Value>());
  }

  bool found() const { return this->found_; }

  const Value &value() const {
    if (!this->found_ || !this->value_) {
      throw std::logic_error("singleflight: cache lookup is a miss");
    }
    return *this->value_;
  }

private:
  CacheLookup(bool found, const std::shared_ptr<Value> &value)
      : found_(found), value_(value) {}

  bool found_;
  std::shared_ptr<Value> value_;
};

class RebuildTimeoutError : public std::runtime_error {
public:
  explicit RebuildTimeoutError(const std::string &key)
      : std::runtime_error("singleflight: cache rebuild timed out for key: " +
                           key) {}
};

struct RedisSingleFlightOptions {
  RedisSingleFlightOptions()
      : lock_ttl(std::chrono::milliseconds(3000)),
        cache_ttl(std::chrono::milliseconds(600000)),
        wait_timeout(std::chrono::milliseconds(500)),
        poll_min(std::chrono::milliseconds(20)),
        poll_max(std::chrono::milliseconds(50)),
        lock_prefix("singleflight:lock:") {}

  std::chrono::milliseconds lock_ttl;
  std::chrono::milliseconds cache_ttl;
  std::chrono::milliseconds wait_timeout;
  std::chrono::milliseconds poll_min;
  std::chrono::milliseconds poll_max;
  std::string lock_prefix;
};

class SingleFlightLock {
public:
  virtual ~SingleFlightLock() {}
  virtual bool try_lock(std::chrono::milliseconds ttl) = 0;
  virtual void unlock() = 0;
};

class RedisSingleFlightLock : public SingleFlightLock {
public:
  RedisSingleFlightLock(const std::shared_ptr<sw::redis::Redis> &redis,
                        const std::string &key)
      : lock_(redis, key) {}

  bool try_lock(std::chrono::milliseconds ttl) override {
    return this->lock_.try_lock(ttl);
  }

  void unlock() override { this->lock_.unlock(); }

private:
  ::RedisLock lock_;
};

template <typename Value> struct RedisSingleFlightResult {
  Value value;
  bool shared;
  bool stale;
};

template <typename Value> class RedisSingleFlight {
public:
  typedef std::function<CacheLookup<Value>(const std::string &)> CacheGet;
  typedef std::function<void(const std::string &, const Value &,
                             std::chrono::milliseconds)>
      CacheSet;
  typedef std::function<Value()> Loader;
  typedef std::function<std::unique_ptr<SingleFlightLock>(
      const std::string &)>
      LockFactory;
  typedef std::function<void(std::chrono::milliseconds)> SleepFunction;
  typedef std::function<std::chrono::milliseconds(
      std::chrono::milliseconds, std::chrono::milliseconds)>
      JitterFunction;

  RedisSingleFlight(const std::shared_ptr<sw::redis::Redis> &redis,
                    const CacheGet &cache_get, const CacheSet &cache_set,
                    const CacheGet &stale_get = CacheGet(),
                    const RedisSingleFlightOptions &options =
                        RedisSingleFlightOptions())
      : cache_get_(cache_get), cache_set_(cache_set), stale_get_(stale_get),
        options_(options),
        lock_factory_([redis](const std::string &key)
                          -> std::unique_ptr<SingleFlightLock> {
          return std::unique_ptr<SingleFlightLock>(
              new RedisSingleFlightLock(redis, key));
        }),
        sleep_function_(&RedisSingleFlight<Value>::default_sleep),
        jitter_function_(&RedisSingleFlight<Value>::default_jitter) {
    if (!redis) {
      throw std::invalid_argument("singleflight: Redis client must not be null");
    }
    this->validate();
  }

  RedisSingleFlight(const CacheGet &cache_get, const CacheSet &cache_set,
                    const LockFactory &lock_factory,
                    const CacheGet &stale_get = CacheGet(),
                    const RedisSingleFlightOptions &options =
                        RedisSingleFlightOptions(),
                    const SleepFunction &sleep_function = SleepFunction(),
                    const JitterFunction &jitter_function = JitterFunction())
      : cache_get_(cache_get), cache_set_(cache_set), stale_get_(stale_get),
        options_(options), lock_factory_(lock_factory),
        sleep_function_(sleep_function
                            ? sleep_function
                            : SleepFunction(
                                  &RedisSingleFlight<Value>::default_sleep)),
        jitter_function_(
            jitter_function
                ? jitter_function
                : JitterFunction(&RedisSingleFlight<Value>::default_jitter)) {
    this->validate();
  }

  RedisSingleFlightResult<Value> get_or_load(const std::string &key,
                                              const Loader &loader) {
    if (key.empty()) {
      throw std::invalid_argument("singleflight: key must not be empty");
    }
    if (!loader) {
      throw std::invalid_argument("singleflight: loader must not be empty");
    }

    CacheLookup<Value> cached = this->cache_get_(key);
    if (cached.found()) {
      return RedisSingleFlightResult<Value>{cached.value(), false, false};
    }

    SingleFlightResult<CoordinatedValue> local_result =
        this->local_.do_call(key, [this, key, loader]() -> CoordinatedValue {
          return this->coordinate(key, loader);
        });

    return RedisSingleFlightResult<Value>{local_result.value.value,
                                          local_result.shared,
                                          local_result.value.stale};
  }

private:
  struct CoordinatedValue {
    CoordinatedValue(const Value &input_value, bool input_stale)
        : value(input_value), stale(input_stale) {}

    Value value;
    bool stale;
  };

  class UnlockGuard {
  public:
    explicit UnlockGuard(SingleFlightLock *lock) : lock_(lock) {}

    ~UnlockGuard() {
      if (this->lock_ != NULL) {
        try {
          this->lock_->unlock();
        } catch (...) {
        }
      }
    }

  private:
    SingleFlightLock *lock_;
  };

  CoordinatedValue coordinate(const std::string &key, const Loader &loader) {
    CacheLookup<Value> cached = this->cache_get_(key);
    if (cached.found()) {
      return CoordinatedValue(cached.value(), false);
    }

    std::unique_ptr<SingleFlightLock> lock =
        this->lock_factory_(this->options_.lock_prefix + key);
    if (!lock) {
      throw std::runtime_error("singleflight: lock factory returned null");
    }

    if (lock->try_lock(this->options_.lock_ttl)) {
      UnlockGuard unlock_guard(lock.get());

      cached = this->cache_get_(key);
      if (cached.found()) {
        return CoordinatedValue(cached.value(), false);
      }

      Value value = loader();
      this->cache_set_(key, value, this->options_.cache_ttl);
      return CoordinatedValue(value, false);
    }

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + this->options_.wait_timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      std::chrono::milliseconds remaining =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              deadline - std::chrono::steady_clock::now());
      if (remaining.count() <= 0) {
        break;
      }

      std::chrono::milliseconds delay = this->jitter_function_(
          this->options_.poll_min, this->options_.poll_max);
      this->sleep_function_(std::min(delay, remaining));

      cached = this->cache_get_(key);
      if (cached.found()) {
        return CoordinatedValue(cached.value(), false);
      }
    }

    if (this->stale_get_) {
      CacheLookup<Value> stale = this->stale_get_(key);
      if (stale.found()) {
        return CoordinatedValue(stale.value(), true);
      }
    }

    throw RebuildTimeoutError(key);
  }

  void validate() const {
    if (!this->cache_get_) {
      throw std::invalid_argument(
          "singleflight: cache get callback must not be empty");
    }
    if (!this->cache_set_) {
      throw std::invalid_argument(
          "singleflight: cache set callback must not be empty");
    }
    if (!this->lock_factory_) {
      throw std::invalid_argument(
          "singleflight: lock factory must not be empty");
    }
    if (this->options_.lock_ttl.count() <= 0) {
      throw std::invalid_argument("singleflight: lock ttl must be positive");
    }
    if (this->options_.cache_ttl.count() <= 0) {
      throw std::invalid_argument("singleflight: cache ttl must be positive");
    }
    if (this->options_.wait_timeout.count() < 0) {
      throw std::invalid_argument(
          "singleflight: wait timeout must not be negative");
    }
    if (this->options_.poll_min.count() <= 0) {
      throw std::invalid_argument(
          "singleflight: poll minimum must be positive");
    }
    if (this->options_.poll_max < this->options_.poll_min) {
      throw std::invalid_argument(
          "singleflight: poll maximum must not be less than poll minimum");
    }
    if (this->options_.lock_prefix.empty()) {
      throw std::invalid_argument(
          "singleflight: lock prefix must not be empty");
    }
  }

  static void default_sleep(std::chrono::milliseconds delay) {
    std::this_thread::sleep_for(delay);
  }

  static std::chrono::milliseconds
  default_jitter(std::chrono::milliseconds minimum,
                 std::chrono::milliseconds maximum) {
    static thread_local std::mt19937 generator(
        static_cast<unsigned int>(std::random_device()()));
    std::uniform_int_distribution<long long> distribution(minimum.count(),
                                                           maximum.count());
    return std::chrono::milliseconds(distribution(generator));
  }

  SingleFlightGroup<std::string, CoordinatedValue> local_;
  CacheGet cache_get_;
  CacheSet cache_set_;
  CacheGet stale_get_;
  RedisSingleFlightOptions options_;
  LockFactory lock_factory_;
  SleepFunction sleep_function_;
  JitterFunction jitter_function_;
};

} // namespace mental1104
