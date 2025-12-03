// include/mental1104/concurrency/mn/boost_mn_coroutine_pool.h
#ifndef MENTAL1104_BOOST_MN_COROUTINE_POOL_H
#define MENTAL1104_BOOST_MN_COROUTINE_POOL_H

#pragma once

#if __cplusplus >= 202002L

#include <boost/asio/thread_pool.hpp>

#include "mental1104/concurrency/thread/boost_asio_executor.h"
#include "mental1104/concurrency/coroutine/coroutine_scheduler.h"
#include "mental1104/concurrency/mn/mn_coroutine_pool.h"  // 为了 MnCoroutinePoolT 模板

namespace mental1104 {

// 用 Boost.Asio 的线程池作为底层线程池，BasicCoroutineScheduler 做调度
using BoostMnCoroutinePool =
    MnCoroutinePoolT<
        boost::asio::thread_pool,  // UnderlyingPool
        BoostAsioExecutor,         // ExecutorAdapter
        BasicCoroutineScheduler    // Scheduler
    >;

} // namespace mental1104

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_BOOST_MN_COROUTINE_POOL_H
