#ifndef __MENTAL1104_THREAD_UTILS
#define __MENTAL1104_THREAD_UTILS

// Thread utilities: sleep helpers + simple thread pool.
#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mental1104 {

template <typename Rep, typename Period>
inline void sleep_for(const std::chrono::duration<Rep, Period> &dur) {
  std::this_thread::sleep_for(dur);
}

inline void sleep_for(int ms) {
  sleep_for(std::chrono::milliseconds(ms));
}

inline void sleep_for_ms(int ms) {
  sleep_for(ms);
}

} // namespace mental1104

class ThreadPool {
public:
  explicit ThreadPool(size_t numThreads);
  ~ThreadPool();

  template <typename F, typename... Args>
  auto submit(F &&f,
              Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>;

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  std::mutex queueMutex;
  std::condition_variable condition;
  bool stop;
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(queueMutex);
          condition.wait(lock, [this] { return stop || !tasks.empty(); });

          if (stop && tasks.empty()) {
            return;
          }

          task = std::move(tasks.front());
          tasks.pop();
        }
        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) {
    worker.join();
  }
}

template <typename F, typename... Args>
auto ThreadPool::submit(F &&f, Args &&...args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using ReturnType = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<ReturnType()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<ReturnType> result = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    tasks.emplace([task]() { (*task)(); });
  }
  condition.notify_one();

  return result;
}

#endif
