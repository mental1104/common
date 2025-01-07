#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "mental1104/timed.h"  // 假设上面的代码保存在 timed.h 中

using ::testing::HasSubstr;

class TimedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 在测试前准备工作
  }

  void TearDown() override {
    // 在测试后清理工作
  }
};

// 一个简单的函数：用于返回一个加法结果
int add(int a, int b) { return a + b; }

// 一个简单的函数：用于测试 void 返回值
void dummy_task(int delay_ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}

// 测试函数返回值正确性
TEST_F(TimedTest, FunctionReturnsCorrectValue) {
  auto timed_add = mental1104::make_timed(add, "AddFunction");
  EXPECT_EQ(timed_add(2, 3), 5);  // 验证返回值
}

// 测试 void 返回值函数的正确性
TEST_F(TimedTest, VoidFunctionExecutesCorrectly) {
  auto timed_dummy = mental1104::make_timed(dummy_task, "VoidFunction");
  timed_dummy(100);  // 不验证返回值，仅验证不抛异常
}

// 测试时间测量功能
TEST_F(TimedTest, MeasuresExecutionTime) {
  auto timed_dummy = mental1104::make_timed(dummy_task, "TimedTask");

  // 捕获标准输出
  testing::internal::CaptureStdout();
  timed_dummy(100);
  std::string output = testing::internal::GetCapturedStdout();

  // 验证输出日志包含时间
  EXPECT_THAT(output, HasSubstr("Entering TimedTask"));
  EXPECT_THAT(output, HasSubstr("Exiting TimedTask with "));
}

// 测试辅助函数是否正确包装
TEST_F(TimedTest, MakeTimedHelperWorks) {
  auto timed_add = mental1104::make_timed(add, "AddHelper");
  EXPECT_EQ(timed_add(10, 20), 30);
}

// 测试多线程场景
TEST_F(TimedTest, ThreadSafetyTest) {
  auto timed_add = mental1104::make_timed(add, "ThreadSafeAdd");

  std::thread t1([&]() { EXPECT_EQ(timed_add(5, 5), 10); });
  std::thread t2([&]() { EXPECT_EQ(timed_add(10, 20), 30); });

  t1.join();
  t2.join();
}
