#include "mental1104/mq/bridge.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace {
using namespace mental1104::mq;

/// 专门复现“异步 callback 内关闭同一 Producer”边界的 backend。
/// worker_ 由 backend 持有；close 避免 join 当前线程，防止 callback 自锁。
class CallbackCloseBackend : public IProducerBackend {
public:
  CallbackCloseBackend() : closed_(false) {}

  /// 测试提前退出时回收 worker_。
  ~CallbackCloseBackend() {
    if (worker_.joinable())
      worker_.join();
  }

  /// 返回固定同步结果；本测试只关注异步关闭路径。
  SendResult send(const Message &) override {
    return SendResult::success("sync");
  }

  /// 在独立 worker 中调用一次 callback。
  OperationResult send_async(const Message &,
                             const DeliveryCallback &callback) override {
    worker_ = std::thread(
        [callback]() { callback(SendResult::success("async")); });
    return OperationResult::success();
  }

  /// 幂等关闭；非 worker 线程负责 join，worker 自身调用时留给析构回收。
  OperationResult close() override {
    if (closed_.exchange(true))
      return OperationResult::success();
    if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id())
      worker_.join();
    return OperationResult::success();
  }

private:
  std::atomic<bool> closed_;
  std::thread worker_;
};

/// 验证用户 callback 可以关闭共享 Producer，而不会与 completion/pending 计数死锁。
TEST(MessageQueueBridge, AsyncCallbackMayCloseTheSameProducer) {
  Producer producer(
      std::unique_ptr<IProducerBackend>(new CallbackCloseBackend()));
  std::promise<OperationResult> closed;
  std::future<OperationResult> completed = closed.get_future();

  OperationResult accepted = producer.async().send_async(
      Message(), [&producer, &closed](const SendResult &result) {
        EXPECT_TRUE(result.ok);
        closed.set_value(producer.close());
      });
  ASSERT_TRUE(accepted.ok);
  ASSERT_EQ(std::future_status::ready,
            completed.wait_for(std::chrono::seconds(1)));
  EXPECT_TRUE(completed.get().ok);
  EXPECT_TRUE(producer.close().ok);
}

} // namespace
