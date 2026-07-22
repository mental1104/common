#include "mental1104/mq/kafka.h"
#include "mental1104/mq/pulsar.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <sstream>
#include <string>

namespace {
using namespace mental1104::mq;

/// 读取非空环境变量。
///
/// @param name 环境变量名称，必须是以空字符结尾的字符串。
/// @return 环境变量存在且非空时返回其值；否则返回空字符串。
std::string env_value(const char *name) {
  const char *value = std::getenv(name);
  return value && value[0] != '\0' ? std::string(value) : std::string();
}

/// 生成进程内足够唯一的测试订阅名称，避免复用历史消费位点。
///
/// @param prefix 订阅名称前缀，用于区分 Kafka 和 Pulsar 测试。
/// @return 由前缀和高精度时钟计数组成的新订阅名称。
std::string unique_subscription(const std::string &prefix) {
  std::ostringstream value;
  value << prefix << "-"
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();
  return value.str();
}

/// 从 Consumer 中等待指定 payload，并确认期间遇到的其他历史消息。
///
/// @param consumer 已连接真实中间件的 Consumer。
/// @param expected_payload 当前测试发送的唯一 payload。
/// @param attempts 最多执行的单次一秒 receive 次数，必须大于零。
/// @return 收到目标消息时返回其共享指针；超时后返回空指针。
MessagePtr receive_expected(Consumer &consumer,
                            const std::string &expected_payload, int attempts) {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    try {
      MessagePtr message = consumer.receive(1000);
      if (record_to_string(message->payload) == expected_payload)
        return message;

      // 测试 topic 可能残留旧消息；确认后继续等待本轮唯一 payload。
      consumer.acknowledge(message);
    } catch (const MQException &error) {
      if (error.error().code != ErrorCode::Timeout)
        throw;
    }
  }
  return MessagePtr();
}

/// 验证异步 delivery callback 可以关闭同一个 Producer，而不会形成自锁。
///
/// @param producer 已连接真实中间件的 Producer。
/// @param payload 本次异步发送的唯一消息内容。
void expect_async_callback_can_close(Producer &producer,
                                     const std::string &payload) {
  std::promise<OperationResult> closed;
  std::future<OperationResult> completed = closed.get_future();
  Message message;
  message.payload = make_record(payload);

  OperationResult accepted = producer.async().send_async(
      message, [&producer, &closed](const SendResult &result) {
        closed.set_value(result.ok ? producer.close()
                                   : OperationResult::failure(result.error));
      });

  ASSERT_TRUE(accepted.ok) << accepted.error.message;
  ASSERT_EQ(std::future_status::ready,
            completed.wait_for(std::chrono::seconds(15)));
  EXPECT_TRUE(completed.get().ok);
  EXPECT_TRUE(producer.close().ok);
}

/// KafkaIntegrationTest 只在 SDK 和真实 Kafka 测试环境完整时运行。
class KafkaIntegrationTest : public ::testing::Test {
protected:
  /// 检查 SDK、连接地址和预建测试 topic；缺失任一条件时跳过当前用例。
  void SetUp() override {
    if (!kafka_available())
      GTEST_SKIP() << "librdkafka is not available in this build";

    const std::string host = env_value("KAFKA_ADVERTISED_HOST");
    const std::string port = env_value("KAFKA_EXTERNAL_PORT");
    topic_ = env_value("KAFKA_TEST_TOPIC");
    if (host.empty() || port.empty() || topic_.empty()) {
      GTEST_SKIP() << "Kafka integration requires KAFKA_ADVERTISED_HOST, "
                      "KAFKA_EXTERNAL_PORT and KAFKA_TEST_TOPIC";
    }
    bootstrap_servers_ = host + ":" + port;
  }

  /// 创建只包含真实 Kafka 连接参数的 backend 配置。
  ///
  /// @param consumer 是否为 Consumer 配置；Consumer 会额外从 earliest 开始读取。
  /// @return 可传给 Kafka backend 工厂的配置值。
  KafkaBackendConfig kafka_backend(bool consumer) const {
    KafkaBackendConfig backend;
    backend.options["bootstrap.servers"] = bootstrap_servers_;
    if (consumer)
      backend.options["auto.offset.reset"] = "earliest";
    return backend;
  }

  std::string bootstrap_servers_;
  std::string topic_;
};

/// PulsarIntegrationTest 只在 SDK 和真实 Pulsar 测试环境完整时运行。
class PulsarIntegrationTest : public ::testing::Test {
protected:
  /// 检查 SDK、连接地址和预建测试 topic；缺失任一条件时跳过当前用例。
  void SetUp() override {
    if (!pulsar_available())
      GTEST_SKIP() << "pulsar-client-cpp is not available in this build";

    const std::string host = env_value("PULSAR_HOST");
    const std::string port = env_value("PULSAR_BROKER_PORT");
    tenant_ = env_value("PULSAR_TEST_TENANT");
    namespace_ = env_value("PULSAR_TEST_NAMESPACE");
    topic_ = env_value("PULSAR_TEST_TOPIC");
    if (host.empty() || port.empty() || tenant_.empty() || namespace_.empty() ||
        topic_.empty()) {
      GTEST_SKIP() << "Pulsar integration requires PULSAR_HOST, "
                      "PULSAR_BROKER_PORT, PULSAR_TEST_TENANT, "
                      "PULSAR_TEST_NAMESPACE and PULSAR_TEST_TOPIC";
    }
    service_url_ = "pulsar://" + host + ":" + port;
  }

