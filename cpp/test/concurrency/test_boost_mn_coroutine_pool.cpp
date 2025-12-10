#include <atomic>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

#include "mental1104/concurrency/mn/boost_mn_coroutine_pool.h"

#if __cplusplus < 202002L
#error "test_boost_mn_coroutine_pool must be compiled with C++20 or higher"
#endif

using mental1104::BoostMnCoroutinePool;
using mental1104::Task;

namespace {

Task counting_coro(std::atomic<int> &counter, int steps) {
  using namespace std::chrono_literals;

  for (int i = 0; i < steps; ++i) {
    counter.fetch_add(1, std::memory_order_relaxed);

    // 打点输出方便观察调度线程
    std::cout << "[boost coro] step " << i << " on thread "
              << std::this_thread::get_id() << "\n";

    co_await std::suspend_always{};
  }
  co_return;
}

} // namespace

TEST(BoostMnCoroutinePoolTest, ExecutesAllSteps) {
  const int thread_count = 4; // 底层线程池线程数
  const int coro_count = 10;  // 启动的协程数量
  const int steps = 5;        // 每个协程执行的步数

  BoostMnCoroutinePool pool(thread_count);

  std::atomic<int> counter{0};

  for (int i = 0; i < coro_count; ++i) {
    pool.spawn(counting_coro(counter, steps));
  }

  pool.wait_all();

  const int expected = coro_count * steps;
  const int value = counter.load(std::memory_order_relaxed);

  std::cout << "[BoostMnCoroutinePoolTest] counter = " << value
            << ", expected = " << expected << std::endl;

  EXPECT_EQ(value, expected);
}
