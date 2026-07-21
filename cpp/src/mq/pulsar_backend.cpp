#include "mental1104/mq/pulsar.h"
#include "mental1104/mq/factory.h"

#include <atomic>
#include <mutex>

#ifdef M1104_HAS_PULSAR
#include <pulsar/Client.h>
#include <pulsar/Consumer.h>
#include <pulsar/Message.h>
#include <pulsar/Producer.h>
#endif

namespace mental1104 {
namespace mq {

PulsarBackendConfig::PulsarBackendConfig() : close_timeout_millis(10000) {}
BackendType PulsarBackendConfig::backend_type() const {
  return BackendType::Pulsar;
}

#ifdef M1104_HAS_PULSAR
namespace {
MQError pulsar_error(const std::string &op, pulsar::Result result,
                     bool retryable = false) {
  return MQError(result == pulsar::ResultTimeout ? ErrorCode::Timeout
                                                 : ErrorCode::Backend,
                 op, pulsar::strResult(result), "pulsar", retryable);
}
std::string message_id(const pulsar::MessageId &id) {
  std::string value;
  id.serialize(value);
  return value;
}
pulsar::Message build_message(const Message &m) {
  pulsar::MessageBuilder builder;
  builder.setContent(std::string(m.payload.begin(), m.payload.end()));
  if (!m.key.empty())
    builder.setPartitionKey(std::string(m.key.begin(), m.key.end()));
  if (!m.headers.empty())
    builder.setProperties(m.headers);
  return builder.build();
}
pulsar::ConsumerType consumer_type(SubscriptionType value) {
  switch (value) {
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

class PulsarProducerBackend : public IProducerBackend {
public:
  PulsarProducerBackend(const ProducerConfig &config,
                        const PulsarBackendConfig &backend)
      : client_(backend.service_url), closed_(false) {
    pulsar::ProducerConfiguration pc;
    pc.setBatchingEnabled(config.batching_enabled);
    pulsar::Result result =
        client_.createProducer(build_pulsar_topic(config.topic), pc, producer_);
    if (result != pulsar::ResultOk)
      throw MQException(pulsar_error("create_producer", result));
  }
  ~PulsarProducerBackend() {
    try {
      close();
    } catch (...) {
    }
  }
  SendResult send(const Message &message) override {
    if (closed_.load())
      return SendResult::failure(
          MQError(ErrorCode::Closed, "send", "producer is closed", "pulsar"));
    pulsar::MessageId id;
    pulsar::Result result = producer_.send(build_message(message), id);
    return result == pulsar::ResultOk
               ? SendResult::success(message_id(id), id.partition())
               : SendResult::failure(pulsar_error("send", result, true));
  }
  OperationResult send_async(const Message &message,
                             const DeliveryCallback &callback) override {
    if (closed_.load())
      return OperationResult::failure(MQError(ErrorCode::Closed, "send_async",
                                              "producer is closed", "pulsar"));
    try {
      producer_.sendAsync(
          build_message(message),
          [callback](pulsar::Result result, const pulsar::MessageId &id) {
            if (!callback)
              return;
            if (result == pulsar::ResultOk)
              callback(SendResult::success(message_id(id), id.partition()));
            else
              callback(
                  SendResult::failure(pulsar_error("delivery", result, true)));
          });
      return OperationResult::success();
    } catch (...) {
      return OperationResult::failure(exception_error("send_async", "pulsar"));
    }
  }
  OperationResult close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_.exchange(true))
      return OperationResult::success();
    pulsar::Result flush = producer_.flush();
    pulsar::Result close_result = producer_.close();
    pulsar::Result client_result = client_.close();
    if (flush != pulsar::ResultOk)
      return OperationResult::failure(pulsar_error("flush", flush, true));
    if (close_result != pulsar::ResultOk)
      return OperationResult::failure(
          pulsar_error("close_producer", close_result));
    if (client_result != pulsar::ResultOk)
      return OperationResult::failure(
          pulsar_error("close_client", client_result));
    return OperationResult::success();
  }

private:
  pulsar::Client client_;
  pulsar::Producer producer_;
  std::atomic<bool> closed_;
  std::mutex mutex_;
};

class PulsarReceipt : public Receipt {
public:
  explicit PulsarReceipt(const pulsar::Message &value) : message(value) {}
  pulsar::Message message;
};

class PulsarConsumerBackend : public IConsumerBackend {
public:
  PulsarConsumerBackend(const ConsumerConfig &config,
                        const PulsarBackendConfig &backend)
      : client_(backend.service_url), topic_(build_pulsar_topic(config.topic)),
        subscription_(config.subscription), type_(config.subscription_type),
        closed_(false) {
    if (subscription_.empty())
      throw MQException(MQError(ErrorCode::InvalidConfig, "create_consumer",
                                "subscription must not be empty", "pulsar"));
    subscribe_new();
  }
  ~PulsarConsumerBackend() {
    try {
      close();
    } catch (...) {
    }
  }
  ReceiveResult receive(int timeout) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return ReceiveResult::failure(MQError(ErrorCode::Closed, "receive",
                                            "consumer is closed", "pulsar"));
    pulsar::Message native;
    pulsar::Result result = timeout < 0 ? consumer_.receive(native)
                                        : consumer_.receive(native, timeout);
    if (result != pulsar::ResultOk)
      return ReceiveResult::failure(pulsar_error("receive", result, true));
    BackendMessage value;
    value.message.topic = native.getTopicName();
    value.message.payload.assign(
        static_cast<const std::uint8_t *>(native.getData()),
        static_cast<const std::uint8_t *>(native.getData()) +
            native.getLength());
    if (native.hasPartitionKey())
      value.message.key.assign(native.getPartitionKey().begin(),
                               native.getPartitionKey().end());
    value.message.headers = native.getProperties();
    value.message.message_id = message_id(native.getMessageId());
    value.message.has_partition = native.getMessageId().partition() >= 0;
    value.message.partition = native.getMessageId().partition();
    value.receipt.reset(new PulsarReceipt(native));
    return ReceiveResult::success(value);
  }
  OperationResult acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<PulsarReceipt> r =
        std::dynamic_pointer_cast<PulsarReceipt>(receipt);
    if (!r)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "acknowledge",
                  "invalid Pulsar receipt", "pulsar"));
    pulsar::Result result = consumer_.acknowledge(r->message);
    return result == pulsar::ResultOk ? OperationResult::success()
                                      : OperationResult::failure(pulsar_error(
                                            "acknowledge", result, true));
  }
  OperationResult negative_acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<PulsarReceipt> r =
        std::dynamic_pointer_cast<PulsarReceipt>(receipt);
    if (!r)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "negative_acknowledge",
                  "invalid Pulsar receipt", "pulsar"));
    consumer_.negativeAcknowledge(r->message);
    return OperationResult::success();
  }
  OperationResult unsubscribe() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "unsubscribe",
                                              "consumer is closed", "pulsar"));
    pulsar::Result result = consumer_.unsubscribe();
    return result == pulsar::ResultOk
               ? OperationResult::success()
               : OperationResult::failure(pulsar_error("unsubscribe", result));
  }
  OperationResult resubscribe() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "resubscribe",
                                              "consumer is closed", "pulsar"));
    consumer_.close();
    return subscribe_new_result();
  }
  OperationResult close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::success();
    closed_ = true;
    pulsar::Result c = consumer_.close();
    pulsar::Result client = client_.close();
    if (c != pulsar::ResultOk)
      return OperationResult::failure(pulsar_error("close_consumer", c));
    if (client != pulsar::ResultOk)
      return OperationResult::failure(pulsar_error("close_client", client));
    return OperationResult::success();
  }

