#include <gtest/gtest.h>

#if defined(M1104_HAS_ASYNC_SIMPLE)

#include <atomic>
#include <chrono>
#include <coroutine>
#include <memory>
#include <thread>

#include "async_simple/executors/SimpleExecutor.h"
#include "mental1104/concurrency/coroutine/async_simple_scheduler.h"
#include "mental1104/concurrency/coroutine/task.h"

using namespace mental1104;

namespace {

Task make_increment_task(std::atomic<int> &counter) {
  counter.fetch_add(1, std::memory_order_relaxed);
  co_return;
}

Task make_two_step_task(std::atomic<int> &counter) {
  counter.fetch_add(1, std::memory_order_relaxed);
  co_await std::suspend_always{};
  counter.fetch_add(1, std::memory_order_relaxed);
  co_return;
}

} // namespace

TEST(AsyncSimpleCoroutineScheduler, RunsTasks) {
  auto exec = std::make_shared<async_simple::executors::SimpleExecutor>(2);
  AsyncSimpleCoroutineScheduler sched(exec);

  std::atomic<int> counter{0};
  constexpr int kTasks = 32;
  for (int i = 0; i < kTasks; ++i) {
    sched.spawn_task(make_increment_task(counter));
  }
  std::cout << "[AsyncSimpleCoroutineSchedulerTest] spawned " << kTasks
            << " tasks.\n";
  sched.wait_all();
  EXPECT_EQ(counter.load(std::memory_order_relaxed), kTasks);
}

TEST(AsyncSimpleCoroutineScheduler, ResumesSuspendedTasks) {
  auto exec = std::make_shared<async_simple::executors::SimpleExecutor>(2);
  AsyncSimpleCoroutineScheduler sched(exec);

  std::atomic<int> counter{0};
  constexpr int kTasks = 8;
  for (int i = 0; i < kTasks; ++i) {
    sched.spawn_task(make_two_step_task(counter));
  }

  sched.wait_all();
  EXPECT_EQ(counter.load(std::memory_order_relaxed), kTasks * 2);
}

#else

TEST(AsyncSimpleCoroutineScheduler, SkipIfNoAsyncSimple) {
  GTEST_SKIP() << "async_simple not available; skipping adapter tests";
}

#endif
