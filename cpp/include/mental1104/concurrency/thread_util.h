// include/mental1104/concurrency/thread_utils.h
#ifndef MENTAL1104_THREAD_UTILS_H
#define MENTAL1104_THREAD_UTILS_H

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

// ===================== 基本 sleep 工具 =====================

template <typename Rep, typename Period>
inline void sleep_for(const std::chrono::duration<Rep, Period> &dur) {
  std::this_thread::sleep_for(dur);
}

inline void sleep_for(int ms) { sleep_for(std::chrono::milliseconds(ms)); }

inline void sleep_for_ms(int ms) { sleep_for(ms); }

} // namespace mental1104

// ===================== ThreadPool 声明（实现放到 .cpp） =====================

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

// 模板实现必须留在头文件
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

#endif // MENTAL1104_THREAD_UTILS_H
