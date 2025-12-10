// include/mental1104/concurrency/coroutine/async_simple_scheduler.h
#ifndef MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H
#define MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H

#pragma once

#if __cplusplus >= 202002L

#if defined(M1104_HAS_ASYNC_SIMPLE)

#include "async_simple/Executor.h"
#include <condition_variable>
#include <memory>
#include <mutex>

#include "mental1104/concurrency/coroutine/coroutine_scheduler.h"

namespace mental1104 {

// 使用 async_simple::Executor 适配 ICoroutineScheduler
class AsyncSimpleCoroutineScheduler : public ICoroutineScheduler {
public:
  explicit AsyncSimpleCoroutineScheduler(
      std::shared_ptr<async_simple::Executor> exec)
      : exec_(std::move(exec)), pending_(0) {
    if (!exec_) {
      throw std::invalid_argument(
          "AsyncSimpleCoroutineScheduler requires a valid executor");
    }
  }

  void spawn_task(Task t) override {
    if (!t)
      return;
    auto holder = std::make_shared<Task>(std::move(t));
    pending_.fetch_add(1, std::memory_order_relaxed);
    schedule_resume(std::move(holder));
  }

  void wait_all() override {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk,
             [this] { return pending_.load(std::memory_order_acquire) == 0; });
  }

private:
  void schedule_resume(std::shared_ptr<Task> task) {
    exec_->schedule([this, task = std::move(task)]() mutable {
      task->resume();
      if (task->done()) {
        pending_.fetch_sub(1, std::memory_order_acq_rel);
        cv_.notify_all();
      } else {
        schedule_resume(std::move(task));
      }
    });
  }

private:
  std::shared_ptr<async_simple::Executor> exec_;
  std::atomic<int> pending_;
  std::mutex mu_;
  std::condition_variable cv_;
};

} // namespace mental1104

#endif // async_simple available

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H
