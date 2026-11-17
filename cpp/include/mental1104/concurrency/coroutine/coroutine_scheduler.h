// include/mental1104/concurrency/coroutine/coroutine_scheduler.h
#ifndef MENTAL1104_COROUTINE_SCHEDULER_H
#define MENTAL1104_COROUTINE_SCHEDULER_H

#pragma once

#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX20

#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>

#include "mental1104/concurrency/coroutine/task.h"
#include "mental1104/concurrency/executor.h"
#include "mental1104/concurrency/thread/thread_util.h" // 为 sleep_for_ms 等

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
/// 维护就绪任务队列和完成计数的基础调度器，借用 IExecutor。
class BasicCoroutineScheduler : public ICoroutineScheduler {
public:
  /// 借用执行器并提交长期运行的调度循环。
  /// @param executor 必须活到调度工作退出，调用方负责其生命周期。
  /// @param scheduler_workers 调度循环数量，0 按 1 处理。
  explicit BasicCoroutineScheduler(IExecutor &executor,
                                   std::size_t scheduler_workers = 1);

  /// 请求调度循环停止；沿用执行器负责工作线程回收的现有约定。
  ~BasicCoroutineScheduler() override;

  /// 接管任务帧并加入受锁保护的就绪队列。
  /// @param t 转移所有权的协程任务。
  void spawn_task(Task t) override;

  /// 阻塞到 pending 归零；不主动停止调度循环。
  void wait_all() override;

  /// 幂等设置停止状态，供调度循环观察。
  void stop();

private:
  /// 向借用的执行器提交调度循环。
  /// @param workers 循环数量，0 按 1 处理。
  void start(std::size_t workers);

  /// 在执行器线程中推进任务，完成时减少 pending，挂起时重新入队。
  void scheduler_loop();

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
