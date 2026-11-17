#include <atomic>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "mental1104/concurrency/singleflight.h"

TEST(SingleFlightGroupTest, CoalescesConcurrentCallsForSameKey) {
  mental1104::SingleFlightGroup<std::string, std::string> group;
  std::mutex gate_mutex;
  std::condition_variable gate_condition;
  bool started = false;
  bool released = false;
  std::atomic<int> loader_calls(0);
  const int worker_count = 8;
  std::vector<mental1104::SingleFlightResult<std::string> > results(
      worker_count, mental1104::SingleFlightResult<std::string>{"", false});
  std::vector<std::thread> threads;

  std::function<std::string()> loader = [&]() {
    ++loader_calls;
    std::unique_lock<std::mutex> lock(gate_mutex);
    started = true;
    gate_condition.notify_all();
    while (!released) {
      gate_condition.wait(lock);
    }
    return std::string("value");
  };

  threads.push_back(std::thread([&]() {
    results[0] = group.do_call("product:123", loader);
  }));
  {
    std::unique_lock<std::mutex> lock(gate_mutex);
    while (!started) {
      gate_condition.wait(lock);
    }
  }

  for (int i = 1; i < worker_count; ++i) {
    threads.push_back(std::thread([&, i]() {
      results[static_cast<std::size_t>(i)] =
          group.do_call("product:123", loader);
    }));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  {
    std::lock_guard<std::mutex> lock(gate_mutex);
    released = true;
  }
  gate_condition.notify_all();

  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i].join();
  }

  EXPECT_EQ(loader_calls.load(), 1);
  int shared_count = 0;
  for (std::size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].value, "value");
    if (results[i].shared) {
      ++shared_count;
    }
  }
  EXPECT_EQ(shared_count, worker_count - 1);
}

TEST(SingleFlightGroupTest, DifferentKeysRunIndependently) {
  mental1104::SingleFlightGroup<std::string, int> group;
  std::mutex mutex;
  std::condition_variable condition;
  int ready = 0;
  bool released = false;
  int left = 0;
  int right = 0;

  std::function<int(int)> loader = [&](int value) {
    std::unique_lock<std::mutex> lock(mutex);
    ++ready;
    condition.notify_all();
    while (!released) {
      condition.wait(lock);
    }
    return value;
  };

  std::thread left_thread(
      [&]() { left = group.do_call("left", [&]() { return loader(1); }).value; });
  std::thread right_thread([&]() {
    right = group.do_call("right", [&]() { return loader(2); }).value;
  });

  {
    std::unique_lock<std::mutex> lock(mutex);
    while (ready < 2) {
      condition.wait(lock);
    }
    released = true;
  }
  condition.notify_all();
  left_thread.join();
  right_thread.join();

  EXPECT_EQ(left, 1);
  EXPECT_EQ(right, 2);
}

TEST(SingleFlightGroupTest, SharesLoaderErrorAndRecovers) {
  mental1104::SingleFlightGroup<std::string, std::string> group;
  std::mutex gate_mutex;
  std::condition_variable gate_condition;
  bool started = false;
  bool released = false;
  std::atomic<int> loader_calls(0);
  std::string first_error;
  std::string second_error;

  std::function<std::string()> loader = [&]() -> std::string {
    ++loader_calls;
    std::unique_lock<std::mutex> lock(gate_mutex);
    started = true;
    gate_condition.notify_all();
    while (!released) {
      gate_condition.wait(lock);
    }
    throw std::runtime_error("boom");
  };

  std::thread first([&]() {
    try {
      group.do_call("key", loader);
    } catch (const std::exception &error) {
      first_error = error.what();
    }
  });
  {
    std::unique_lock<std::mutex> lock(gate_mutex);
    while (!started) {
      gate_condition.wait(lock);
    }
  }
  std::thread second([&]() {
    try {
      group.do_call("key", loader);
    } catch (const std::exception &error) {
      second_error = error.what();
    }
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  {
    std::lock_guard<std::mutex> lock(gate_mutex);
    released = true;
  }
  gate_condition.notify_all();
  first.join();
  second.join();

  EXPECT_EQ(loader_calls.load(), 1);
  EXPECT_EQ(first_error, "boom");
  EXPECT_EQ(second_error, "boom");
  EXPECT_EQ(group.do_call("key", []() { return std::string("recovered"); })
                .value,
            "recovered");
}