private:
  OperationResult subscribe_new_result() {
    pulsar::ConsumerConfiguration cc;
    cc.setConsumerType(consumer_type(type_));
    pulsar::Result result =
        client_.subscribe(topic_, subscription_, cc, consumer_);
    return result == pulsar::ResultOk
               ? OperationResult::success()
               : OperationResult::failure(pulsar_error("subscribe", result));
  }
  void subscribe_new() {
    OperationResult r = subscribe_new_result();
    if (!r.ok)
      throw MQException(r.error);
  }
  pulsar::Client client_;
  std::string topic_;
  std::string subscription_;
  SubscriptionType type_;
  pulsar::Consumer consumer_;
  bool closed_;
  std::mutex mutex_;
};
} // namespace
#endif

bool pulsar_available() {
#ifdef M1104_HAS_PULSAR
  return true;
#else
  return false;
#endif
}
std::unique_ptr<IProducerBackend>
create_pulsar_producer_backend(const ProducerConfig &config,
                               const PulsarBackendConfig &backend) {
#ifdef M1104_HAS_PULSAR
  return std::unique_ptr<IProducerBackend>(
      new PulsarProducerBackend(config, backend));
#else
  (void)config;
  (void)backend;
  throw MQException(MQError(ErrorCode::InvalidConfig, "create_producer_backend",
                            "pulsar-client-cpp is unavailable", "pulsar"));
#endif
}
std::unique_ptr<IConsumerBackend>
create_pulsar_consumer_backend(const ConsumerConfig &config,
                               const PulsarBackendConfig &backend) {
#ifdef M1104_HAS_PULSAR
  return std::unique_ptr<IConsumerBackend>(
      new PulsarConsumerBackend(config, backend));
#else
  (void)config;
  (void)backend;
  throw MQException(MQError(ErrorCode::InvalidConfig, "create_consumer_backend",
                            "pulsar-client-cpp is unavailable", "pulsar"));
#endif
}

