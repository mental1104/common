#include "mental1104/mq/pulsar.h"
#include "mental1104/mq/transport.h"

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef M1104_HAS_PULSAR
#include <pulsar/Client.h>
#include <pulsar/Consumer.h>
#include <pulsar/Message.h>
#include <pulsar/Producer.h>
#endif

namespace mental1104 {
namespace mq {

#ifdef M1104_HAS_PULSAR
namespace {

std::string pulsar_error(const char *prefix, pulsar::Result result) {
  return std::string(prefix) + pulsar::strResult(result);
}

std::string pulsar_message_id(const pulsar::MessageId &id) {
  std::string serialized;
  id.serialize(serialized);
  return serialized;
}

pulsar::Message build_pulsar_message(const Record &record) {
  const std::string content(record.begin(), record.end());
  return pulsar::MessageBuilder().setContent(content).build();
}

pulsar::ConsumerType to_pulsar_type(SubscriptionType type) {
  switch (type) {
  case SubscriptionType::Exclusive:
    return pulsar::ConsumerExclusive;
  case SubscriptionType::Failover:
    return pulsar::ConsumerFailover;
  case SubscriptionType::KeyShared:
    return pulsar::ConsumerKeyShared;
  case SubscriptionType::Shared:
  default:
    return pulsar::ConsumerShared;
  }
}

class PulsarProducerTransport : public ProducerTransport {
public:
  explicit PulsarProducerTransport(const pulsar::Producer &producer)
      : producer_(producer), closed_(false) {}

  SendResult send(const Record &record) override {
    ensure_open();
    const pulsar::Message message = build_pulsar_message(record);
    pulsar::MessageId id;
    const pulsar::Result result = producer_.send(message, id);
    if (result != pulsar::ResultOk) {
      return SendResult::failure(pulsar::strResult(result));
    }
    return SendResult::success(pulsar_message_id(id));
  }

  void send_async(const Record &record, const SendCallback &callback) override {
    ensure_open();
    const pulsar::Message message = build_pulsar_message(record);
    producer_.sendAsync(message,
                        [callback](pulsar::Result result,
                                   const pulsar::MessageId &id) {
                          if (!callback) {
                            return;
                          }
                          if (result == pulsar::ResultOk) {
                            callback(SendResult::success(
                                pulsar_message_id(id)));
                          } else {
                            callback(SendResult::failure(
                                pulsar::strResult(result)));
                          }
                        });
  }

  void close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_.exchange(true)) {
      return;
    }
    producer_.flush();
    producer_.close();
  }

private:
  void ensure_open() const {
    if (closed_.load()) {
      throw std::runtime_error("Pulsar producer is closed");
    }
  }

  pulsar::Producer producer_;
  mutable std::mutex mutex_;
  std::atomic<bool> closed_;
};

class PulsarMessageHandle : public MessageHandle {
public:
  explicit PulsarMessageHandle(const pulsar::Message &message) : message(message) {}
  pulsar::Message message;
};

class PulsarConsumerTransport : public ConsumerTransport {
public:
  PulsarConsumerTransport(pulsar::Client &client, const std::string &topic,
                          const std::string &subscription,
                          const pulsar::ConsumerConfiguration &configuration)
      : client_(client), topic_(topic), subscription_(subscription),
        configuration_(configuration), closed_(false) {
    subscribe_new();
  }

  MessagePtr receive(int timeout_millis) override {
    ensure_open();
    pulsar::Message native;
    const pulsar::Result result =
        timeout_millis < 0 ? consumer_.receive(native)
                           : consumer_.receive(native, timeout_millis);
    if (result == pulsar::ResultTimeout) {
      throw std::runtime_error("message receive timed out");
    }
    if (result != pulsar::ResultOk) {
      throw std::runtime_error(pulsar_error("Pulsar receive: ", result));
    }
    MessagePtr message(new Message());
    const std::string payload = native.getDataAsString();
    message->payload.assign(payload.begin(), payload.end());
    message->native.reset(new PulsarMessageHandle(native));
    return message;
  }

  void acknowledge(const MessagePtr &message) override {
    const pulsar::Result result =
        consumer_.acknowledge(handle_of(message)->message);
    if (result != pulsar::ResultOk) {
      throw std::runtime_error(pulsar_error("Pulsar acknowledge: ", result));
    }
  }

  void negative_acknowledge(const MessagePtr &message) override {
    consumer_.negativeAcknowledge(handle_of(message)->message);
  }

  void unsubscribe() override {
    ensure_open();
    const pulsar::Result result = consumer_.unsubscribe();
    if (result != pulsar::ResultOk) {
      throw std::runtime_error(pulsar_error("Pulsar unsubscribe: ", result));
    }
  }

