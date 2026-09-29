#include "mental1104/concurrency/barrier.h"
#include "mental1104/meta/barrier_support.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

/// 记录 barrier completion function 被调用的总次数。
///
/// 该辅助类型只借用外部 atomic，不拥有计数器；测试必须保证计数器生命周期
/// 覆盖 barrier 及其所有参与线程。
struct CountingCompletion {
  /// 创建一个写入指定原子计数器的 completion function。
  ///
  /// @param count 每个 phase 完成时递增的计数器，不允许为 nullptr。
  explicit CountingCompletion(std::atomic<int> *count) : count(count) {}

  /// 记录当前 phase 已完成；可由任意最后到达的参与线程调用。
  void operator()() noexcept { this->count->fetch_add(1); }

  /// completion function 借用的原子计数器。
  std::atomic<int> *count;
};

/// 等待线程集合中的每个线程结束。
///
/// @param threads 待 join 的线程集合；调用后其中所有线程均不可再次 join。
void join_all(std::vector<std::thread> &threads) {
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }
}

} // namespace

/// 验证标准库实现可用时，公共别名不会继续使用 fallback 类型。
TEST(BarrierTest, UsesStandardBarrierWhenAvailable) {
#if M1104_HAS_STD_BARRIER
  EXPECT_TRUE((std::is_same<mental1104::barrier<CountingCompletion>,
                            std::barrier<CountingCompletion>>::value));
#else
  SUCCEED();
#endif
}

/// 验证 arrival_token 满足标准要求的移动与析构能力。
TEST(BarrierTest, ArrivalTokenMeetsStandardMoveRequirements) {
  typedef mental1104::barrier<> barrier_type;
  typedef barrier_type::arrival_token arrival_token;

  // 标准只保证 arrival_token 可移动构造、可移动赋值和可析构；
  // 是否支持复制属于实现细节，不能作为 std::barrier 兼容性断言。
  EXPECT_TRUE(std::is_move_constructible<arrival_token>::value);
  EXPECT_TRUE(std::is_move_assignable<arrival_token>::value);
  EXPECT_TRUE(std::is_destructible<arrival_token>::value);
}

/// 验证单参与者屏障可以连续完成多个 phase，而不会残留上一轮状态。
TEST(BarrierTest, SingleParticipantCanReuseBarrier) {
  mental1104::barrier<> barrier(1);

  barrier.arrive_and_wait();
  barrier.arrive_and_wait();
  barrier.arrive_and_wait();
}

/// 验证任一参与者都不能在所有线程到达当前 phase 前继续执行。
TEST(BarrierTest, AllParticipantsMeetBeforeContinuing) {
  const int worker_count = 4;
  std::atomic<int> arrived(0);
  std::atomic<int> passed(0);
  std::atomic<bool> observed_all(true);
  mental1104::barrier<> barrier(worker_count);
  std::vector<std::thread> threads;

  for (int worker = 0; worker < worker_count; ++worker) {
    // 每个 worker 先发布到达事实，再通过 barrier 等待其他参与者；越过屏障后
    // 应当能观察到 arrived 已达到 worker_count。
    threads.push_back(std::thread([&] {
      arrived.fetch_add(1);
      barrier.arrive_and_wait();
      if (arrived.load() != worker_count) {
        observed_all.store(false);
      }
      passed.fetch_add(1);
    }));
  }

  join_all(threads);

  EXPECT_TRUE(observed_all.load());
  EXPECT_EQ(worker_count, passed.load());
}

/// 验证 barrier 可跨多个 phase 复用，并发布屏障前的普通内存写入。
TEST(BarrierTest, ReusesAcrossPhasesAndPublishesWrites) {
  const int worker_count = 4;
  const int phase_count = 64;
  std::vector<int> values(static_cast<std::size_t>(worker_count), 0);
  std::atomic<bool> observed_values(true);
  mental1104::barrier<> barrier(worker_count);
  std::vector<std::thread> threads;

  for (int worker = 0; worker < worker_count; ++worker) {
    threads.push_back(std::thread([&, worker] {
      for (int phase = 0; phase < phase_count; ++phase) {
        values[static_cast<std::size_t>(worker)] = phase + 1;

        // 第一处屏障发布本轮所有 worker 的写入，使 worker 0 可以统一校验。
        barrier.arrive_and_wait();

        if (worker == 0) {
          for (int index = 0; index < worker_count; ++index) {
            if (values[static_cast<std::size_t>(index)] != phase + 1) {
              observed_values.store(false);
            }
          }
        }

        // 第二处屏障等待校验结束，避免其他 worker 提前覆盖下一 phase 的槽位。
        barrier.arrive_and_wait();
      }
    }));
  }

  join_all(threads);

  EXPECT_TRUE(observed_values.load());
}

