#include "mental1104/mq/kafka.h"
#include "mental1104/mq/factory.h"

#include <atomic>
#include <future>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef M1104_HAS_RDKAFKA
#include <librdkafka/rdkafkacpp.h>
#endif

namespace mental1104 {
namespace mq {

KafkaBackendConfig::KafkaBackendConfig() : close_timeout_millis(10000) {}
BackendType KafkaBackendConfig::backend_type() const {
  return BackendType::Kafka;
}

#ifdef M1104_HAS_RDKAFKA
namespace {

MQError kafka_error(const std::string &op, const std::string &message,
                    bool retryable = false) {
  return MQError(ErrorCode::Backend, op, message, "kafka", retryable);
}
void apply_options(RdKafka::Conf &conf, const Options &options) {
  std::string error;
  for (Options::const_iterator it = options.begin(); it != options.end(); ++it)
    if (conf.set(it->first, it->second, error) != RdKafka::Conf::CONF_OK)
      throw MQException(MQError(ErrorCode::InvalidConfig, "kafka_config",
                                it->first + ": " + error, "kafka"));
}

struct Delivery {
  explicit Delivery(const DeliveryCallback &cb) : callback(cb) {}
  DeliveryCallback callback;
};

class KafkaProducerBackend : public IProducerBackend,
                             public RdKafka::DeliveryReportCb {
public:
  KafkaProducerBackend(const ProducerConfig &config,
                       const KafkaBackendConfig &backend)
      : topic_(build_kafka_topic(config.topic)),
        close_timeout_(backend.close_timeout_millis), running_(true),
        closed_(false) {
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf)
      throw MQException(
          kafka_error("create_producer", "cannot create configuration"));
    Options effective = backend.options;
    if (!config.batching_enabled) {
      effective["linger.ms"] = "0";
      effective["batch.num.messages"] = "1";
    }
    std::string error;
    if (conf->set("dr_cb", this, error) != RdKafka::Conf::CONF_OK)
      throw MQException(kafka_error("create_producer", error));
    apply_options(*conf, effective);
    producer_.reset(RdKafka::Producer::create(conf.get(), error));
    if (!producer_)
      throw MQException(kafka_error("create_producer", error));
    poller_ = std::thread([this]() {
      while (running_.load())
        producer_->poll(50);
      producer_->poll(0);
    });
  }
  ~KafkaProducerBackend() {
    try {
      close();
    } catch (...) {
    }
  }

  SendResult send(const Message &message) override {
    std::promise<SendResult> promise;
    std::future<SendResult> future = promise.get_future();
    OperationResult accepted = send_async(
        message, [&promise](const SendResult &r) { promise.set_value(r); });
    if (!accepted.ok)
      return SendResult::failure(accepted.error);
    return future.get();
  }

  OperationResult send_async(const Message &message,
                             const DeliveryCallback &callback) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "send_async",
                                              "producer is closed", "kafka"));
    std::unique_ptr<RdKafka::Headers> headers;
    if (!message.headers.empty()) {
      headers.reset(RdKafka::Headers::create());
      for (MessageHeaders::const_iterator it = message.headers.begin();
           it != message.headers.end(); ++it)
        if (headers->add(it->first, it->second) != RdKafka::ERR_NO_ERROR)
          return OperationResult::failure(
              kafka_error("send_async", "failed to add message header"));
    }
    Delivery *delivery = new Delivery(callback);
    const int32_t partition = message.has_partition
                                  ? message.partition
                                  : RdKafka::Topic::PARTITION_UA;
    RdKafka::Headers *raw_headers = headers.release();
    const RdKafka::ErrorCode code = producer_->produce(
        message.topic.empty() ? topic_ : message.topic, partition,
        RdKafka::Producer::RK_MSG_COPY,
        message.payload.empty()
            ? NULL
            : const_cast<std::uint8_t *>(&message.payload[0]),
        message.payload.size(), message.key.empty() ? NULL : &message.key[0],
        message.key.size(), 0, raw_headers, delivery);
    if (code != RdKafka::ERR_NO_ERROR) {
      delete raw_headers;
      delete delivery;
      return OperationResult::failure(
          kafka_error("send_async", RdKafka::err2str(code),
                      code == RdKafka::ERR__QUEUE_FULL));
    }
    producer_->poll(0);
    return OperationResult::success();
  }

  void dr_cb(RdKafka::Message &message) override {
    std::unique_ptr<Delivery> delivery(
        static_cast<Delivery *>(message.msg_opaque()));
    if (!delivery || !delivery->callback)
      return;
    if (message.err() == RdKafka::ERR_NO_ERROR) {
      std::ostringstream id;
      id << message.topic_name() << ":" << message.partition() << ":"
         << message.offset();
      delivery->callback(SendResult::success(id.str(), message.partition()));
    } else
      delivery->callback(
          SendResult::failure(kafka_error("delivery", message.errstr(), true)));
  }

  OperationResult close() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_)
        return OperationResult::success();
      closed_ = true;
    }
    RdKafka::ErrorCode code = producer_->flush(close_timeout_);
    running_.store(false);
    if (poller_.joinable() && poller_.get_id() != std::this_thread::get_id())
      poller_.join();
    if (code != RdKafka::ERR_NO_ERROR)
      return OperationResult::failure(
          kafka_error("close", RdKafka::err2str(code), true));
    return OperationResult::success();
  }

