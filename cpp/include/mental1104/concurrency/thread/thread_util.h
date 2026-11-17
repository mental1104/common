// include/mental1104/concurrency/thread/thread_util.h
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
#include <tuple>
#include <type_traits>
#include <vector>

namespace mental1104 {

namespace detail {

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) ||                          \
    (!defined(_MSVC_LANG) && __cplusplus >= 201703L)
template <typename F, typename... Args>
using threadpool_invoke_result_t = std::invoke_result_t<F, Args...>;
#else
template <typename F, typename... Args>
using threadpool_invoke_result_t = typename std::result_of<F(Args...)>::type;
#endif

} // namespace detail

// ===================== 基本 sleep 工具 =====================

template <typename Rep, typename Period>
inline void sleep_for(const std::chrono::duration<Rep, Period> &dur) {
  std::this_thread::sleep_for(dur);
}

inline void sleep_for(int ms) { sleep_for(std::chrono::milliseconds(ms)); }

inline void sleep_for_ms(int ms) { sleep_for(ms); }

} // namespace mental1104

// ===================== ThreadPool 声明（实现放到 .cpp） =====================

/// 持有工作线程和任务队列，模板入口保留结果类型，队列同步由库负责。
class ThreadPool {
public:
  explicit ThreadPool(size_t numThreads);
  ~ThreadPool();

  template <typename F, typename... Args>
  auto submit(F &&f, Args &&...args)
      -> std::future<
          mental1104::detail::threadpool_invoke_result_t<F, Args...>>;

private:
  /// 将已适配成 void() 的任务放入队列，并唤醒一个工作线程。
  /// @param task 转移到队列的可调用对象；异常行为沿用容器操作。
  void enqueue(std::function<void()> task);

  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  std::mutex queueMutex;
  std::condition_variable condition;
  bool stop;
};

// 模板实现必须留在头文件
template <typename F, typename... Args>
auto ThreadPool::submit(F &&f, Args &&...args)
    -> std::future<mental1104::detail::threadpool_invoke_result_t<F, Args...>> {
  using ReturnType = mental1104::detail::threadpool_invoke_result_t<F, Args...>;

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) ||                          \
    (!defined(_MSVC_LANG) && __cplusplus >= 201703L)
  auto bound = [func = std::forward<F>(f),
                args_tuple = std::make_tuple(
                    std::forward<Args>(args)...)]() mutable -> ReturnType {
    return std::apply(std::move(func), std::move(args_tuple));
  };
  auto task =
      std::make_shared<std::packaged_task<ReturnType()>>(std::move(bound));
#else
  auto task = std::make_shared<std::packaged_task<ReturnType()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));
#endif

  std::future<ReturnType> result = task->get_future();
  // 任务包装保留具体结果类型，入队同步由库中的固定签名核心负责。
  this->enqueue([task]() { (*task)(); });

  return result;
}

#endif // MENTAL1104_THREAD_UTILS_H
