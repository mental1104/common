#pragma once

#include "mental1104/mq/bridge.h"

namespace mental1104 {
namespace mq {

// Compatibility factory facade. New code should prefer factory.h and the
// Producer/AsyncProducer/Consumer bridge types directly.
class AbstractMessageQueue {
public:
  virtual ~AbstractMessageQueue() {}
  AbstractMessageQueue(const AbstractMessageQueue &) = delete;
  AbstractMessageQueue &operator=(const AbstractMessageQueue &) = delete;

  virtual std::shared_ptr<Producer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) = 0;
  virtual std::shared_ptr<Consumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) = 0;
  virtual void close() = 0;

protected:
  AbstractMessageQueue() {}
};

typedef Producer AbstractProducer;
typedef Consumer AbstractConsumer;

} // namespace mq
} // namespace mental1104
