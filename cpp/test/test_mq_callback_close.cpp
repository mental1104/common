#include "mental1104/mq/bridge.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace {
using namespace mental1104::mq;

class CallbackCloseBackend : public IProducerBackend {
public:
  CallbackCloseBackend() : closed_(false) {}
  ~CallbackCloseBackend() {
    if (worker_.joinable())
      worker_.join();
  }

  SendResult send(const Message &) override {
    return SendResult::success("sync");
  }

  OperationResult send_async(const Message &,
                             const DeliveryCallback &callback) override {
    worker_ = std::thread(
        [callback]() { callback(SendResult::success("async")); });
    return OperationResult::success();
  }

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
