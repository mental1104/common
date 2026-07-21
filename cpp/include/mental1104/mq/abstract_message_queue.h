#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mental1104 {
namespace mq {

typedef std::vector<std::uint8_t> Record;
typedef std::string Schema;
typedef std::map<std::string, std::string> Options;

struct SendResult {
  bool ok;
  std::string message_id;
  std::string error;

  static SendResult success(const std::string &message_id = std::string());
  static SendResult failure(const std::string &error);
};

typedef std::function<void(const SendResult &)> SendCallback;

class MessageHandle {
public:
  virtual ~MessageHandle() {}
};

struct Message {
  Record payload;
  std::shared_ptr<MessageHandle> native;
};

typedef std::shared_ptr<Message> MessagePtr;
typedef std::function<void(const MessagePtr &)> MessageListener;

enum class SubscriptionType {
  Shared,
  Exclusive,
  Failover,
  KeyShared,
};

class AbstractProducer {
public:
  AbstractProducer() {}
  virtual ~AbstractProducer() {}
  AbstractProducer(const AbstractProducer &) = delete;
  AbstractProducer &operator=(const AbstractProducer &) = delete;

  virtual void send(const Record &record) = 0;
  virtual void send_async(const Record &record,
                          const SendCallback &callback = SendCallback()) = 0;
  virtual void close() = 0;
};

class AbstractConsumer {
public:
  AbstractConsumer() {}
  virtual ~AbstractConsumer() {}
  AbstractConsumer(const AbstractConsumer &) = delete;
  AbstractConsumer &operator=(const AbstractConsumer &) = delete;

  virtual MessagePtr receive(int timeout_millis = -1) = 0;
  virtual void acknowledge(const MessagePtr &message) = 0;
  virtual void negative_acknowledge(const MessagePtr &message) = 0;
  virtual void unsubscribe() = 0;
  virtual void resubscribe() = 0;
  virtual void close() = 0;
};

class AbstractMessageQueue {
public:
  AbstractMessageQueue() {}
  virtual ~AbstractMessageQueue() {}
  AbstractMessageQueue(const AbstractMessageQueue &) = delete;
  AbstractMessageQueue &operator=(const AbstractMessageQueue &) = delete;

  virtual std::shared_ptr<AbstractProducer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) = 0;

  virtual std::shared_ptr<AbstractConsumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) = 0;

  virtual void close() = 0;
};

Record make_record(const std::string &value);
std::string record_to_string(const Record &record);
std::string build_kafka_topic(const std::string &tenant,
                              const std::string &namespace_name,
                              const std::string &topic);
std::string build_pulsar_topic(const std::string &tenant,
                               const std::string &namespace_name,
                               const std::string &topic);

} // namespace mq
} // namespace mental1104
