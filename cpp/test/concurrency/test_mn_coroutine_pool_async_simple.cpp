#include <atomic>
#include <iostream>
#include <thread>
#include <gtest/gtest.h>

#include "mental1104/concurrency/mn/mn_coroutine_pool.h"

#if __cplusplus < 202002L
#  error "test_mn_coroutine_pool_async_simple must be compiled with C++20 or higher"
#endif

using mental1104::Task;

namespace {

Task counting_coro(std::atomic<int>& counter, int steps) {
  for (int i = 0; i < steps; ++i) {
    counter.fetch_add(1, std::memory_order_relaxed);
    co_await std::suspend_always{};
  }
  co_return;
}

}  // namespace

#if defined(M1104_HAS_ASYNC_SIMPLE)

TEST(MnCoroutinePoolAsyncSimpleTest, ExecutesAllSteps) {
  const int thread_count = 4;
  const int coro_count   = 12;
  const int steps        = 4;

  mental1104::MnCoroutinePoolAsyncSimple pool(thread_count);
  std::atomic<int> counter{0};

  for (int i = 0; i < coro_count; ++i) {
    pool.spawn(counting_coro(counter, steps));
  }

  pool.wait_all();
  EXPECT_EQ(counter.load(std::memory_order_relaxed), coro_count * steps);
}

TEST(BoostMnCoroutinePoolAsyncSimpleTest, ExecutesAllSteps) {
  const int thread_count = 4;
  const int coro_count   = 10;
  const int steps        = 3;

  mental1104::BoostMnCoroutinePoolAsyncSimple pool(thread_count);
  std::atomic<int> counter{0};

  for (int i = 0; i < coro_count; ++i) {
    pool.spawn(counting_coro(counter, steps));
  }

  pool.wait_all();
  EXPECT_EQ(counter.load(std::memory_order_relaxed), coro_count * steps);
}

#else

TEST(MnCoroutinePoolAsyncSimpleTest, SkipIfNoAsyncSimple) {
  GTEST_SKIP() << "async_simple not available; skipping async_simple pool tests";
}

TEST(BoostMnCoroutinePoolAsyncSimpleTest, SkipIfNoAsyncSimple) {
  GTEST_SKIP() << "async_simple not available; skipping async_simple pool tests";
}

#endif
