// include/mental1104/concurrency/thread/boost_asio_executor.h
#ifndef MENTAL1104_BOOST_ASIO_EXECUTOR_H
#define MENTAL1104_BOOST_ASIO_EXECUTOR_H

#pragma once

#include <functional>
#include <utility>

#if defined(M1104_HAS_ASYNC_SIMPLE)
#  include "async_simple/Executor.h"
#endif

#include "boost/asio/post.hpp"
#include "boost/asio/thread_pool.hpp"

#include "mental1104/concurrency/executor.h"

namespace mental1104 {

// 用 boost::asio::thread_pool 实现 IExecutor；可选实现 async_simple::Executor 接口
class BoostAsioExecutor
#if defined(M1104_HAS_ASYNC_SIMPLE)
    : public async_simple::Executor
    , public IExecutor
#else
    : public IExecutor
#endif
{
public:
  explicit BoostAsioExecutor(std::size_t thread_count)
      : pool_(thread_count) {}

  void execute(std::function<void()> fn) override {
    boost::asio::post(pool_, std::move(fn));
  }

#if defined(M1104_HAS_ASYNC_SIMPLE)
  bool schedule(async_simple::Executor::Func func) override {
    boost::asio::post(pool_, std::move(func));
    return true;
  }

  using async_simple::Executor::schedule;
#endif

  boost::asio::thread_pool &underlying_pool() { return pool_; }

private:
  boost::asio::thread_pool pool_;
};

} // namespace mental1104

#endif // MENTAL1104_BOOST_ASIO_EXECUTOR_H
