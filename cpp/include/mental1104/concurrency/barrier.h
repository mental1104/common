#ifndef MENTAL1104_CONCURRENCY_BARRIER_H
#define MENTAL1104_CONCURRENCY_BARRIER_H

#include "mental1104/meta/barrier_support.h"

#include <cstddef>
#include <utility>

#if !M1104_HAS_STD_BARRIER
#include <cassert>
#include <condition_variable>
#include <limits>
#include <mutex>
#endif

namespace mental1104 {
namespace detail {

/// 为 barrier 提供默认的空 completion function。
///
/// 该回调不维护状态且保证 noexcept，使默认 barrier 在每个 phase 完成时
/// 无需执行额外工作。
struct empty_barrier_completion {
  /// 完成当前 phase，不产生任何副作用。
  void operator()() noexcept {}
};

} // namespace detail

#if M1104_HAS_STD_BARRIER

/// 使用标准库 std::barrier 提供可循环复用的线程会合点。
///
/// 仅当编译器、标准库头文件和特性测试宏共同证明实现完整时启用该别名；
/// 其他环境使用下方 C++11 fallback，调用方无需按语言版本切换接口。
///
/// @tparam CompletionFunction 每个 phase 完成时调用一次的无参 noexcept
/// 回调类型。
template <class CompletionFunction = detail::empty_barrier_completion>
using barrier = std::barrier<CompletionFunction>;

#else

#if M1104_HAS_CXX17
#define M1104_BARRIER_NODISCARD [[nodiscard]]
#else
#define M1104_BARRIER_NODISCARD
#endif

/// C++11 兼容的可循环复用线程屏障，公共操作与 std::barrier 保持一致。
///
/// 所有 phase 状态由同一把 mutex 保护；最后一个到达者在锁内执行
/// completion function、推进 phase 并重置下一轮计数，随后唤醒等待线程。
/// 同一个对象允许多个参与线程并发调用，但析构前调用方必须保证不再有线程
/// 等待或访问该对象。
///
/// 违反 expected、update、arrival_token 归属等标准前置条件时，Debug 构建
/// 通过 assert 暴露错误；Release 构建行为与 std::barrier 一样未定义。
///
/// @tparam CompletionFunction 每个 phase 完成时调用一次的无参 noexcept
/// 回调类型。
template <class CompletionFunction = detail::empty_barrier_completion>
class barrier {
public:
  /// 表示一次 arrive() 所属 barrier 与 phase 的一次性等待凭证。
  ///
  /// token 只能移动，不能复制；wait() 消费 token 后会清空其 owner，避免同一
  /// 次到达被重复等待。token 不拥有 barrier，调用方必须保证 barrier 的生命
  /// 周期覆盖 token 的创建、移动和消费过程。
  class arrival_token {
  public:
    /// 转移另一个 token 的等待凭证，并使源 token 失效。
    ///
    /// @param other 待移动的 token；调用完成后其 owner 被清空。
    arrival_token(arrival_token &&other) noexcept
        : owner_(other.owner_), phase_(other.phase_) {
      other.owner_ = NULL;
    }

    /// 释放当前保存的凭证，并接管另一个 token 的等待凭证。
    ///
    /// @param other 待移动的 token；调用完成后其 owner 被清空。
    /// @return 当前 token 的引用。
    arrival_token &operator=(arrival_token &&other) noexcept {
      if (this != &other) {
        this->owner_ = other.owner_;
        this->phase_ = other.phase_;
        other.owner_ = NULL;
      }
      return *this;
    }

    /// arrival_token 是一次性凭证，禁止复制后被多个调用点重复消费。
    arrival_token(const arrival_token &) = delete;
    arrival_token &operator=(const arrival_token &) = delete;

  private:
    friend class barrier;

    /// 创建与指定 barrier 当前 phase 绑定的等待凭证。
    ///
    /// @param owner 创建该 token 的 barrier；仅借用指针，不转移所有权。
    /// @param phase arrive() 发生时的 phase 编号。
    arrival_token(const barrier *owner, std::size_t phase) noexcept
        : owner_(owner), phase_(phase) {}

    /// 创建该凭证的 barrier；wait() 消费后置为 NULL。
    const barrier *owner_;
    /// arrive() 发生时的 phase，用于判断该轮是否已经完成。
    std::size_t phase_;
  };

  /// 返回 fallback 可表示的最大初始参与者数量。
  ///
  /// @return std::ptrdiff_t 的最大正值。
  static constexpr std::ptrdiff_t max() noexcept {
    return (std::numeric_limits<std::ptrdiff_t>::max)();
  }

  /// 创建具有固定初始参与者数量的可循环复用屏障。
  ///
  /// @param expected 首个 phase 需要的到达计数，必须位于 [1, max()]。
  /// @param completion 每个 phase 最后一次到达时同步执行一次的回调；回调在
  /// barrier 内部 mutex 的保护下运行，必须满足 noexcept。
  explicit barrier(std::ptrdiff_t expected,
                   CompletionFunction completion = CompletionFunction())
      : expected_(expected), remaining_(expected), phase_(0),
        completion_(std::move(completion)) {
    // completion 发生在同步原语内部，若异常越过该边界会破坏 phase 状态。
    static_assert(noexcept(std::declval<CompletionFunction &>()()),
                  "barrier completion must be noexcept");
    assert(expected > 0);
    assert(expected <= max());
  }

