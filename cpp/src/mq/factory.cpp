#include "mental1104/mq/factory.h"
#include "mental1104/mq/kafka.h"
#include "mental1104/mq/pulsar.h"

namespace mental1104 {
namespace mq {
namespace {
std::string backend_name(BackendType type) {
  return type == BackendType::Kafka ? "kafka" : "pulsar";
}
} // namespace

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
    throw MQException(exception_error("create_producer_backend", name));
  }
}

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
    throw MQException(exception_error("create_consumer_backend", name));
  }
}

Producer create_producer(const ProducerConfig &config) {
  return Producer(create_producer_backend(config));
}
Consumer create_consumer(const ConsumerConfig &config) {
  return Consumer(create_consumer_backend(config));
}

} // namespace mq
} // namespace mental1104
