#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace mental1104 {

class CancellationToken {
public:
  CancellationToken() : cancelled_(false) {}

  void cancel() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      cancelled_ = true;
    }
    condition_.notify_all();
  }

  bool is_cancelled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancelled_;
  }

  template <typename Rep, typename Period>
  bool wait_for(const std::chrono::duration<Rep, Period> &duration) const {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, duration, [this] { return cancelled_; });
  }

private:
  CancellationToken(const CancellationToken &);
  CancellationToken &operator=(const CancellationToken &);

  mutable std::mutex mutex_;
  mutable std::condition_variable condition_;
  bool cancelled_;
};

class AcquireCancelledError : public std::runtime_error {
public:
  AcquireCancelledError() : std::runtime_error("token acquisition cancelled") {}
};

class TokenBucket {
public:
  TokenBucket(double rate, std::size_t capacity)
      : rate_(rate), capacity_(static_cast<double>(capacity)),
        tokens_(static_cast<double>(capacity)), last_(clock_type::now()) {
    if (!std::isfinite(rate_) || rate_ <= 0.0) {
      throw std::invalid_argument("rate must be a finite positive number");
    }
    if (capacity == 0U) {
      throw std::invalid_argument("capacity must be positive");
    }
  }

  bool acquire(const CancellationToken *cancellation = NULL) {
    for (;;) {
      if (cancellation != NULL && cancellation->is_cancelled()) {
        return false;
      }

      double wait_seconds = 0.0;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        const clock_type::time_point now = clock_type::now();
        const std::chrono::duration<double> elapsed = now - last_;
        tokens_ = std::min(capacity_, tokens_ + elapsed.count() * rate_);
        last_ = now;

        if (tokens_ >= 1.0) {
          tokens_ -= 1.0;
          return true;
        }

        wait_seconds = (1.0 - tokens_) / rate_;
      }

      const std::chrono::duration<double> wait(wait_seconds);
      if (cancellation != NULL) {
        if (cancellation->wait_for(wait)) {
          return false;
        }
      } else {
        std::this_thread::sleep_for(wait);
      }
    }
  }

  void release() noexcept {}

private:
  typedef std::chrono::steady_clock clock_type;

  TokenBucket(const TokenBucket &);
  TokenBucket &operator=(const TokenBucket &);

  std::mutex mutex_;
  double rate_;
  double capacity_;
  double tokens_;
  clock_type::time_point last_;
};

namespace detail {

template <typename Limiter> class LimiterReleaseGuard {
public:
  explicit LimiterReleaseGuard(Limiter &limiter) : limiter_(limiter) {}

  ~LimiterReleaseGuard() { limiter_.release(); }

private:
  LimiterReleaseGuard(const LimiterReleaseGuard &);
  LimiterReleaseGuard &operator=(const LimiterReleaseGuard &);

  Limiter &limiter_;
};

} // namespace detail

template <typename Limiter, typename Callable> class RateLimitedCallable {
public:
  template <typename CallableArg>
  RateLimitedCallable(Limiter &limiter, CallableArg &&callable,
                      const CancellationToken *cancellation)
      : limiter_(&limiter), callable_(std::forward<CallableArg>(callable)),
        cancellation_(cancellation) {}

  template <typename... Args>
  auto operator()(Args &&...args)
      -> decltype(std::declval<Callable &>()(std::forward<Args>(args)...)) {
    if (!limiter_->acquire(cancellation_)) {
      throw AcquireCancelledError();
    }

    detail::LimiterReleaseGuard<Limiter> guard(*limiter_);
    return callable_(std::forward<Args>(args)...);
  }

private:
  Limiter *limiter_;
  Callable callable_;
  const CancellationToken *cancellation_;
};

template <typename Limiter, typename Callable>
RateLimitedCallable<Limiter, typename std::decay<Callable>::type>
rate_limited(Limiter &limiter, Callable &&callable,
             const CancellationToken *cancellation = NULL) {
  typedef typename std::decay<Callable>::type callable_type;
  return RateLimitedCallable<Limiter, callable_type>(
      limiter, std::forward<Callable>(callable), cancellation);
}

} // namespace mental1104
