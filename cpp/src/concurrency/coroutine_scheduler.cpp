#include "mental1104/concurrency/coroutine/coroutine_scheduler.h"

#if M1104_HAS_CXX20
namespace mental1104 {

/// 借用执行器并提交长期运行的调度循环。
/// @param executor 必须活到调度工作退出，调用方负责其生命周期。
/// @param scheduler_workers 调度循环数量，0 按 1 处理。
BasicCoroutineScheduler::BasicCoroutineScheduler(IExecutor &executor,
                                                 std::size_t scheduler_workers)
    : executor_(executor), stopping_(false), pending_(0) {
  this->start(scheduler_workers);
}

/// 请求调度循环停止；沿用执行器负责工作线程回收的现有约定。
BasicCoroutineScheduler::~BasicCoroutineScheduler() { this->stop(); }

/// 接管任务帧并加入受锁保护的就绪队列。
/// @param t 转移所有权的协程任务。
void BasicCoroutineScheduler::spawn_task(Task t) {
  {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->ready_.push(std::move(t));
    ++this->pending_;
  }
}

/// 阻塞到 pending 归零；不主动停止调度循环。
void BasicCoroutineScheduler::wait_all() {
  using namespace std::chrono_literals;
  while (this->pending_.load(std::memory_order_acquire) > 0) {
    std::this_thread::sleep_for(1ms);
  }
}

/// 幂等设置停止状态，供调度循环观察。
void BasicCoroutineScheduler::stop() {
  bool expected = false;
  if (!this->stopping_.compare_exchange_strong(expected, true)) {
    return; // 已经停过
  }
}

/// 向借用的执行器提交调度循环。
/// @param workers 循环数量，0 按 1 处理。
void BasicCoroutineScheduler::start(std::size_t workers) {
  if (workers == 0)
    workers = 1;
  for (std::size_t i = 0; i < workers; ++i) {
    this->executor_.execute([this] { this->scheduler_loop(); });
  }
}

/// 在执行器线程中推进任务，完成时减少 pending，挂起时重新入队。
void BasicCoroutineScheduler::scheduler_loop() {
  using namespace std::chrono_literals;

  while (!this->stopping_.load(std::memory_order_acquire)) {
    Task t;

    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      if (!this->ready_.empty()) {
        t = std::move(this->ready_.front());
        this->ready_.pop();
      }
    }

    if (!t) {
      std::this_thread::sleep_for(1ms);
      continue;
    }

    t.resume();

    if (t.done()) {
      this->pending_.fetch_sub(1, std::memory_order_acq_rel);
    } else {
      std::lock_guard<std::mutex> lock(this->mutex_);
      this->ready_.push(std::move(t));
    }
  }
}

} // namespace mental1104
#endif
