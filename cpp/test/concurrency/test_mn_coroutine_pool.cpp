#include <atomic>
#include <iostream>
#include <thread>
#include <gtest/gtest.h>

#include "mental1104/concurrency/mn/mn_coroutine_pool.h"

#if __cplusplus < 202002L
#  error "test_mn_coroutine_pool must be compiled with C++20 or higher"
#endif

using mental1104::MnCoroutinePool;
using mental1104::Task;

Task counting_coro(std::atomic<int>& counter, int steps) {
  using namespace std::chrono_literals;

  for (int i = 0; i < steps; ++i) {
    counter.fetch_add(1, std::memory_order_relaxed);

    // 打点输出方便肉眼观察调度情况（非必需）
    std::cout << "[coro] step " << i
              << " on thread " << std::this_thread::get_id() << "\n";

    co_await std::suspend_always{};
  }
  co_return;
}

TEST(MnCoroutinePoolTest, ExecutesAllSteps) {
  const int thread_count = 4;  // n
  const int coro_count   = 10; // m
  const int steps        = 5;  // 每个协程步数

  MnCoroutinePool pool(thread_count);

  std::atomic<int> counter{0};

  for (int i = 0; i < coro_count; ++i) {
    pool.spawn(counting_coro(counter, steps));
  }

  pool.wait_all();

  int expected = coro_count * steps;
  int value = counter.load(std::memory_order_relaxed);

  std::cout << "[MnCoroutinePoolTest] counter = " << value
            << ", expected = " << expected << std::endl;

  EXPECT_EQ(value, expected);
}
