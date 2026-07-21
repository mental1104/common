#pragma once

#include "mental1104/mq/abstract_message_queue.h"

#include <memory>

namespace mental1104 {
namespace mq {

bool kafka_available();

class KafkaMessageQueue : public AbstractMessageQueue {
public:
  explicit KafkaMessageQueue(const Options &config = Options());
  ~KafkaMessageQueue();

  std::shared_ptr<AbstractProducer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) override;

  std::shared_ptr<AbstractConsumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) override;

  void close() override;

private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

} // namespace mq
} // namespace mental1104
