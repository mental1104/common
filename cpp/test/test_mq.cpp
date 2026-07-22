#include "mental1104/mq/bridge.h"
#include "mental1104/mq/kafka.h"
#include "mental1104/mq/pulsar.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace mental1104::mq;

/// FakeConsumerBackend 生成的私有确认凭据。
class FakeReceipt : public Receipt {};

/// 只验证 Bridge 契约的 Producer backend，不模拟任何具体 MQ SDK。
/// workers 由 backend 持有并在 close 中 join，避免测试 goroutine/线程泄漏。
class FakeProducerBackend : public IProducerBackend {
public:
  FakeProducerBackend() : reject(false), duplicate(false), close_count(0) {}

  /// 析构时复用幂等 close，保证测试提前失败也回收线程。
  ~FakeProducerBackend() { close(); }

  /// 记录最后一条消息并按 reject 开关返回同步结果。
  SendResult send(const Message &message) override {
    last = message;
    return reject ? SendResult::failure(
                        MQError(ErrorCode::Backend, "send", "failed"))
                  : SendResult::success("sync-id");
  }

  /// 启动 worker 异步完成请求；duplicate 用于验证 Bridge 的 exactly-once 门禁。
  OperationResult send_async(const Message &message,
                             const DeliveryCallback &callback) override {
    last = message;
    if (reject)
      return OperationResult::failure(
          MQError(ErrorCode::Backend, "send_async", "rejected"));
    workers.push_back(std::thread([this, callback]() {
      callback(SendResult::success("async-id"));
      if (duplicate)
        callback(SendResult::success("duplicate"));
    }));
    return OperationResult::success();
  }

  /// 幂等语义由 Bridge 验证；本 fake 记录实际调用次数并 join 全部 worker。
  OperationResult close() override {
    ++close_count;
    for (std::size_t i = 0; i < workers.size(); ++i)
      if (workers[i].joinable())
        workers[i].join();
    return OperationResult::success();
  }

  bool reject;
  bool duplicate;
  std::atomic<int> close_count;
  Message last;
  std::vector<std::thread> workers;
};

/// 通过内存消息序列验证 Consumer Bridge 状态机的最小 backend。
class FakeConsumerBackend : public IConsumerBackend {
public:
  FakeConsumerBackend()
      : index(0), ack_count(0), nack_count(0), close_count(0) {}

  /// 依次返回 messages；耗尽后返回 Timeout，让 Bridge 保持轮询。
  ReceiveResult receive(int) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (index < messages.size()) {
      BackendMessage backend_message;
      backend_message.message = messages[index++];
      backend_message.receipt.reset(new FakeReceipt());
      return ReceiveResult::success(backend_message);
    }
    return ReceiveResult::failure(
        MQError(ErrorCode::Timeout, "receive", "timeout"));
  }

  /// 记录 ack 次数。
  OperationResult acknowledge(const ReceiptPtr &) override {
    ++ack_count;
    return OperationResult::success();
  }

  /// 记录 nack 次数。
  OperationResult negative_acknowledge(const ReceiptPtr &) override {
    ++nack_count;
    return OperationResult::success();
  }

  OperationResult unsubscribe() override { return OperationResult::success(); }
  OperationResult resubscribe() override { return OperationResult::success(); }

  /// 记录实际 close 次数。
  OperationResult close() override {
    ++close_count;
    return OperationResult::success();
  }

  std::vector<Message> messages;
  std::size_t index;
  std::atomic<int> ack_count, nack_count, close_count;
};

/// 验证公共 Message 只包含领域字段，并验证 Kafka/Pulsar topic 映射。
TEST(MessageQueueBridge, DomainDoesNotExposeSdkMessage) {
  Message message;
  message.topic = "events";
  message.key = make_record("key");
  message.payload = make_record("payload");
  message.headers["trace"] = "1";
  EXPECT_EQ("payload", record_to_string(message.payload));
  EXPECT_EQ("tenant.namespace.events",
            build_kafka_topic("tenant", "namespace", "events"));
  EXPECT_EQ("persistent://tenant/namespace/events",
            build_pulsar_topic("tenant", "namespace", "events"));
}

