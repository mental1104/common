// include/mental1104/concurrency/mn_coroutine_pool.h
#ifndef MENTAL1104_MN_COROUTINE_POOL_H
#define MENTAL1104_MN_COROUTINE_POOL_H

#pragma once

#if __cplusplus >= 202002L

#include <type_traits>

#include "mental1104/concurrency/thread_util.h"
#include "mental1104/concurrency/thread_pool_executor.h"
#include "mental1104/concurrency/coroutine_scheduler.h"

namespace mental1104 {

// UnderlyingPool: 底层线程池类型（如 ::ThreadPool 或 boost::asio::thread_pool）
// ExecutorAdapter: 把 UnderlyingPool 适配成 IExecutor 的适配器类型
// Scheduler: 协程调度器类型，需实现 ICoroutineScheduler 接口
template <class UnderlyingPool,
          class ExecutorAdapter,
          class Scheduler = BasicCoroutineScheduler>
class MnCoroutinePoolT {
public:
  explicit MnCoroutinePoolT(std::size_t thread_count,
                            std::size_t scheduler_workers = 0)
      : pool_(thread_count),
        executor_(pool_),
        scheduler_(executor_,
                   scheduler_workers == 0 ? thread_count : scheduler_workers) {
    static_assert(std::is_base_of_v<IExecutor, ExecutorAdapter>,
                  "ExecutorAdapter must derive from IExecutor");
    static_assert(std::is_base_of_v<ICoroutineScheduler, Scheduler>,
                  "Scheduler must implement ICoroutineScheduler");
  }

  void spawn(Task t) {
    scheduler_.spawn_task(std::move(t));
  }

  void wait_all() {
    scheduler_.wait_all();
  }

  Scheduler &scheduler() { return scheduler_; }
  ExecutorAdapter &executor() { return executor_; }
  UnderlyingPool &underlying_pool() { return pool_; }

private:
  UnderlyingPool pool_;       // n 个底层线程
  ExecutorAdapter executor_;  // 适配到 IExecutor
  Scheduler scheduler_;       // 在 n 个线程上调度 m 个协程
};

// 默认实现：你现在的 ThreadPool + ThreadPoolExecutor + BasicCoroutineScheduler
using MnCoroutinePool =
    MnCoroutinePoolT<::ThreadPool, ThreadPoolExecutor, BasicCoroutineScheduler>;

} // namespace mental1104

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_MN_COROUTINE_POOL_H
