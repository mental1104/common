#include "mental1104/concurrency/thread/boost_asio_executor.h"

namespace mental1104 {

/// 建立由当前对象持有的线程池。
/// @param thread_count 池中的工作线程数量。
BoostAsioExecutor::BoostAsioExecutor(std::size_t thread_count)
    : pool_(thread_count) {}

/// 把固定签名的工作提交给内部线程池。
/// @param fn 转移到池中执行的可调用对象，不返回 future。
void BoostAsioExecutor::execute(std::function<void()> fn) {
  boost::asio::post(this->pool_, std::move(fn));
}

#if defined(M1104_HAS_ASYNC_SIMPLE)

/// 适配 async_simple 的任务提交协议。
/// @param func 转移到池中执行的任务。
/// @return 正常提交后返回 true；底层异常继续传播。
bool BoostAsioExecutor::schedule(async_simple::Executor::Func func) {
  boost::asio::post(this->pool_, std::move(func));
  return true;
}

#endif

} // namespace mental1104
