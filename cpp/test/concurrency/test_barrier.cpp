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

struct CountingCompletion {
  explicit CountingCompletion(std::atomic<int> *count) : count(count) {}

  void operator()() noexcept { this->count->fetch_add(1); }

  std::atomic<int> *count;
};

void join_all(std::vector<std::thread> &threads) {
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }
}

} // namespace

TEST(BarrierTest, UsesStandardBarrierWhenAvailable) {
#if M1104_HAS_STD_BARRIER
  EXPECT_TRUE((std::is_same<mental1104::barrier<CountingCompletion>,
                            std::barrier<CountingCompletion>>::value));
#else
  SUCCEED();
#endif
}

TEST(BarrierTest, ArrivalTokenMeetsStandardMoveRequirements) {
  typedef mental1104::barrier<> barrier_type;
  typedef barrier_type::arrival_token arrival_token;

  // 标准只保证 arrival_token 可移动构造、可移动赋值和可析构；
  // 是否支持复制属于实现细节，不能作为 std::barrier 兼容性断言。
  EXPECT_TRUE(std::is_move_constructible<arrival_token>::value);
  EXPECT_TRUE(std::is_move_assignable<arrival_token>::value);
  EXPECT_TRUE(std::is_destructible<arrival_token>::value);
}

TEST(BarrierTest, SingleParticipantCanReuseBarrier) {
  mental1104::barrier<> barrier(1);

  barrier.arrive_and_wait();
  barrier.arrive_and_wait();
  barrier.arrive_and_wait();
}

TEST(BarrierTest, AllParticipantsMeetBeforeContinuing) {
  const int worker_count = 4;
  std::atomic<int> arrived(0);
  std::atomic<int> passed(0);
  std::atomic<bool> observed_all(true);
  mental1104::barrier<> barrier(worker_count);
  std::vector<std::thread> threads;

  for (int worker = 0; worker < worker_count; ++worker) {
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
        barrier.arrive_and_wait();

        if (worker == 0) {
          for (int index = 0; index < worker_count; ++index) {
            if (values[static_cast<std::size_t>(index)] != phase + 1) {
              observed_values.store(false);
            }
          }
        }

        // Do not let another worker overwrite its slot for the next phase
        // while worker zero is still validating the current phase.
        barrier.arrive_and_wait();
      }
    }));
  }

  join_all(threads);

  EXPECT_TRUE(observed_values.load());
}

TEST(BarrierTest, CompletionRunsOncePerPhaseAndIsVisible) {
  const int worker_count = 4;
  const int phase_count = 64;
  std::atomic<int> completion_count(0);
  std::atomic<bool> completion_visible(true);
  mental1104::barrier<CountingCompletion> barrier(
      worker_count, CountingCompletion(&completion_count));
  std::vector<std::thread> threads;

  for (int worker = 0; worker < worker_count; ++worker) {
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

TEST(BarrierTest, ArriveAndWaitCanBeSeparated) {
  mental1104::barrier<> barrier(2);
  std::atomic<bool> worker_passed(false);

  mental1104::barrier<>::arrival_token arrival = barrier.arrive();
  std::thread worker([&] {
    barrier.arrive_and_wait();
    worker_passed.store(true);
  });

  barrier.wait(std::move(arrival));
  worker.join();

  EXPECT_TRUE(worker_passed.load());
}

TEST(BarrierTest, ArriveAcceptsMultipleUpdates) {
  mental1104::barrier<> barrier(3);
  std::atomic<bool> worker_passed(false);

  mental1104::barrier<>::arrival_token arrival = barrier.arrive(2);
  std::thread worker([&] {
    barrier.arrive_and_wait();
    worker_passed.store(true);
  });

  barrier.wait(std::move(arrival));
  worker.join();

  EXPECT_TRUE(worker_passed.load());
}

TEST(BarrierTest, ArriveAndDropChangesFollowingPhases) {
  std::atomic<int> completion_count(0);
  mental1104::barrier<CountingCompletion> barrier(
      3, CountingCompletion(&completion_count));

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
