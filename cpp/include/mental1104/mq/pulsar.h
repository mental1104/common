#pragma once

#include "mental1104/mq/abstract_message_queue.h"

namespace mental1104 {
namespace mq {

struct PulsarBackendConfig : public BackendConfig {
  std::string service_url;
  std::string authentication_token;
  Options options;
  int close_timeout_millis;
  PulsarBackendConfig();
  BackendType backend_type() const override;
};

bool pulsar_available();
std::unique_ptr<IProducerBackend>
create_pulsar_producer_backend(const ProducerConfig &config,
                               const PulsarBackendConfig &backend);
std::unique_ptr<IConsumerBackend>
create_pulsar_consumer_backend(const ConsumerConfig &config,
                               const PulsarBackendConfig &backend);

class PulsarMessageQueue : public AbstractMessageQueue {
public:
  explicit PulsarMessageQueue(const Options &config = Options());
  ~PulsarMessageQueue() noexcept;
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
