#pragma once

#include "mental1104/mq/abstract_message_queue.h"

namespace mental1104 {
namespace mq {

struct KafkaBackendConfig : public BackendConfig {
  Options options;
  int close_timeout_millis;
  KafkaBackendConfig();
  BackendType backend_type() const override;
};

bool kafka_available();
std::unique_ptr<IProducerBackend>
create_kafka_producer_backend(const ProducerConfig &config,
                              const KafkaBackendConfig &backend);
std::unique_ptr<IConsumerBackend>
create_kafka_consumer_backend(const ConsumerConfig &config,
                              const KafkaBackendConfig &backend);

class KafkaMessageQueue : public AbstractMessageQueue {
public:
  explicit KafkaMessageQueue(const Options &config = Options());
  ~KafkaMessageQueue() noexcept;
  std::shared_ptr<Producer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) override;
  std::shared_ptr<Consumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) override;
  void close() override;

private:
  Options options_;
  bool closed_;
};

} // namespace mq
} // namespace mental1104
