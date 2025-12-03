// include/mental1104/concurrency/thread_pool_executor.h
#ifndef MENTAL1104_THREAD_POOL_EXECUTOR_H
#define MENTAL1104_THREAD_POOL_EXECUTOR_H

#pragma once

#include "mental1104/concurrency/thread_util.h"
#include "mental1104/concurrency/executor.h"

namespace mental1104 {

class ThreadPoolExecutor : public IExecutor {
public:
  explicit ThreadPoolExecutor(::ThreadPool &pool) : pool_(pool) {}

  void execute(std::function<void()> fn) override {
    // 不关心返回值，丢弃 future
    pool_.submit(std::move(fn));
  }

private:
  ::ThreadPool &pool_;
};

} // namespace mental1104

#endif // MENTAL1104_THREAD_POOL_EXECUTOR_H
