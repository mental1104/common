// include/mental1104/concurrency/coroutine/async_simple_scheduler.h
#ifndef MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H
#define MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H

#pragma once

#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX20

#if defined(M1104_HAS_ASYNC_SIMPLE)

#include "async_simple/Executor.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>

#include "mental1104/concurrency/coroutine/coroutine_scheduler.h"

namespace mental1104 {

// 使用 async_simple::Executor 适配 ICoroutineScheduler
/// 通过 async_simple 执行器恢复任务，并维护待完成计数与等待条件。
class AsyncSimpleCoroutineScheduler : public ICoroutineScheduler {
public:
  // 使用 shared_ptr 主要是共享所有权并保证调度期间 executor
  // 存活；若调用方能保证生命周期，也可改为引用/裸指针。
  /// 共享执行器所有权，拒绝空执行器。
  /// @param exec 非空执行器，调度器保存共享所有权。
  explicit AsyncSimpleCoroutineScheduler(
      std::shared_ptr<async_simple::Executor> exec);

  /// 接管任务并提交首次恢复，空任务直接忽略。
  /// @param t 转移帧所有权的任务。
  void spawn_task(Task t) override;

  /// 等待 pending 归零；调用方须在销毁调度器前完成等待。
  void wait_all() override;

private:
  /// 在执行器中推进任务，完成或提交失败时归还 pending 计数。
  /// @param task 跨异步回调共享所有权的任务帧。
  void schedule_resume(std::shared_ptr<Task> task);

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
