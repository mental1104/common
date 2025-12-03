// include/mental1104/concurrency/thread/boost_asio_executor.h
#ifndef MENTAL1104_BOOST_ASIO_EXECUTOR_H
#define MENTAL1104_BOOST_ASIO_EXECUTOR_H

#pragma once

#include <functional>
#include <utility>

#include "boost/asio/thread_pool.hpp"
#include "boost/asio/post.hpp"

#include "mental1104/concurrency/executor.h"

namespace mental1104 {

// 用 boost::asio::thread_pool 实现 IExecutor
class BoostAsioExecutor : public IExecutor {
public:
  explicit BoostAsioExecutor(boost::asio::thread_pool& pool)
    : pool_(pool) {}

  void execute(std::function<void()> fn) override {
    // 把任务丢进 Asio 线程池
    boost::asio::post(pool_, std::move(fn));
  }

private:
  boost::asio::thread_pool& pool_;
};

} // namespace mental1104

#endif // MENTAL1104_BOOST_ASIO_EXECUTOR_H
