// include/mental1104/concurrency/coroutine/coroutine_scheduler.h
#ifndef MENTAL1104_COROUTINE_SCHEDULER_H
#define MENTAL1104_COROUTINE_SCHEDULER_H

#pragma once

#if __cplusplus >= 202002L

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>

#include "mental1104/concurrency/executor.h"
#include "mental1104/concurrency/coroutine/task.h"
#include "mental1104/concurrency/thread/thread_util.h"  // 为 sleep_for_ms 等

namespace mental1104 {

// 若 async_simple 可用，则提供一个可查询的标记
#if defined(M1104_HAS_ASYNC_SIMPLE)
constexpr bool kHasAsyncSimple = true;
#else
constexpr bool kHasAsyncSimple = false;
#endif

// 协程调度器抽象接口：为将来接入阿里协程池等预留
class ICoroutineScheduler {
public:
  virtual ~ICoroutineScheduler() = default;

  virtual void spawn_task(Task t) = 0;
  virtual void wait_all() = 0;
};

// 基于 IExecutor 的默认 m→n 协程调度器实现
class BasicCoroutineScheduler : public ICoroutineScheduler {
public:
  explicit BasicCoroutineScheduler(IExecutor &executor,
                                   std::size_t scheduler_workers = 1)
      : executor_(executor),
        stopping_(false),
        pending_(0) {
    start(scheduler_workers);
  }

  ~BasicCoroutineScheduler() override {
    stop();
  }

  void spawn_task(Task t) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ready_.push(std::move(t));
      ++pending_;
    }
  }

  void wait_all() override {
    using namespace std::chrono_literals;
    while (pending_.load(std::memory_order_acquire) > 0) {
      std::this_thread::sleep_for(1ms);
    }
  }

  void stop() {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true)) {
      return; // 已经停过
    }
  }

private:
  void start(std::size_t workers) {
    if (workers == 0) workers = 1;
    for (std::size_t i = 0; i < workers; ++i) {
      executor_.execute([this] { scheduler_loop(); });
    }
  }

  void scheduler_loop() {
    using namespace std::chrono_literals;

    while (!stopping_.load(std::memory_order_acquire)) {
      Task t;

      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_.empty()) {
          t = std::move(ready_.front());
          ready_.pop();
        }
      }

      if (!t) {
        std::this_thread::sleep_for(1ms);
        continue;
      }

      t.resume();

      if (t.done()) {
        pending_.fetch_sub(1, std::memory_order_acq_rel);
      } else {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_.push(std::move(t));
      }
    }
  }

private:
  IExecutor &executor_;
  std::queue<Task> ready_;
  std::mutex mutex_;
  std::atomic<bool> stopping_;
  std::atomic<int> pending_;
};

} // namespace mental1104

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_COROUTINE_SCHEDULER_H