private:
  std::string topic_;
  int close_timeout_;
  std::unique_ptr<RdKafka::Producer> producer_;
  std::atomic<bool> running_;
  bool closed_;
  std::mutex mutex_;
  std::thread poller_;
};

class KafkaReceipt : public Receipt {
public:
  explicit KafkaReceipt(RdKafka::Message *value) : message(value) {}
  std::unique_ptr<RdKafka::Message> message;
};

class KafkaConsumerBackend : public IConsumerBackend {
public:
  KafkaConsumerBackend(const ConsumerConfig &config,
                       const KafkaBackendConfig &backend)
      : options_(backend.options), topic_(build_kafka_topic(config.topic)),
        subscription_(config.subscription), closed_(false) {
    subscribe_new();
  }
  ~KafkaConsumerBackend() {
    try {
      close();
    } catch (...) {
    }
  }

  ReceiveResult receive(int timeout) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_ || !consumer_)
      return ReceiveResult::failure(
          MQError(ErrorCode::Closed, "receive", "consumer is closed", "kafka"));
    RdKafka::Message *native = consumer_->consume(timeout < 0 ? 1000 : timeout);
    if (!native)
      return ReceiveResult::failure(
          kafka_error("receive", "no message returned"));
    if (native->err() == RdKafka::ERR__TIMED_OUT) {
      delete native;
      return ReceiveResult::failure(MQError(ErrorCode::Timeout, "receive",
                                            "message receive timed out",
                                            "kafka", true));
    }
    if (native->err() != RdKafka::ERR_NO_ERROR) {
      std::string e = native->errstr();
      delete native;
      return ReceiveResult::failure(kafka_error("receive", e, true));
    }
    BackendMessage value;
    value.message.topic = native->topic_name();
    value.message.has_partition = true;
    value.message.partition = native->partition();
    std::ostringstream id;
    id << native->topic_name() << ":" << native->partition() << ":"
       << native->offset();
    value.message.message_id = id.str();
    const std::uint8_t *payload =
        static_cast<const std::uint8_t *>(native->payload());
    if (payload && native->len())
      value.message.payload.assign(payload, payload + native->len());
    const std::uint8_t *key =
        static_cast<const std::uint8_t *>(native->key_pointer());
    if (key && native->key_len())
      value.message.key.assign(key, key + native->key_len());
    RdKafka::Headers *headers = native->headers();
    if (headers) {
      const std::vector<RdKafka::Headers::Header> all = headers->get_all();
      for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i].err() == RdKafka::ERR_NO_ERROR)
          value.message.headers[all[i].key()] = std::string(
              static_cast<const char *>(all[i].value()), all[i].value_size());
    }
    value.receipt.reset(new KafkaReceipt(native));
    return ReceiveResult::success(value);
  }

  OperationResult acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<KafkaReceipt> r =
        std::dynamic_pointer_cast<KafkaReceipt>(receipt);
    if (!r || !r->message)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "acknowledge",
                  "invalid Kafka receipt", "kafka"));
    RdKafka::ErrorCode code = consumer_->commitSync(r->message.get());
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(
                     kafka_error("acknowledge", RdKafka::err2str(code), true));
  }
  OperationResult negative_acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<KafkaReceipt> r =
        std::dynamic_pointer_cast<KafkaReceipt>(receipt);
    if (!r || !r->message)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "negative_acknowledge",
                  "invalid Kafka receipt", "kafka"));
    std::unique_ptr<RdKafka::TopicPartition> p(RdKafka::TopicPartition::create(
        r->message->topic_name(), r->message->partition(),
        r->message->offset()));
    RdKafka::ErrorCode code = consumer_->seek(*p, 5000);
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(kafka_error(
                     "negative_acknowledge", RdKafka::err2str(code), true));
  }
  OperationResult unsubscribe() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "unsubscribe",
                                              "consumer is closed", "kafka"));
    RdKafka::ErrorCode code = consumer_->unsubscribe();
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(
                     kafka_error("unsubscribe", RdKafka::err2str(code)));
  }
  OperationResult resubscribe() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "resubscribe",
                                              "consumer is closed", "kafka"));
    std::vector<std::string> topics(1, topic_);
    RdKafka::ErrorCode code = consumer_->subscribe(topics);
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(
                     kafka_error("resubscribe", RdKafka::err2str(code)));
  }
  OperationResult close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::success();
    closed_ = true;
    if (!consumer_)
      return OperationResult::success();
    RdKafka::ErrorCode code = consumer_->close();
    consumer_.reset();
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(
                     kafka_error("close", RdKafka::err2str(code)));
  }

