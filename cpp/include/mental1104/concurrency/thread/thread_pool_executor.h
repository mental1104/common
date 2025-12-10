// include/mental1104/concurrency/thread/thread_pool_executor.h
#ifndef MENTAL1104_THREAD_POOL_EXECUTOR_H
#define MENTAL1104_THREAD_POOL_EXECUTOR_H

#pragma once

#include <utility>

#if defined(M1104_HAS_ASYNC_SIMPLE)
#include "async_simple/Executor.h"
#endif

#include "mental1104/concurrency/executor.h"
#include "mental1104/concurrency/thread/thread_util.h"

namespace mental1104 {

// 线程池执行器：内部自带 ThreadPool，既实现 IExecutor，也在可选时实现
// async_simple::Executor
class ThreadPoolExecutor
#if defined(M1104_HAS_ASYNC_SIMPLE)
    : public async_simple::Executor,
      public IExecutor
#else
    : public IExecutor
#endif
{
public:
  explicit ThreadPoolExecutor(std::size_t thread_count) : pool_(thread_count) {}

  void execute(std::function<void()> fn) override {
    pool_.submit(std::move(fn));
  }

#if defined(M1104_HAS_ASYNC_SIMPLE)
  bool schedule(async_simple::Executor::Func func) override {
    pool_.submit(std::move(func));
    return true;
  }

  using async_simple::Executor::schedule; // 继承可选的重载
#endif

  ::ThreadPool &underlying_pool() { return pool_; }

private:
  ::ThreadPool pool_;
};

} // namespace mental1104

#endif // MENTAL1104_THREAD_POOL_EXECUTOR_H
