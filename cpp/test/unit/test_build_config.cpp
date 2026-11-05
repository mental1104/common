#include "mental1104/meta/compiler_support.h"

#ifndef M1104_BUILD_HAS_ASYNC_SIMPLE
#error "public headers must load the generated mental1104 build configuration"
#endif

#include "mental1104/concurrency/thread/thread_pool_executor.h"

#include <future>
#include <gtest/gtest.h>

/// 未传播 target 编译宏时，公共头仍须取得与库一致的条件基类布局。
TEST(CompiledBuildConfig,
     HeadersRecoverExecutorLayoutWithoutTargetDefinitions) {
#if M1104_BUILD_HAS_ASYNC_SIMPLE
  EXPECT_TRUE((std::is_base_of<async_simple::Executor,
                               mental1104::ThreadPoolExecutor>::value));
#else
#ifdef M1104_HAS_ASYNC_SIMPLE
#error "disabled async_simple must not change the public executor layout"
#endif
#endif
  std::promise<int> result;
  auto future = result.get_future();
  mental1104::ThreadPoolExecutor executor(1);
  // promise 保持存活到 future 返回；此调用也覆盖构造与析构的跨库布局。
  executor.execute([&result] { result.set_value(7); });
  EXPECT_EQ(7, future.get());
}