  /// 销毁屏障；调用方必须先结束所有等待和并发访问。
  ~barrier() {}

  /// barrier 持有 mutex、condition_variable 和 phase 状态，禁止复制或移动。
  barrier(const barrier &) = delete;
  barrier &operator=(const barrier &) = delete;
  barrier(barrier &&) = delete;
  barrier &operator=(barrier &&) = delete;

  /// 为当前 phase 提交一个或多个到达计数，并返回可供 wait() 消费的 token。
  ///
  /// 当本次提交使 remaining 归零时，当前调用线程会同步执行 completion、推进
  /// phase 并唤醒等待者；因此该方法可能包含回调执行开销。
  ///
  /// @param update 本次提交的到达计数，必须位于 [1, 当前 remaining]。
  /// @return 绑定提交前 phase 的移动专用 token；必须传给同一 barrier 的
  /// wait()，也可以直接销毁以只到达而不等待。
  M1104_BARRIER_NODISCARD arrival_token
  arrive(std::ptrdiff_t update = 1) {
    std::unique_lock<std::mutex> lock(this->mutex_);
    assert(update > 0);
    assert(update <= this->remaining_);

    // 必须在可能推进 phase 前保存编号，确保最后一个到达者拿到的 token 仍然
    // 表示“刚刚完成的 phase”，wait() 因此可以立即返回。
    const std::size_t arrival_phase = this->phase_;
    this->remaining_ -= update;
    if (this->remaining_ == 0) {
      this->complete_phase(lock);
    }

    return arrival_token(this, arrival_phase);
  }

  /// 等待 token 所属 phase 完成，并消费该 token。
  ///
  /// condition_variable 允许伪唤醒，因此等待条件使用 phase 是否变化，而不是
  /// 单次通知事件。phase 已经推进时立即返回。
  ///
  /// @param arrival 由同一 barrier 的 arrive() 返回的 token；所有权转入本
  /// 方法，返回后 token 失效且不得再次等待。
  void wait(arrival_token &&arrival) const {
    assert(arrival.owner_ == this);

    std::unique_lock<std::mutex> lock(this->mutex_);
    const std::size_t arrival_phase = arrival.phase_;
    // 在进入等待前消费 token，避免调用方再次使用同一到达凭证。
    arrival.owner_ = NULL;

    this->condition_.wait(lock, [this, arrival_phase] {
      return this->phase_ != arrival_phase;
    });
  }

  /// 为当前 phase 到达一次，并阻塞到该 phase 完成。
  void arrive_and_wait() {
    arrival_token arrival = this->arrive();
    this->wait(std::move(arrival));
  }

  /// 为当前 phase 到达一次，并永久减少后续 phase 的参与者数量。
  ///
  /// expected 与 remaining 在同一临界区内同步减少；若本次 drop 是当前 phase
  /// 的最后一次到达，当前线程负责完成该 phase。调用后本线程不等待，也不应再
  /// 参与该 barrier 的后续 phase。
  void arrive_and_drop() {
    std::unique_lock<std::mutex> lock(this->mutex_);
    assert(this->expected_ > 0);
    assert(this->remaining_ > 0);

    --this->expected_;
    --this->remaining_;
    if (this->remaining_ == 0) {
      this->complete_phase(lock);
    }
  }

private:
  /// 完成当前 phase，重置下一轮状态并唤醒所有等待者。
  ///
  /// completion 必须在 phase 发布前且 mutex 仍被持有时执行，才能保证每轮
  /// 恰好调用一次，并让等待线程在观察到新 phase 时同时观察到回调和前序参与
  /// 者的写入。状态更新完成后先解锁再 notify_all，避免被唤醒线程立即争用仍被
  /// 当前线程占用的 mutex。
  ///
  /// @param lock 已持有 mutex_ 的 unique_lock；本方法会主动解锁，调用方返回
  /// 后不得再假设它仍拥有锁。
  void complete_phase(std::unique_lock<std::mutex> &lock) {
    this->completion_();
    ++this->phase_;
    this->remaining_ = this->expected_;

    lock.unlock();
    this->condition_.notify_all();
  }

  /// 保护 expected_、remaining_、phase_ 和 completion_ 的互斥量。
  mutable std::mutex mutex_;
  /// 等待 phase_ 发生变化的条件变量。
  mutable std::condition_variable condition_;
  /// 每个新 phase 需要的参与者数量，可由 arrive_and_drop() 永久减少。
  std::ptrdiff_t expected_;
  /// 当前 phase 尚未提交的到达计数。
  std::ptrdiff_t remaining_;
  /// 当前 phase 编号；递增即表示上一 phase 已完成并可释放等待者。
  std::size_t phase_;
  /// 最后一个到达者在推进每个 phase 前执行的回调。
  CompletionFunction completion_;
};

#undef M1104_BARRIER_NODISCARD

#endif

} // namespace mental1104

#endif // MENTAL1104_CONCURRENCY_BARRIER_H