PulsarMessageQueue::PulsarMessageQueue(const Options &o)
    : options_(o), closed_(false) {}
PulsarMessageQueue::~PulsarMessageQueue() noexcept {
  try {
    close();
  } catch (...) {
  }
}
std::shared_ptr<Producer> PulsarMessageQueue::create_producer(
    const std::string &tenant, const std::string &ns, const std::string &topic,
    const Schema &, bool batching) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_producer",
                              "message queue is closed", "pulsar"));
  std::shared_ptr<PulsarBackendConfig> b(new PulsarBackendConfig());
  Options::const_iterator it = options_.find("service.url");
  if (it != options_.end())
    b->service_url = it->second;
  b->options = options_;
  ProducerConfig c;
  c.topic.tenant = tenant;
  c.topic.namespace_name = ns;
  c.topic.topic = topic;
  c.batching_enabled = batching;
  c.backend = b;
  return std::shared_ptr<Producer>(new Producer(create_producer_backend(c)));
}
std::shared_ptr<Consumer> PulsarMessageQueue::create_consumer(
    const std::string &tenant, const std::string &ns, const std::string &topic,
    const std::string &subscription, const Schema &, SubscriptionType type,
    const MessageListener &listener, const Options &options) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_consumer",
                              "message queue is closed", "pulsar"));
  std::shared_ptr<PulsarBackendConfig> b(new PulsarBackendConfig());
  Options merged = options_;
  merged.insert(options.begin(), options.end());
  Options::const_iterator it = merged.find("service.url");
  if (it != merged.end())
    b->service_url = it->second;
  b->options = merged;
  ConsumerConfig c;
  c.topic.tenant = tenant;
  c.topic.namespace_name = ns;
  c.topic.topic = topic;
  c.subscription = subscription;
  c.subscription_type = type;
  c.backend = b;
  return std::shared_ptr<Consumer>(new Consumer(
      std::shared_ptr<IConsumerBackend>(create_consumer_backend(c).release()),
      listener));
}
void PulsarMessageQueue::close() { closed_ = true; }

} // namespace mq
} // namespace mental1104