private:
  void subscribe_new() {
    if (subscription_.empty())
      throw MQException(MQError(ErrorCode::InvalidConfig, "create_consumer",
                                "subscription must not be empty", "kafka"));
    Options effective = options_;
    effective["group.id"] = subscription_;
    effective["enable.auto.commit"] = "false";
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf)
      throw MQException(
          kafka_error("create_consumer", "cannot create configuration"));
    apply_options(*conf, effective);
    std::string error;
    consumer_.reset(RdKafka::KafkaConsumer::create(conf.get(), error));
    if (!consumer_)
      throw MQException(kafka_error("create_consumer", error));
    std::vector<std::string> topics(1, topic_);
    RdKafka::ErrorCode code = consumer_->subscribe(topics);
    if (code != RdKafka::ERR_NO_ERROR)
      throw MQException(kafka_error("create_consumer", RdKafka::err2str(code)));
  }
  Options options_;
  std::string topic_;
  std::string subscription_;
  std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
  bool closed_;
  std::mutex mutex_;
};
} // namespace
#endif

bool kafka_available() {
#ifdef M1104_HAS_RDKAFKA
  return true;
#else
  return false;
#endif
}

std::unique_ptr<IProducerBackend>
create_kafka_producer_backend(const ProducerConfig &config,
                              const KafkaBackendConfig &backend) {
#ifdef M1104_HAS_RDKAFKA
  return std::unique_ptr<IProducerBackend>(
      new KafkaProducerBackend(config, backend));
#else
  (void)config;
  (void)backend;
  throw MQException(MQError(ErrorCode::InvalidConfig, "create_producer_backend",
                            "librdkafka is unavailable", "kafka"));
#endif
}
std::unique_ptr<IConsumerBackend>
create_kafka_consumer_backend(const ConsumerConfig &config,
                              const KafkaBackendConfig &backend) {
#ifdef M1104_HAS_RDKAFKA
  return std::unique_ptr<IConsumerBackend>(
      new KafkaConsumerBackend(config, backend));
#else
  (void)config;
  (void)backend;
  throw MQException(MQError(ErrorCode::InvalidConfig, "create_consumer_backend",
                            "librdkafka is unavailable", "kafka"));
#endif
}

KafkaMessageQueue::KafkaMessageQueue(const Options &o)
    : options_(o), closed_(false) {}
KafkaMessageQueue::~KafkaMessageQueue() noexcept {
  try {
    close();
  } catch (...) {
  }
}
std::shared_ptr<Producer> KafkaMessageQueue::create_producer(
    const std::string &tenant, const std::string &ns, const std::string &topic,
    const Schema &, bool batching) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_producer",
                              "message queue is closed", "kafka"));
  std::shared_ptr<KafkaBackendConfig> b(new KafkaBackendConfig());
  b->options = options_;
  ProducerConfig c;
  c.topic.tenant = tenant;
  c.topic.namespace_name = ns;
  c.topic.topic = topic;
  c.batching_enabled = batching;
  c.backend = b;
  return std::shared_ptr<Producer>(new Producer(create_producer_backend(c)));
}
std::shared_ptr<Consumer> KafkaMessageQueue::create_consumer(
    const std::string &tenant, const std::string &ns, const std::string &topic,
    const std::string &subscription, const Schema &, SubscriptionType type,
    const MessageListener &listener, const Options &options) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_consumer",
                              "message queue is closed", "kafka"));
  std::shared_ptr<KafkaBackendConfig> b(new KafkaBackendConfig());
  b->options = options_;
  b->options.insert(options.begin(), options.end());
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
void KafkaMessageQueue::close() { closed_ = true; }

} // namespace mq
} // namespace mental1104
