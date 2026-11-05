#include "linkage_consumer.h"

#include "mental1104/c_api_raii.h"

#include <gtest/gtest.h>

/// 独立消费端的私有模板类型与已编译方法可以正常组合，并保持行为。
TEST(CompiledLinkage, CallerTemplatesUseRuntimeCore) {
  const auto observed = linkage_test::observe_consumer();
  EXPECT_EQ("mixed", observed.normalized);
  EXPECT_EQ(42, observed.computed);
  EXPECT_TRUE(observed.membership);
  EXPECT_TRUE(observed.parse_ok);
  EXPECT_TRUE(observed.owns_file);
  EXPECT_TRUE(observed.has_random_key);
  EXPECT_EQ(10, observed.backoff_millis);
#if M1104_EXPECTED_CXX_STANDARD >= 17
  EXPECT_TRUE(observed.uses_std_string_view);
#else
  EXPECT_FALSE(observed.uses_std_string_view);
#endif
#if M1104_EXPECTED_CXX_STANDARD >= 20
  EXPECT_EQ(2, observed.coroutine_steps);
#else
  EXPECT_EQ(0, observed.coroutine_steps);
#endif
}

/// 主测试与独立消费端观察相同的日志等级，过滤在构造消息前生效。
TEST(CompiledLinkage, LogLevelIsSharedAcrossTranslationUnits) {
  const auto previous = mental1104::get_log_level();
  auto restore = mental1104::make_scope_exit(
      [previous] { mental1104::set_log_level(previous); });
  mental1104::set_log_level(mental1104::LogLevel::Error);
  EXPECT_EQ(mental1104::LogLevel::Error, linkage_test::consumer_log_level());
  testing::internal::CaptureStdout();
  linkage_test::write_consumer_log();
  EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
}
