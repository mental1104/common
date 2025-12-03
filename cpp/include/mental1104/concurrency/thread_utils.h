#ifndef __MENTAL1104_THREAD_UTILS
#define __MENTAL1104_THREAD_UTILS

#pragma once

// 基础线程 + 并发工具
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <type_traits>

#if __cplusplus >= 202002L
  #include <coroutine>
#endif

namespace mental1104 {

// ===================== 基本 sleep 工具 =====================

template <typename Rep, typename Period>
inline void sleep_for(const std::chrono::duration<Rep, Period> &dur) {
  std::this_thread::sleep_for(dur);
}

inline void sleep_for(int ms) { sleep_for(std::chrono::milliseconds(ms)); }

inline void sleep_for_ms(int ms) { sleep_for(ms); }

// ===================== 抽象执行器接口（为将来换 Boost 等预留） =====================

class IExecutor {
public:
  virtual ~IExecutor() = default;

  // 提交一个可执行任务，不关心返回值（fire-and-forget）
  virtual void execute(std::function<void()> fn) = 0;
};

} // namespace mental1104

// ===================== 现有 ThreadPool 定义（保持原样接口） =====================

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

inline ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
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

inline ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
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

// ===================== 将 ThreadPool 适配为 IExecutor =====================

namespace mental1104 {

class ThreadPoolExecutor : public IExecutor {
public:
  explicit ThreadPoolExecutor(::ThreadPool &pool) : pool_(pool) {}

  void execute(std::function<void()> fn) override {
    // 我们不关心返回值，直接丢弃 future
    pool_.submit(std::move(fn));
  }

private:
  ::ThreadPool &pool_;
};

} // namespace mental1104

// ===================== C++20 及以上：协程 Task + 协程调度器 =====================

#if __cplusplus >= 202002L

namespace mental1104 {

// ---------- Task<void>：你这侧统一协程类型 ----------

class Task {
public:
  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;

  Task() noexcept : handle_(nullptr) {}
  Task(handle_type h) : handle_(h) {}

  Task(Task &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }
  Task(const Task &) = delete;

  ~Task() {
    if (handle_) {
      handle_.destroy();
    }
  }

  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  bool done() const {
    return !handle_ || handle_.done();
  }

  void resume() {
    if (handle_ && !handle_.done()) {
      handle_.resume();
    }
  }

  handle_type native_handle() const { return handle_; }

  explicit operator bool() const noexcept { return handle_ != nullptr; }

  struct promise_type {
    auto get_return_object() {
      return Task{handle_type::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
  };

private:
  handle_type handle_;
};

// ---------- 协程调度器抽象接口（为将来换阿里协程池预留） ----------

class ICoroutineScheduler {
public:
  virtual ~ICoroutineScheduler() = default;

  // 提交一个协程任务
  virtual void spawn_task(Task t) = 0;

  // 等所有任务完成（具体语义由实现定义）
  virtual void wait_all() = 0;
};

// ---------- 基于 IExecutor 的一个默认协程调度器实现 ----------
// 这就是我们现在的 “m 协程 → n 线程” 基线实现，
// 后面可以用适配器换成阿里云协程池、Boost.Asio 等。

class BasicCoroutineScheduler : public ICoroutineScheduler {
public:
  explicit BasicCoroutineScheduler(IExecutor &executor,
                                   std::size_t scheduler_workers = 1)
      : executor_(executor),
        stopping_(false),
        pending_(0) {
    start(scheduler_workers);
  }

  ~BasicCoroutineScheduler() override {
    stop();
  }

  void spawn_task(Task t) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ready_.push(std::move(t));
      ++pending_;
    }
  }

  // 粗糙版 wait_all：轮询 pending_，工程里你可以改成条件变量
  void wait_all() override {
    using namespace std::chrono_literals;
    while (pending_.load(std::memory_order_acquire) > 0) {
      std::this_thread::sleep_for(1ms);
    }
  }

  void stop() {
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true)) {
      return; // 已经停过
    }
  }

private:
  void start(std::size_t workers) {
    if (workers == 0) workers = 1;
    for (std::size_t i = 0; i < workers; ++i) {
      executor_.execute([this] { scheduler_loop(); });
    }
  }

  void scheduler_loop() {
    using namespace std::chrono_literals;

    while (!stopping_.load(std::memory_order_acquire)) {
      Task t;

      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ready_.empty()) {
          t = std::move(ready_.front());
          ready_.pop();
        }
      }

      if (!t) {
        std::this_thread::sleep_for(1ms);
        continue;
      }

      t.resume();

      if (t.done()) {
        pending_.fetch_sub(1, std::memory_order_acq_rel);
      } else {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_.push(std::move(t));
      }
    }
  }

private:
  IExecutor &executor_;
  std::queue<Task> ready_;
  std::mutex mutex_;
  std::atomic<bool> stopping_;
  std::atomic<int> pending_;
};

// UnderlyingPool: 底层线程池类型（如 ::ThreadPool 或 boost::asio::thread_pool）
// ExecutorAdapter: 把 UnderlyingPool 适配成 IExecutor 的适配器类型
// Scheduler: 协程调度器类型，需实现 ICoroutineScheduler 接口
template <class UnderlyingPool,
          class ExecutorAdapter,
          class Scheduler = BasicCoroutineScheduler>
class MnCoroutinePoolT {
public:
  // 这里假设：
  // 1) UnderlyingPool 可用 (size_t thread_count) 构造
  // 2) ExecutorAdapter 可用 (UnderlyingPool&) 构造
  // 3) Scheduler 可用 (IExecutor&, size_t scheduler_workers) 构造
  explicit MnCoroutinePoolT(std::size_t thread_count,
                            std::size_t scheduler_workers = 0)
      : pool_(thread_count),
        executor_(pool_),
        scheduler_(
            executor_,
            scheduler_workers == 0 ? thread_count : scheduler_workers) {
    static_assert(std::is_base_of_v<IExecutor, ExecutorAdapter>,
                  "ExecutorAdapter must derive from IExecutor");
    static_assert(std::is_base_of_v<ICoroutineScheduler, Scheduler>,
                  "Scheduler must implement ICoroutineScheduler");
  }

  // 提交一个协程任务（m 之一）
  void spawn(Task t) {
    scheduler_.spawn_task(std::move(t));
  }

  // 等待所有已提交任务完成
  void wait_all() {
    scheduler_.wait_all();
  }

  // 如果你有需要，也可以把 scheduler / executor 暴露出来做高级玩法
  Scheduler& scheduler() { return scheduler_; }
  ExecutorAdapter& executor() { return executor_; }
  UnderlyingPool& underlying_pool() { return pool_; }

private:
  UnderlyingPool pool_;        // n 个底层线程池
  ExecutorAdapter executor_;   // 适配成 IExecutor
  Scheduler scheduler_;        // 在 n 个线程上调度 m 个协程
};

// 默认实现：用你现在的 ThreadPool + ThreadPoolExecutor + BasicCoroutineScheduler
using MnCoroutinePool =
    MnCoroutinePoolT<::ThreadPool, ThreadPoolExecutor, BasicCoroutineScheduler>;

    
} // namespace mental1104

#endif // __cplusplus >= 202002L

#endif // __MENTAL1104_THREAD_UTILS
