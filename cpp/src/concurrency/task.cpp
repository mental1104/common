#include "mental1104/concurrency/coroutine/task.h"

#if M1104_HAS_CXX20
namespace mental1104 {

/// 销毁仍由本对象独占持有的协程帧。
Task::~Task() {
  if (this->handle_) {
    this->handle_.destroy();
  }
}

/// 释放旧帧并接管源任务。
/// @param other 转移帧所有权的任务，移动后为空。
/// @return 当前任务引用。
Task &Task::operator=(Task &&other) noexcept {
  if (this != &other) {
    if (this->handle_) {
      this->handle_.destroy();
    }
    this->handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

/// 查询任务是否为空或已执行完成。
/// @return 空任务和完成任务为 true。
bool Task::done() const { return !this->handle_ || this->handle_.done(); }

/// 恢复一个尚未完成的协程；空任务或已完成任务保持不变。
void Task::resume() {
  if (this->handle_ && !this->handle_.done()) {
    this->handle_.resume();
  }
}

} // namespace mental1104
#endif
