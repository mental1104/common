// include/mental1104/concurrency/coroutine/async_simple_scheduler.h
#ifndef MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H
#define MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H

#pragma once

#if __cplusplus >= 202002L

#if defined(M1104_HAS_ASYNC_SIMPLE)

#include "async_simple/Executor.h"
#include <condition_variable>
#include <memory>
#include <mutex>

#include "mental1104/concurrency/coroutine/coroutine_scheduler.h"

namespace mental1104 {

// 使用 async_simple::Executor 适配 ICoroutineScheduler
class AsyncSimpleCoroutineScheduler : public ICoroutineScheduler {
public:
  // 使用 shared_ptr 主要是共享所有权并保证调度期间 executor 存活；若调用方能保证生命周期，也可改为引用/裸指针。
  explicit AsyncSimpleCoroutineScheduler(
      std::shared_ptr<async_simple::Executor> exec)
      : exec_(std::move(exec)), pending_(0) {
    if (!exec_) { // 若传入的 shared_ptr 为空（即没有有效 Executor），这里直接抛异常拒绝构造
      throw std::invalid_argument(
          "AsyncSimpleCoroutineScheduler requires a valid executor");
    }
  }

  void spawn_task(Task t) override {
    if (!t) // Task 有 explicit operator bool；默认构造/被 move 后 handle_ 为空时为 false，会走这里
      return;
    // Task 只能 move；这里把所有权转移到 holder 中，调用侧需传 rvalue 或 std::move，原对象被置空后不应再用
    // 若先 pending_++ 再 schedule_resume(std::make_shared<Task>(std::move(t))) 也可；这里拆成 holder 仅为清晰并确保调度前已完成计数
    auto holder = std::make_shared<Task>(std::move(t));
    pending_.fetch_add(1, std::memory_order_relaxed); // relaxed 只保证原子性不提供跨线程顺序；seq_cst 额外提供全局总序/更强栅栏，通常更慢
    schedule_resume(std::move(holder));
  }

  void wait_all() override {
    std::unique_lock<std::mutex> lk(mu_); // cv_.wait 的前置条件是 lock 持有 mutex；不加锁会导致未定义行为/可能丢通知
    cv_.wait(lk, [this] {
      return pending_.load(std::memory_order_acquire) == 0; // 等 pending_ 归零，表示所有任务完成
    });
  }

private:
  void schedule_resume(std::shared_ptr<Task> task) {
    // 显式使用 this-> 便于静态检查与区分成员变量
    // 将任务投递到执行器：尝试 resume 一次，若完成则减计数并唤醒等待者；未完成则再次调度继续执行
    // 每次调用都会创建一个新的 lambda 并交给 Executor；是否并发运行取决于 Executor 的实现（线程池可并发，单线程则串行）
    bool scheduled = this->exec_->schedule([this, task = std::move(task)]() mutable {
      task->resume(); // 在执行器线程上推进协程一步
      if (task->done()) { // 若本次推进后已完成
        // 这里不用 relaxed：需要 release 让计数变化对等待方可见；不用 seq_cst：更强但无额外收益；用 acq_rel 是折中且与 wait_all 的 acquire 配对
        this->pending_.fetch_sub(1, std::memory_order_acq_rel); // 结束路径需要 release 保证计数更新对 wait_all 可见，acquire 便于配合等待方的 acquire 读取
        this->cv_.notify_all(); // 唤醒 wait_all 等待者
      } else {
        // 未完成则再次投递，同一 Task 可能被不同线程继续执行（取决于 Executor）
        schedule_resume(std::move(task));
      }
    });
    if (!scheduled) {
      this->pending_.fetch_sub(1, std::memory_order_acq_rel);
      this->cv_.notify_all();
    }
  }

private:
  std::shared_ptr<async_simple::Executor> exec_;
  std::atomic<int> pending_;
  std::mutex mu_;
  std::condition_variable cv_;
};

} // namespace mental1104

#endif // async_simple available

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_ASYNC_SIMPLE_SCHEDULER_H
