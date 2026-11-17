#include "mental1104/concurrency/thread/thread_util.h" // 线程池和线程工具
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

TEST(ThreadPoolTest, SingleTaskWithReturnValue) {
  ThreadPool pool(2);

  auto future = pool.submit([] { return 42; });

  EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, SingleTaskWithoutReturnValue) {
  ThreadPool pool(2);

  bool executed = false;
  auto future = pool.submit([&] { executed = true; });
  future.get(); // 等待任务完成

  EXPECT_TRUE(executed);
}

TEST(ThreadPoolTest, MultipleTasksExecution) {
  ThreadPool pool(4);

  std::vector<std::future<int>> futures;
  for (int i = 0; i < 8; ++i) {
    futures.emplace_back(pool.submit([i] {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return i * i;
    }));
  }

  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(futures[i].get(), i * i);
  }
}

TEST(ThreadPoolTest, TaskWithException) {
  ThreadPool pool(2);

  auto future =
      pool.submit([]() -> int { throw std::runtime_error("Test exception"); });

  EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ThreadPoolTest, ThreadPoolDestruction) {
  auto pool = std::make_unique<ThreadPool>(2);

  auto future = pool->submit([] { return 42; });
  EXPECT_EQ(future.get(), 42);

  // 线程池在析构时应该能正常退出
}

TEST(ThreadPoolTest, LargeNumberOfTasks) {
  ThreadPool pool(4);

  const int numTasks = 1000;
  std::vector<std::future<int>> futures;

  for (int i = 0; i < numTasks; ++i) {
    futures.push_back(pool.submit([i] { return i; }));
  }

  for (int i = 0; i < numTasks; ++i) {
    EXPECT_EQ(futures[i].get(), i);
  }
}

TEST(ThreadPoolTest, IOBoundTasks) {
  ThreadPool pool(4);

  const int numTasks = 100;
  std::vector<std::future<void>> futures;

  for (int i = 0; i < numTasks; ++i) {
    futures.push_back(pool.submit([] {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(50)); // 模拟 I/O 等待
    }));
  }

  for (auto &future : futures) {
    EXPECT_NO_THROW(future.get());
  }
}

TEST(ThreadPoolTest, CPUBoundTasks) {
  ThreadPool pool(4);

  const int numTasks = 10;
  std::vector<std::future<int>> futures;

  for (int i = 0; i < numTasks; ++i) {
    futures.push_back(pool.submit([i] {
      // 计算大数的阶乘，模拟 CPU 密集型任务
      constexpr int numIterations = 1'000'000;
      int sum = 0;
      for (int j = 0; j < numIterations; ++j) {
        sum += j;
      }
      return sum + i;
    }));
  }

  for (auto &future : futures) {
    EXPECT_NO_THROW(future.get());
  }
}
