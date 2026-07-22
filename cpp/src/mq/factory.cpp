#include "mental1104/mq/factory.h"
#include "mental1104/mq/kafka.h"
#include "mental1104/mq/pulsar.h"

namespace mental1104 {
namespace mq {
namespace {

/// 把 BackendType 转换为稳定日志名称。
std::string backend_name(BackendType type) {
  return type == BackendType::Kafka ? "kafka" : "pulsar";
}

} // namespace

/// 根据多态配置选择 Kafka 或 Pulsar Producer backend。
std::unique_ptr<IProducerBackend>
create_producer_backend(const ProducerConfig &config) {
  if (!config.backend)
    throw MQException(MQError(ErrorCode::InvalidConfig,
                              "create_producer_backend",
                              "backend config must not be null"));
  const std::string name = backend_name(config.backend->backend_type());
  try {
    if (config.backend->backend_type() == BackendType::Kafka) {
      const KafkaBackendConfig *value =
          dynamic_cast<const KafkaBackendConfig *>(config.backend.get());
      if (!value)
        throw MQException(
            MQError(ErrorCode::InvalidConfig, "create_producer_backend",
                    "Kafka backend config has the wrong dynamic type"));
      return create_kafka_producer_backend(config, *value);
    }
    if (config.backend->backend_type() == BackendType::Pulsar) {
      const PulsarBackendConfig *value =
          dynamic_cast<const PulsarBackendConfig *>(config.backend.get());
      if (!value)
        throw MQException(
            MQError(ErrorCode::InvalidConfig, "create_producer_backend",
                    "Pulsar backend config has the wrong dynamic type"));
      return create_pulsar_producer_backend(config, *value);
    }
    throw MQException(MQError(ErrorCode::InvalidConfig,
                              "create_producer_backend",
                              "unsupported backend type"));
  } catch (const MQException &) {
    throw;
  } catch (...) {
    // 第三方 SDK 构造异常在 Factory 边界统一收敛。
    throw MQException(exception_error("create_producer_backend", name));
  }
}

/// 根据多态配置选择 Kafka 或 Pulsar Consumer backend。
std::unique_ptr<IConsumerBackend>
create_consumer_backend(const ConsumerConfig &config) {
  if (!config.backend)
    throw MQException(MQError(ErrorCode::InvalidConfig,
                              "create_consumer_backend",
                              "backend config must not be null"));
  const std::string name = backend_name(config.backend->backend_type());
  try {
    if (config.backend->backend_type() == BackendType::Kafka) {
      const KafkaBackendConfig *value =
          dynamic_cast<const KafkaBackendConfig *>(config.backend.get());
      if (!value)
        throw MQException(
            MQError(ErrorCode::InvalidConfig, "create_consumer_backend",
                    "Kafka backend config has the wrong dynamic type"));
      return create_kafka_consumer_backend(config, *value);
    }
    if (config.backend->backend_type() == BackendType::Pulsar) {
      const PulsarBackendConfig *value =
          dynamic_cast<const PulsarBackendConfig *>(config.backend.get());
      if (!value)
        throw MQException(
            MQError(ErrorCode::InvalidConfig, "create_consumer_backend",
                    "Pulsar backend config has the wrong dynamic type"));
      return create_pulsar_consumer_backend(config, *value);
    }
    throw MQException(MQError(ErrorCode::InvalidConfig,
                              "create_consumer_backend",
                              "unsupported backend type"));
  } catch (const MQException &) {
    throw;
  } catch (...) {
    // 第三方 SDK 构造异常在 Factory 边界统一收敛。
    throw MQException(exception_error("create_consumer_backend", name));
  }
}

/// 创建拥有具体 Producer backend 的 Bridge。
Producer create_producer(const ProducerConfig &config) {
  return Producer(create_producer_backend(config));
}

/// 创建拥有具体 Consumer backend 的 Bridge。
Consumer create_consumer(const ConsumerConfig &config) {
  return Consumer(create_consumer_backend(config));
}

} // namespace mq
} // namespace mental1104
