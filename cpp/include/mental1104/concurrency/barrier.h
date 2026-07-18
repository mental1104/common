#ifndef MENTAL1104_CONCURRENCY_BARRIER_H
#define MENTAL1104_CONCURRENCY_BARRIER_H

#include "mental1104/meta/barrier_support.h"

#include <cstddef>
#include <utility>

#if !M1104_HAS_STD_BARRIER
#include <cassert>
#include <condition_variable>
#include <limits>
#include <mutex>
#endif

namespace mental1104 {
namespace detail {

struct empty_barrier_completion {
  void operator()() noexcept {}
};

} // namespace detail

#if M1104_HAS_STD_BARRIER

// Prefer the standard-library implementation whenever the compiler and its
// standard library expose a complete C++20 barrier implementation.
template <class CompletionFunction = detail::empty_barrier_completion>
using barrier = std::barrier<CompletionFunction>;

#else

#if M1104_HAS_CXX17
#define M1104_BARRIER_NODISCARD [[nodiscard]]
#else
#define M1104_BARRIER_NODISCARD
#endif

// C++11-compatible reusable barrier with the same public operations as
// std::barrier. The standard preconditions are enforced with assertions in
// debug builds; violating them in release builds is undefined behavior, as it
// is for std::barrier.
template <class CompletionFunction = detail::empty_barrier_completion>
class barrier {
public:
  class arrival_token {
  public:
    arrival_token(arrival_token &&other) noexcept
        : owner_(other.owner_), phase_(other.phase_) {
      other.owner_ = NULL;
    }

    arrival_token &operator=(arrival_token &&other) noexcept {
      if (this != &other) {
        this->owner_ = other.owner_;
        this->phase_ = other.phase_;
        other.owner_ = NULL;
      }
      return *this;
    }

    arrival_token(const arrival_token &) = delete;
    arrival_token &operator=(const arrival_token &) = delete;

  private:
    friend class barrier;

    arrival_token(const barrier *owner, std::size_t phase) noexcept
        : owner_(owner), phase_(phase) {}

    const barrier *owner_;
    std::size_t phase_;
  };

  static constexpr std::ptrdiff_t max() noexcept {
    return (std::numeric_limits<std::ptrdiff_t>::max)();
  }

  explicit barrier(std::ptrdiff_t expected,
                   CompletionFunction completion = CompletionFunction())
      : expected_(expected), remaining_(expected), phase_(0),
        completion_(std::move(completion)) {
    static_assert(noexcept(std::declval<CompletionFunction &>()()),
                  "barrier completion must be noexcept");
    assert(expected > 0);
    assert(expected <= max());
  }

  ~barrier() {}

  barrier(const barrier &) = delete;
  barrier &operator=(const barrier &) = delete;
  barrier(barrier &&) = delete;
  barrier &operator=(barrier &&) = delete;

  M1104_BARRIER_NODISCARD arrival_token
  arrive(std::ptrdiff_t update = 1) {
    std::unique_lock<std::mutex> lock(this->mutex_);
    assert(update > 0);
    assert(update <= this->remaining_);

    const std::size_t arrival_phase = this->phase_;
    this->remaining_ -= update;
    if (this->remaining_ == 0) {
      this->complete_phase(lock);
    }

    return arrival_token(this, arrival_phase);
  }

  void wait(arrival_token &&arrival) const {
    assert(arrival.owner_ == this);

    std::unique_lock<std::mutex> lock(this->mutex_);
    const std::size_t arrival_phase = arrival.phase_;
    arrival.owner_ = NULL;

    this->condition_.wait(lock, [this, arrival_phase] {
      return this->phase_ != arrival_phase;
    });
  }

  void arrive_and_wait() {
    arrival_token arrival = this->arrive();
    this->wait(std::move(arrival));
  }

  void arrive_and_drop() {
    std::unique_lock<std::mutex> lock(this->mutex_);
    assert(this->expected_ > 0);
    assert(this->remaining_ > 0);

    --this->expected_;
    --this->remaining_;
    if (this->remaining_ == 0) {
      this->complete_phase(lock);
    }
  }

private:
  void complete_phase(std::unique_lock<std::mutex> &lock) {
    this->completion_();
    ++this->phase_;
    this->remaining_ = this->expected_;

    lock.unlock();
    this->condition_.notify_all();
  }

  mutable std::mutex mutex_;
  mutable std::condition_variable condition_;
  std::ptrdiff_t expected_;
  std::ptrdiff_t remaining_;
  std::size_t phase_;
  CompletionFunction completion_;
};

#undef M1104_BARRIER_NODISCARD

#endif

} // namespace mental1104

#endif // MENTAL1104_CONCURRENCY_BARRIER_H
