// include/mental1104/concurrency/mn/mn_coroutine_pool.h
#ifndef MENTAL1104_MN_COROUTINE_POOL_H
#define MENTAL1104_MN_COROUTINE_POOL_H

#pragma once

#if __cplusplus >= 202002L

#include <memory>
#include <type_traits>

#if defined(M1104_HAS_ASYNC_SIMPLE)
#include "async_simple/Executor.h"
#endif

#include "mental1104/concurrency/coroutine/async_simple_scheduler.h"
#include "mental1104/concurrency/coroutine/coroutine_scheduler.h"
#include "mental1104/concurrency/executor.h"
#include "mental1104/concurrency/thread/boost_asio_executor.h"
#include "mental1104/concurrency/thread/thread_pool_executor.h"
#include "mental1104/concurrency/thread/thread_util.h"

namespace mental1104 {

namespace detail {

// 统一的调度器创建器：BasicCoroutineScheduler 需要
// IExecutor&，AsyncSimpleCoroutineScheduler 需要 async_simple::Executor
template <class Scheduler, class ExecutorAdapter>
Scheduler make_scheduler(ExecutorAdapter &exec,
                         const std::shared_ptr<ExecutorAdapter> &exec_ptr,
                         std::size_t thread_count,
                         std::size_t scheduler_workers) {
  using ExecRef = ExecutorAdapter &;

  if constexpr (std::is_constructible_v<Scheduler, ExecRef, std::size_t>) {
    const std::size_t workers =
        scheduler_workers == 0 ? thread_count : scheduler_workers;
    return Scheduler(exec, workers);
  } else if constexpr (std::is_constructible_v<Scheduler, ExecRef>) {
    return Scheduler(exec);
  }
#if defined(M1104_HAS_ASYNC_SIMPLE)
  else if constexpr (std::is_constructible_v<
                         Scheduler, std::shared_ptr<async_simple::Executor>>) {
    static_assert(std::is_base_of_v<async_simple::Executor, ExecutorAdapter>,
                  "ExecutorAdapter must derive async_simple::Executor for this "
                  "scheduler");
    return Scheduler(
        std::static_pointer_cast<async_simple::Executor>(exec_ptr));
  }
#endif
  else {
    static_assert(sizeof(Scheduler) == 0,
                  "Unsupported scheduler constructor for MnCoroutinePoolT");
  }
}

} // namespace detail

// ExecutorAdapter: 把底层线程池封装到内部、实现 IExecutor（以及可选的
// async_simple::Executor） Scheduler: 协程调度器类型，需实现
// ICoroutineScheduler 接口
template <class ExecutorAdapter, class Scheduler = BasicCoroutineScheduler>
class MnCoroutinePoolT {
public:
  explicit MnCoroutinePoolT(std::size_t thread_count,
                            std::size_t scheduler_workers = 0)
      : executor_(std::make_shared<ExecutorAdapter>(thread_count)),
        scheduler_(detail::make_scheduler<Scheduler>(
            *executor_, executor_, thread_count, scheduler_workers)) {
    static_assert(std::is_base_of_v<IExecutor, ExecutorAdapter>,
                  "ExecutorAdapter must derive from IExecutor");
    static_assert(std::is_base_of_v<ICoroutineScheduler, Scheduler>,
                  "Scheduler must implement ICoroutineScheduler");
  }

  void spawn(Task t) { scheduler_.spawn_task(std::move(t)); }

  void wait_all() { scheduler_.wait_all(); }

  Scheduler &scheduler() { return scheduler_; }
  ExecutorAdapter &executor() { return *executor_; }
  std::shared_ptr<ExecutorAdapter> executor_ptr() { return executor_; }

private:
  std::shared_ptr<ExecutorAdapter> executor_; // 封装底层线程池的执行器
  Scheduler scheduler_;                       // 调度 m 个协程
};

// 默认实现：自研 ThreadPool + Basic 调度
using MnCoroutinePool =
    MnCoroutinePoolT<ThreadPoolExecutor, BasicCoroutineScheduler>;

#if defined(M1104_HAS_ASYNC_SIMPLE)
// 自研 ThreadPool + async_simple 调度
using MnCoroutinePoolAsyncSimple =
    MnCoroutinePoolT<ThreadPoolExecutor, AsyncSimpleCoroutineScheduler>;
#endif

// Boost.Asio 线程池 + Basic 调度
using BoostMnCoroutinePool =
    MnCoroutinePoolT<BoostAsioExecutor, BasicCoroutineScheduler>;

#if defined(M1104_HAS_ASYNC_SIMPLE)
// Boost.Asio 线程池 + async_simple 调度
using BoostMnCoroutinePoolAsyncSimple =
    MnCoroutinePoolT<BoostAsioExecutor, AsyncSimpleCoroutineScheduler>;
#endif

} // namespace mental1104

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_MN_COROUTINE_POOL_H
