// include/mental1104/concurrency/coroutine/task.h
#ifndef MENTAL1104_TASK_H
#define MENTAL1104_TASK_H

#pragma once

#if __cplusplus >= 202002L

#include <coroutine>
#include <utility>

namespace mental1104 {

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

  bool done() const { return !handle_ || handle_.done(); }

  void resume() {
    if (handle_ && !handle_.done()) {
      handle_.resume();
    }
  }

  handle_type native_handle() const { return handle_; }

  explicit operator bool() const noexcept { return handle_ != nullptr; }

  struct promise_type {
    auto get_return_object() { return Task{handle_type::from_promise(*this)}; }
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