  /// 创建只包含真实 Pulsar service URL 的 backend 配置。
  ///
  /// @return 可传给 Pulsar backend 工厂的配置值。
  PulsarBackendConfig pulsar_backend() const {
    PulsarBackendConfig backend;
    backend.service_url = service_url_;
    return backend;
  }

  std::string service_url_;
  std::string tenant_;
  std::string namespace_;
  std::string topic_;
};

/// 验证公共 Message 只包含领域字段，并验证 Kafka/Pulsar topic 映射。
TEST(MessageQueueDomain, BuildsBackendTopicNames) {
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

/// 使用真实 Kafka 完成生产、消费和确认闭环。
TEST_F(KafkaIntegrationTest, ProducesConsumesAndAcknowledges) {
  KafkaBackendConfig producer_backend = this->kafka_backend(false);
  KafkaBackendConfig consumer_backend = this->kafka_backend(true);

  ProducerConfig producer_config;
  producer_config.topic.topic = this->topic_;
  ConsumerConfig consumer_config;
  consumer_config.topic.topic = this->topic_;
  consumer_config.subscription = unique_subscription("common-cpp-kafka");

  Consumer consumer(
      create_kafka_consumer_backend(consumer_config, consumer_backend));
  Producer producer(
      create_kafka_producer_backend(producer_config, producer_backend));

  const std::string payload = unique_subscription("kafka-payload");
  Message message;
  message.payload = make_record(payload);
  message.headers["source"] = "common-cpp-test";

  SendResult sent = producer.send(message);
  ASSERT_TRUE(sent.ok) << sent.error.message;

  MessagePtr received = receive_expected(consumer, payload, 15);
  ASSERT_TRUE(static_cast<bool>(received));
  EXPECT_EQ("common-cpp-test", received->headers["source"]);
  consumer.acknowledge(received);

  EXPECT_TRUE(producer.close().ok);
  EXPECT_TRUE(consumer.close().ok);
}

/// 使用真实 Kafka 验证异步 callback 内关闭 Producer 的生命周期边界。
TEST_F(KafkaIntegrationTest, AsyncCallbackMayCloseProducer) {
  KafkaBackendConfig backend = this->kafka_backend(false);
  ProducerConfig config;
  config.topic.topic = this->topic_;
  Producer producer(create_kafka_producer_backend(config, backend));

  expect_async_callback_can_close(
      producer, unique_subscription("kafka-async-close"));
}

/// 使用真实 Pulsar 完成生产、消费和确认闭环。
TEST_F(PulsarIntegrationTest, ProducesConsumesAndAcknowledges) {
  PulsarBackendConfig producer_backend = this->pulsar_backend();
  PulsarBackendConfig consumer_backend = this->pulsar_backend();

  ProducerConfig producer_config;
  producer_config.topic.tenant = this->tenant_;
  producer_config.topic.namespace_name = this->namespace_;
  producer_config.topic.topic = this->topic_;
  ConsumerConfig consumer_config;
  consumer_config.topic = producer_config.topic;
  consumer_config.subscription = unique_subscription("common-cpp-pulsar");

  Consumer consumer(
      create_pulsar_consumer_backend(consumer_config, consumer_backend));
  Producer producer(
      create_pulsar_producer_backend(producer_config, producer_backend));

  const std::string payload = unique_subscription("pulsar-payload");
  Message message;
  message.payload = make_record(payload);
  message.headers["source"] = "common-cpp-test";

  SendResult sent = producer.send(message);
  ASSERT_TRUE(sent.ok) << sent.error.message;

  MessagePtr received = receive_expected(consumer, payload, 15);
  ASSERT_TRUE(static_cast<bool>(received));
  EXPECT_EQ("common-cpp-test", received->headers["source"]);
  consumer.acknowledge(received);

  EXPECT_TRUE(producer.close().ok);
  EXPECT_TRUE(consumer.close().ok);
}

/// 使用真实 Pulsar 验证异步 callback 内关闭 Producer 的生命周期边界。
TEST_F(PulsarIntegrationTest, AsyncCallbackMayCloseProducer) {
  PulsarBackendConfig backend = this->pulsar_backend();
  ProducerConfig config;
  config.topic.tenant = this->tenant_;
  config.topic.namespace_name = this->namespace_;
  config.topic.topic = this->topic_;
  Producer producer(create_pulsar_producer_backend(config, backend));

  expect_async_callback_can_close(
      producer, unique_subscription("pulsar-async-close"));
}

/// 未编译可选 SDK 时，工厂应返回统一配置异常而不是链接或空指针错误。
TEST(MessageQueueAvailability, MissingSdkFailsWithUnifiedError) {
  if (!kafka_available()) {
    ProducerConfig config;
    KafkaBackendConfig backend;
    EXPECT_THROW(create_kafka_producer_backend(config, backend), MQException);
  }
  if (!pulsar_available()) {
    ProducerConfig config;
    PulsarBackendConfig backend;
    EXPECT_THROW(create_pulsar_producer_backend(config, backend), MQException);
  }
}

} // namespace
