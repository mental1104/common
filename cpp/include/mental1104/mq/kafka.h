#pragma once

#include "mental1104/mq/abstract_message_queue.h"

namespace mental1104 {
namespace mq {

/// librdkafka 专属配置。
/// options 直接映射 librdkafka 配置键值，但 SDK 对象不会进入公共 API。
struct KafkaBackendConfig : public BackendConfig {
  Options options;
  int close_timeout_millis;
  /// 默认关闭等待为后端实现规定值。
  KafkaBackendConfig();
  /// @return BackendType::Kafka。
  BackendType backend_type() const override;
};

/// @return 当前构建是否检测并链接了 librdkafka C++ 客户端。
bool kafka_available();
/// 创建 Kafka Producer backend；调用方取得唯一所有权。
std::unique_ptr<IProducerBackend>
create_kafka_producer_backend(const ProducerConfig &config,
                              const KafkaBackendConfig &backend);
/// 创建 Kafka Consumer backend；调用方取得唯一所有权。
std::unique_ptr<IConsumerBackend>
create_kafka_consumer_backend(const ConsumerConfig &config,
                              const KafkaBackendConfig &backend);

/// 第一版 API 的 Kafka 兼容工厂 facade。
class KafkaMessageQueue : public AbstractMessageQueue {
public:
  /// @param config librdkafka 全局配置键值。
  explicit KafkaMessageQueue(const Options &config = Options());
  /// 尽力幂等关闭，不向外抛异常。
  ~KafkaMessageQueue() noexcept;
  /// 创建 Kafka Producer Bridge；schema 当前仅作为兼容扩展位。
  std::shared_ptr<Producer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) override;
  /// 创建 Kafka Consumer Bridge；subscription 映射为 consumer group。
  std::shared_ptr<Consumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) override;
  /// 幂等关闭 facade；已创建 Bridge 继续管理自己的 backend 生命周期。
  void close() override;

private:
  Options options_;
  bool closed_;
};

} // namespace mq
} // namespace mental1104