/// 验证 completion 每个 phase 恰好执行一次，且越过屏障后对所有线程可见。
TEST(BarrierTest, CompletionRunsOncePerPhaseAndIsVisible) {
  const int worker_count = 4;
  const int phase_count = 64;
  std::atomic<int> completion_count(0);
  std::atomic<bool> completion_visible(true);
  mental1104::barrier<CountingCompletion> barrier(
      worker_count, CountingCompletion(&completion_count));
  std::vector<std::thread> threads;

  for (int worker = 0; worker < worker_count; ++worker) {
    // completion 由每轮最后一个到达的线程执行，但所有参与者返回时都必须已经
    // 观察到对应轮次的计数增长。
    threads.push_back(std::thread([&] {
      for (int phase = 0; phase < phase_count; ++phase) {
        barrier.arrive_and_wait();
        if (completion_count.load() < phase + 1) {
          completion_visible.store(false);
        }
      }
    }));
  }

  join_all(threads);

  EXPECT_TRUE(completion_visible.load());
  EXPECT_EQ(phase_count, completion_count.load());
}

/// 验证 arrive() 与 wait() 可以分离，并通过移动 token 连接两次操作。
TEST(BarrierTest, ArriveAndWaitCanBeSeparated) {
  mental1104::barrier<> barrier(2);
  std::atomic<bool> worker_passed(false);

  // 主线程先提交一次到达但暂不等待，worker 提交第二次到达并完成 phase。
  mental1104::barrier<>::arrival_token arrival = barrier.arrive();
  std::thread worker([&] {
    barrier.arrive_and_wait();
    worker_passed.store(true);
  });

  barrier.wait(std::move(arrival));
  worker.join();

  EXPECT_TRUE(worker_passed.load());
}

/// 验证一次 arrive(update) 可以代表多个参与者的到达计数。
TEST(BarrierTest, ArriveAcceptsMultipleUpdates) {
  mental1104::barrier<> barrier(3);
  std::atomic<bool> worker_passed(false);

  // 主线程一次贡献两个计数，worker 的一次到达应当补齐第三个计数并释放双方。
  mental1104::barrier<>::arrival_token arrival = barrier.arrive(2);
  std::thread worker([&] {
    barrier.arrive_and_wait();
    worker_passed.store(true);
  });

  barrier.wait(std::move(arrival));
  worker.join();

  EXPECT_TRUE(worker_passed.load());
}

/// 验证 arrive_and_drop() 同时完成当前到达并减少后续 phase 的参与者数量。
TEST(BarrierTest, ArriveAndDropChangesFollowingPhases) {
  std::atomic<int> completion_count(0);
  mental1104::barrier<CountingCompletion> barrier(
      3, CountingCompletion(&completion_count));

  // dropping_worker 只参与首个 phase；后续 phase 应只等待剩余两个 worker。
  std::thread dropping_worker([&] { barrier.arrive_and_drop(); });
  std::thread first_worker([&] {
    barrier.arrive_and_wait();
    barrier.arrive_and_wait();
  });
  std::thread second_worker([&] {
    barrier.arrive_and_wait();
    barrier.arrive_and_wait();
  });

  dropping_worker.join();
  first_worker.join();
  second_worker.join();

  EXPECT_EQ(2, completion_count.load());
}

/// 验证频繁主动让出 CPU 时，多 phase 状态仍不会丢失通知或重复完成。
TEST(BarrierTest, SurvivesSchedulingJitterAcrossManyPhases) {
  const int worker_count = 8;
  const int phase_count = 128;
  std::atomic<int> completion_count(0);
  mental1104::barrier<CountingCompletion> barrier(
      worker_count, CountingCompletion(&completion_count));
  std::vector<std::thread> threads;

  for (int worker = 0; worker < worker_count; ++worker) {
    threads.push_back(std::thread([&, worker] {
      for (int phase = 0; phase < phase_count; ++phase) {
        // 按 worker 与 phase 组合注入不同的调度扰动，扩大到达顺序变化范围。
        if ((worker + phase) % 3 == 0) {
          std::this_thread::yield();
        }
        barrier.arrive_and_wait();
      }
    }));
  }

  join_all(threads);

  EXPECT_EQ(phase_count, completion_count.load());
}