/// 验证同步 Producer 转发、结果映射、幂等关闭和关闭后拒绝发送。
TEST(MessageQueueBridge, ProducerForwardsResultAndClosesIdempotently) {
  std::shared_ptr<FakeProducerBackend> backend(new FakeProducerBackend());
  Producer producer(backend);
  Message message;
  message.payload = make_record("value");
  message.headers["trace"] = "1";
  SendResult result = producer.send(message);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ("sync-id", result.message_id);
  EXPECT_EQ("1", backend->last.headers["trace"]);
  EXPECT_TRUE(producer.close().ok);
  EXPECT_TRUE(producer.close().ok);
  EXPECT_EQ(1, backend->close_count.load());
  EXPECT_EQ(ErrorCode::Closed, producer.send(message).error.code);
}

/// 验证重复 backend completion 只触发一次用户 callback，且 callback 异常被隔离。
TEST(MessageQueueBridge, AsyncCallbackIsExactlyOnceAndPanicSafe) {
  std::shared_ptr<FakeProducerBackend> backend(new FakeProducerBackend());
  backend->duplicate = true;
  Producer producer(backend);
  std::atomic<int> calls(0);
  Message message;
  EXPECT_TRUE(producer.async()
                  .send_async(message,
                              [&](const SendResult &result) {
                                EXPECT_TRUE(result.ok);
                                ++calls;
                                throw std::runtime_error("ignored");
                              })
                  .ok);
  EXPECT_TRUE(producer.close().ok);
  for (int i = 0; i < 100 && calls.load() < 1; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_EQ(1, calls.load());
}

/// 验证 backend 同步拒绝时不调用 callback。
TEST(MessageQueueBridge, SynchronousAsyncRejectionDoesNotCallCallback) {
  std::shared_ptr<FakeProducerBackend> backend(new FakeProducerBackend());
  backend->reject = true;
  Producer producer(backend);
  std::atomic<int> calls(0);
  EXPECT_FALSE(producer.async()
                   .send_async(Message(), [&](const SendResult &) { ++calls; })
                   .ok);
  producer.close();
  EXPECT_EQ(0, calls.load());
}

/// 验证 Consumer 非阻塞启动、重复启动、stop/restart 及 handler 失败自动 nack。
TEST(MessageQueueBridge, ConsumerStartStopRestartAndHandlerFailure) {
  std::shared_ptr<FakeConsumerBackend> backend(new FakeConsumerBackend());
  Message one;
  one.payload = make_record("one");
  Message two;
  two.payload = make_record("two");
  backend->messages.push_back(one);
  backend->messages.push_back(two);
  Consumer consumer(backend);
  std::atomic<int> handled(0);
  EXPECT_TRUE(consumer
                  .start([&](const Message &message) {
                    ++handled;
                    if (record_to_string(message.payload) == "two")
                      throw std::runtime_error("fail");
                    return HandlerResult::acknowledge();
                  })
                  .ok);
  EXPECT_EQ(
      ErrorCode::AlreadyStarted,
      consumer
          .start([](const Message &) { return HandlerResult::acknowledge(); })
          .error.code);
  for (int i = 0; i < 100 && handled.load() < 2; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  EXPECT_TRUE(consumer.stop().ok);
  EXPECT_EQ(2, handled.load());
  EXPECT_EQ(1, backend->ack_count.load());
  EXPECT_EQ(1, backend->nack_count.load());
  EXPECT_TRUE(
      consumer
          .start([](const Message &) { return HandlerResult::acknowledge(); })
          .ok);
  EXPECT_TRUE(consumer.stop().ok);
  EXPECT_TRUE(consumer.close().ok);
  EXPECT_TRUE(consumer.close().ok);
  EXPECT_EQ(1, backend->close_count.load());
}

/// 验证缺少可选 native SDK 时返回统一 MQException，而非链接或空指针错误。
TEST(MessageQueueBridge, OptionalBackendsFailWithUnifiedError) {
  if (!kafka_available()) {
    ProducerConfig config;
    config.backend.reset(new KafkaBackendConfig());
    config.topic.topic = "events";
    EXPECT_THROW(create_kafka_producer_backend(
                     config, *static_cast<const KafkaBackendConfig *>(
                                 config.backend.get())),
                 MQException);
  }
  if (!pulsar_available()) {
    ProducerConfig config;
    config.backend.reset(new PulsarBackendConfig());
    config.topic.tenant = "t";
    config.topic.namespace_name = "n";
    config.topic.topic = "events";
    EXPECT_THROW(create_pulsar_producer_backend(
                     config, *static_cast<const PulsarBackendConfig *>(
                                 config.backend.get())),
                 MQException);
  }
}

} // namespace
