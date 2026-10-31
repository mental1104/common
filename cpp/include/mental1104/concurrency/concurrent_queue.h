#ifndef MENTAL1104_CONCURRENCY_CONCURRENT_QUEUE_H
#define MENTAL1104_CONCURRENCY_CONCURRENT_QUEUE_H

#include "mental1104/meta/compiler_support.h"

#if M1104_HAS_CXX17

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace mental1104 {

/// 提供进程内多生产者、多消费者的线程安全 FIFO 队列。
///
/// 容量为 0 时队列无界；正容量队列会在满时阻塞生产者。消费者在队列为空时阻塞，
/// 直到有元素可取或队列关闭。关闭后不再接受新元素，但消费者仍可取完已有元素。
/// 此类使用 C++17 的 std::optional，且不能复制或移动；销毁前必须停止并 join 所有调用线程。
/// 元素的构造、复制、移动和析构可能在持锁期间执行，T 的相关特殊成员函数不得重入此队列。
template <typename T>
class ConcurrentQueue {
public:
  /// 创建队列；容量为 0 表示无界队列。
  ///
  /// @param capacity 最大排队元素数，0 表示不限制队列容量。
  explicit ConcurrentQueue(std::size_t capacity = 0)
      : capacity_(capacity), closed_(false) {}

  /// 队列持有互斥锁和条件变量，复制或移动会破坏等待线程所依赖的对象身份。
  ConcurrentQueue(const ConcurrentQueue &) = delete;
  ConcurrentQueue &operator=(const ConcurrentQueue &) = delete;
  ConcurrentQueue(ConcurrentQueue &&) = delete;
  ConcurrentQueue &operator=(ConcurrentQueue &&) = delete;

  /// 等待队列有空间后原地构造元素；队列关闭时不构造元素并返回 false。
  ///
  /// @param args 转发给 T 构造函数的参数。
  /// @return 成功入队返回 true；队列已关闭返回 false。
  /// @note 构造 T 时持有队列互斥锁，因此 T 的构造函数不应重入此队列。
  template <typename... Args>
  bool emplace(Args &&...args) {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->not_full_.wait(lock, [this] {
      return this->closed_ || this->capacity_ == 0 ||
             this->queue_.size() < this->capacity_;
    });

    // close() 与入队操作共用同一把锁，保证关闭后不会再接受新元素。
    if (this->closed_) {
      return false;
    }

    try {
      this->queue_.emplace_back(std::forward<Args>(args)...);
    } catch (...) {
      // 构造失败时队列容量仍可用；继续唤醒其他生产者，避免等待线程滞留。
      lock.unlock();
      this->not_full_.notify_one();
      throw;
    }
    lock.unlock();
    this->not_empty_.notify_one();
    return true;
  }

  /// 等待队列有空间后复制元素入队；队列关闭时返回 false。
  ///
  /// @param value 要复制入队的元素。
  /// @return 成功入队返回 true；队列已关闭返回 false。
  bool push(const T &value) { return this->emplace(value); }

  /// 等待队列有空间后移动元素入队；队列关闭时返回 false。
  ///
  /// @param value 要移动入队的元素；入队成功后其状态由 T 的移动语义决定。
  /// @return 成功入队返回 true；队列已关闭返回 false。
  bool push(T &&value) { return this->emplace(std::move(value)); }

  /// 等待并取出队首元素；队列关闭且已排空时返回 std::nullopt。
  ///
  /// @return 成功取出时按值返回队首元素；仅在队列关闭且没有剩余元素时返回空值。
  /// @note 返回值需要 T 可复制或可移动构造；元素构造异常会向调用方传播。
  std::optional<T> pop() {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->not_empty_.wait(lock, [this] {
      return this->closed_ || !this->queue_.empty();
    });

    // 关闭只阻止新入队；队列内仍有元素时必须先排空，再向消费者报告结束。
    if (this->queue_.empty()) {
      return std::nullopt;
    }

    std::optional<T> item;
    try {
      item.emplace(std::move_if_noexcept(this->queue_.front()));
    } catch (...) {
      // 元素仍留在队首；继续唤醒其他消费者，让它们有机会完成读取。
      lock.unlock();
      this->not_empty_.notify_one();
      throw;
    }
    this->queue_.pop_front();
    lock.unlock();
    this->not_full_.notify_one();
    return item;
  }

  /// 关闭队列并唤醒所有等待中的生产者和消费者；重复调用没有副作用。
  ///
  /// 已排队元素不会被丢弃；后续 push/emplace 返回 false，pop 会继续排空元素。
  void close() {
    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      if (this->closed_) {
        return;
      }
      this->closed_ = true;
    }

    // 关闭可能同时解除“队列满”和“队列空”两种等待条件，因此必须唤醒双方所有线程。
    this->not_empty_.notify_all();
    this->not_full_.notify_all();
  }

  /// 返回当前排队元素数量；结果只是加锁期间的快照。
  std::size_t size() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->queue_.size();
  }

  /// 判断队列在当前快照中是否为空。
  bool empty() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->queue_.empty();
  }

  /// 返回队列是否已关闭。
  bool is_closed() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->closed_;
  }

  /// 返回构造时设置的容量；0 表示无界队列。
  std::size_t capacity() const noexcept { return this->capacity_; }

private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<T> queue_;
  bool closed_;
};

} // namespace mental1104

#endif // M1104_HAS_CXX17
#endif // MENTAL1104_CONCURRENCY_CONCURRENT_QUEUE_H
