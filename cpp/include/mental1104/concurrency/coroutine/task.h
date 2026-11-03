// include/mental1104/concurrency/coroutine/task.h
#ifndef MENTAL1104_TASK_H
#define MENTAL1104_TASK_H

#pragma once

#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX20

#include <coroutine>
#include <exception>
#include <utility>

namespace mental1104 {

/// 独占协程帧的可移动任务；调用方通过完整 promise_type 编译协程。
class Task {
public:
  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;

  Task() noexcept : handle_(nullptr) {}
  explicit Task(handle_type h) : handle_(h) {}

  Task(Task &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }
  Task(const Task &) = delete;

  /// 销毁仍由本对象独占持有的协程帧。
  ~Task();

  /// 释放旧帧并接管源任务。
  /// @param other 转移帧所有权的任务，移动后为空。
  /// @return 当前任务引用。
  Task &operator=(Task &&other) noexcept;

  /// 查询任务是否为空或已执行完成。
  /// @return 空任务和完成任务为 true。
  bool done() const;

  /// 恢复一个尚未完成的协程；空任务或已完成任务保持不变。
  void resume();

  handle_type native_handle() const { return handle_; }

  explicit operator bool() const noexcept { return handle_ != nullptr; }

  /// 协程编译协议；返回类型与挂起语义必须在调用方可见。
  struct promise_type {
    /// 把当前 promise 对应的帧交给独占 Task。
    /// @return 独占当前帧的任务。
    Task get_return_object() { return Task{handle_type::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
  };

private:
  handle_type handle_;
};

} // namespace mental1104

#endif // __cplusplus >= 202002L

#endif // MENTAL1104_TASK_H
