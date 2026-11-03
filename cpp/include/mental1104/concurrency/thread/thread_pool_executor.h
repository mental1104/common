// include/mental1104/concurrency/thread/thread_pool_executor.h
#ifndef MENTAL1104_THREAD_POOL_EXECUTOR_H
#define MENTAL1104_THREAD_POOL_EXECUTOR_H

#pragma once

#include <utility>

// 条件基类决定对象布局；运行时适配器必须读取其链接库生成的配置。
#include "mental1104/build_config.h"
#include "mental1104/meta/compiler_support.h"

#if defined(M1104_HAS_ASYNC_SIMPLE)
#include "async_simple/Executor.h"
#endif

#include "mental1104/concurrency/executor.h"
#include "mental1104/concurrency/thread/thread_util.h"

namespace mental1104 {

// 线程池执行器：内部自带 ThreadPool，既实现 IExecutor，也在可选时实现
// async_simple::Executor
/// 持有具体线程池的非模板执行器；运行时实现由 mental1104 库提供。
class ThreadPoolExecutor
#if defined(M1104_HAS_ASYNC_SIMPLE)
    : public async_simple::Executor,
      public IExecutor
#else
    : public IExecutor
#endif
{
public:
  /// 建立由当前对象持有的线程池。
  /// @param thread_count 池中的工作线程数量。
  explicit ThreadPoolExecutor(std::size_t thread_count);

  /// 把固定签名的工作提交给内部线程池。
  /// @param fn 转移到池中执行的可调用对象，不返回 future。
  void execute(std::function<void()> fn) override;

#if defined(M1104_HAS_ASYNC_SIMPLE)
  /// 适配 async_simple 的任务提交协议。
  /// @param func 转移到池中执行的任务。
  /// @return 正常提交后返回 true；底层异常继续传播。
  bool schedule(async_simple::Executor::Func func) override;

  using async_simple::Executor::schedule; // 继承可选的重载
#endif

  ::ThreadPool &underlying_pool() { return pool_; }

private:
  ::ThreadPool pool_;
};

} // namespace mental1104

#endif // MENTAL1104_THREAD_POOL_EXECUTOR_H