  void resubscribe() override {
    ensure_open();
    consumer_.close();
    subscribe_new();
  }

  void close() override {
    if (closed_) {
      return;
    }
    closed_ = true;
    consumer_.close();
  }

private:
  void ensure_open() const {
    if (closed_) {
      throw std::runtime_error("Pulsar consumer is closed");
    }
  }

  std::shared_ptr<PulsarMessageHandle>
  handle_of(const MessagePtr &message) const {
    if (!message || !message->native) {
      throw std::invalid_argument("message does not belong to Pulsar consumer");
    }
    std::shared_ptr<PulsarMessageHandle> handle =
        std::dynamic_pointer_cast<PulsarMessageHandle>(message->native);
    if (!handle) {
      throw std::invalid_argument("message does not belong to Pulsar consumer");
    }
    return handle;
  }

  void subscribe_new() {
    const pulsar::Result result =
        client_.subscribe(topic_, subscription_, configuration_, consumer_);
    if (result != pulsar::ResultOk) {
      throw std::runtime_error(pulsar_error("Pulsar subscribe: ", result));
    }
  }

  pulsar::Client &client_;
  std::string topic_;
  std::string subscription_;
  pulsar::ConsumerConfiguration configuration_;
  pulsar::Consumer consumer_;
  bool closed_;
};

} // namespace
#endif

class PulsarMessageQueue::Impl {
public:
  explicit Impl(const Options &config)
#ifdef M1104_HAS_PULSAR
      : client(option(config, "service.url")), config(config), closed(false) {}
#else
      : config(config), closed(false) {}
#endif

#ifdef M1104_HAS_PULSAR
  static std::string option(const Options &config, const std::string &key) {
    Options::const_iterator it = config.find(key);
    if (it == config.end() || it->second.empty()) {
      throw std::invalid_argument("missing Pulsar option: " + key);
    }
    return it->second;
  }
  pulsar::Client client;
#endif
  Options config;
  bool closed;
  std::mutex mutex;
};

bool pulsar_available() {
#ifdef M1104_HAS_PULSAR
  return true;
#else
  return false;
#endif
}

PulsarMessageQueue::PulsarMessageQueue(const Options &config)
    : impl_(new Impl(config)) {}

PulsarMessageQueue::~PulsarMessageQueue() { close(); }

std::shared_ptr<AbstractProducer> PulsarMessageQueue::create_producer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const Schema &schema, bool batching_enabled) {
  (void)schema;
  const std::string full_topic =
      build_pulsar_topic(tenant, namespace_name, topic);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->closed) {
    throw std::runtime_error("PulsarMessageQueue is closed");
  }
#ifdef M1104_HAS_PULSAR
  pulsar::ProducerConfiguration configuration;
  configuration.setBatchingEnabled(batching_enabled);
  pulsar::Producer native;
  const pulsar::Result result =
      impl_->client.createProducer(full_topic, configuration, native);
  if (result != pulsar::ResultOk) {
    throw std::runtime_error(
        pulsar_error("create Pulsar producer: ", result));
  }
  return std::shared_ptr<AbstractProducer>(new Producer(
      std::shared_ptr<ProducerTransport>(new PulsarProducerTransport(native))));
#else
  (void)full_topic;
  (void)batching_enabled;
  throw std::runtime_error(
      "Pulsar support is unavailable; install pulsar-client-cpp headers and library");
#endif
}

std::shared_ptr<AbstractConsumer> PulsarMessageQueue::create_consumer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const std::string &subscription,
    const Schema &schema, SubscriptionType subscription_type,
    const MessageListener &message_listener, const Options &options) {
  (void)schema;
  (void)options;
  if (subscription.empty()) {
    throw std::invalid_argument("Pulsar subscription must not be empty");
  }
  const std::string full_topic =
      build_pulsar_topic(tenant, namespace_name, topic);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->closed) {
    throw std::runtime_error("PulsarMessageQueue is closed");
  }
#ifdef M1104_HAS_PULSAR
  pulsar::ConsumerConfiguration configuration;
  configuration.setConsumerType(to_pulsar_type(subscription_type));
  return std::shared_ptr<AbstractConsumer>(new Consumer(
      std::shared_ptr<ConsumerTransport>(new PulsarConsumerTransport(
          impl_->client, full_topic, subscription, configuration)),
      message_listener));
#else
  (void)full_topic;
  (void)subscription_type;
  (void)message_listener;
  throw std::runtime_error(
      "Pulsar support is unavailable; install pulsar-client-cpp headers and library");
#endif
}

void PulsarMessageQueue::close() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->closed) {
    return;
  }
  impl_->closed = true;
#ifdef M1104_HAS_PULSAR
  impl_->client.close();
#endif
}

} // namespace mq
} // namespace mental1104
