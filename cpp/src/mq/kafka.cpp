#include "mental1104/mq/kafka.h"
#include "mental1104/mq/transport.h"

#include <atomic>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

#ifdef M1104_HAS_RDKAFKA
#include <librdkafka/rdkafkacpp.h>
#endif

namespace mental1104 {
namespace mq {

#ifdef M1104_HAS_RDKAFKA
namespace {

void apply_kafka_options(RdKafka::Conf &conf, const Options &options) {
  std::string error;
  for (Options::const_iterator it = options.begin(); it != options.end(); ++it) {
    if (conf.set(it->first, it->second, error) != RdKafka::Conf::CONF_OK) {
      throw std::invalid_argument("invalid Kafka option " + it->first + ": " + error);
    }
  }
}

struct KafkaDelivery {
  explicit KafkaDelivery(const SendCallback &callback) : callback(callback) {}
  SendCallback callback;
};

class KafkaProducerTransport : public ProducerTransport,
                               public RdKafka::DeliveryReportCb {
public:
  KafkaProducerTransport(const Options &config, const std::string &topic,
                         bool batching_enabled)
      : topic_(topic), closed_(false), running_(true) {
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf) {
      throw std::runtime_error("cannot create Kafka producer configuration");
    }
    Options effective = config;
    if (!batching_enabled) {
      effective["linger.ms"] = "0";
      effective["batch.num.messages"] = "1";
    }
    std::string error;
    if (conf->set("dr_cb", this, error) != RdKafka::Conf::CONF_OK) {
      throw std::runtime_error(error);
    }
    apply_kafka_options(*conf, effective);
    producer_.reset(RdKafka::Producer::create(conf.get(), error));
    if (!producer_) {
      throw std::runtime_error("create Kafka producer: " + error);
    }
    poller_ = std::thread([this]() {
      while (running_.load()) {
        producer_->poll(50);
      }
      producer_->poll(0);
    });
  }

  ~KafkaProducerTransport() {
    try {
      close();
    } catch (...) {
    }
  }

  SendResult send(const Record &record) override {
    std::promise<SendResult> promise;
    std::future<SendResult> future = promise.get_future();
    send_async(record,
               [&promise](const SendResult &result) { promise.set_value(result); });
    return future.get();
  }

  void send_async(const Record &record, const SendCallback &callback) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      throw std::runtime_error("Kafka producer is closed");
    }
    KafkaDelivery *delivery = new KafkaDelivery(callback);
    const RdKafka::ErrorCode error = producer_->produce(
        topic_, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
        record.empty() ? NULL : const_cast<std::uint8_t *>(&record[0]),
        record.size(), NULL, 0, 0, delivery);
    if (error != RdKafka::ERR_NO_ERROR) {
      delete delivery;
      throw std::runtime_error("Kafka produce: " + RdKafka::err2str(error));
    }
    producer_->poll(0);
  }

  void dr_cb(RdKafka::Message &message) override {
    std::unique_ptr<KafkaDelivery> delivery(
        static_cast<KafkaDelivery *>(message.msg_opaque()));
    if (!delivery || !delivery->callback) {
      return;
    }
    if (message.err() == RdKafka::ERR_NO_ERROR) {
      delivery->callback(SendResult::success());
    } else {
      delivery->callback(SendResult::failure(message.errstr()));
    }
  }

  void close() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) {
        return;
      }
      closed_ = true;
    }
    while (producer_->outq_len() > 0) {
      producer_->poll(100);
    }
    running_.store(false);
    if (poller_.joinable()) {
      poller_.join();
    }
  }

private:
  std::string topic_;
  std::unique_ptr<RdKafka::Producer> producer_;
  std::mutex mutex_;
  bool closed_;
  std::atomic<bool> running_;
  std::thread poller_;
};

class KafkaMessageHandle : public MessageHandle {
public:
  explicit KafkaMessageHandle(RdKafka::Message *message) : message(message) {}
  std::unique_ptr<RdKafka::Message> message;
};

class KafkaConsumerTransport : public ConsumerTransport {
public:
  KafkaConsumerTransport(const Options &config, const std::string &topic,
                         const std::string &subscription)
      : config_(config), topic_(topic), subscription_(subscription),
        closed_(false) {
    subscribe_new();
  }

  ~KafkaConsumerTransport() {
    try {
      close();
    } catch (...) {
    }
  }

  MessagePtr receive(int timeout_millis) override {
    ensure_open();
    const int timeout = timeout_millis < 0 ? 1000 : timeout_millis;
    RdKafka::Message *native = consumer_->consume(timeout);
    if (!native) {
      throw std::runtime_error("Kafka receive returned no message");
    }
    if (native->err() == RdKafka::ERR__TIMED_OUT) {
      delete native;
      throw std::runtime_error("message receive timed out");
    }
    if (native->err() != RdKafka::ERR_NO_ERROR) {
      const std::string error = native->errstr();
      delete native;
      throw std::runtime_error("Kafka receive: " + error);
    }
    std::shared_ptr<KafkaMessageHandle> handle(new KafkaMessageHandle(native));
    MessagePtr message(new Message());
    const std::uint8_t *begin =
        static_cast<const std::uint8_t *>(native->payload());
    if (begin && native->len() > 0) {
      message->payload.assign(begin, begin + native->len());
    }
    message->native = handle;
    return message;
  }

  void acknowledge(const MessagePtr &message) override {
    std::shared_ptr<KafkaMessageHandle> handle = handle_of(message);
    const RdKafka::ErrorCode error = consumer_->commitSync(handle->message.get());
    if (error != RdKafka::ERR_NO_ERROR) {
      throw std::runtime_error("Kafka acknowledge: " +
                               RdKafka::err2str(error));
    }
  }

  void negative_acknowledge(const MessagePtr &message) override {
    std::shared_ptr<KafkaMessageHandle> handle = handle_of(message);
    std::unique_ptr<RdKafka::TopicPartition> partition(
        RdKafka::TopicPartition::create(handle->message->topic_name(),
                                        handle->message->partition(),
                                        handle->message->offset()));
    const RdKafka::ErrorCode error = consumer_->seek(*partition, 5000);
    if (error != RdKafka::ERR_NO_ERROR) {
      throw std::runtime_error("Kafka negative acknowledge: " +
                               RdKafka::err2str(error));
    }
  }

  void unsubscribe() override {
    ensure_open();
    const RdKafka::ErrorCode error = consumer_->unsubscribe();
    if (error != RdKafka::ERR_NO_ERROR) {
      throw std::runtime_error("Kafka unsubscribe: " +
                               RdKafka::err2str(error));
    }
  }

  void resubscribe() override {
    ensure_open();
    const std::vector<std::string> topics(1, topic_);
    const RdKafka::ErrorCode error = consumer_->subscribe(topics);
    if (error != RdKafka::ERR_NO_ERROR) {
      throw std::runtime_error("Kafka resubscribe: " +
                               RdKafka::err2str(error));
    }
  }

  void close() override {
    if (closed_) {
      return;
    }
    closed_ = true;
    if (consumer_) {
      consumer_->close();
      consumer_.reset();
    }
  }

private:
  void ensure_open() const {
    if (closed_ || !consumer_) {
      throw std::runtime_error("Kafka consumer is closed");
    }
  }

  std::shared_ptr<KafkaMessageHandle>
  handle_of(const MessagePtr &message) const {
    if (!message || !message->native) {
      throw std::invalid_argument("message does not belong to Kafka consumer");
    }
    std::shared_ptr<KafkaMessageHandle> handle =
        std::dynamic_pointer_cast<KafkaMessageHandle>(message->native);
    if (!handle || !handle->message) {
      throw std::invalid_argument("message does not belong to Kafka consumer");
    }
    return handle;
  }

  void subscribe_new() {
    Options effective = config_;
    effective["group.id"] = subscription_;
    effective["enable.auto.commit"] = "false";
    std::unique_ptr<RdKafka::Conf> conf(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf) {
      throw std::runtime_error("cannot create Kafka consumer configuration");
    }
    apply_kafka_options(*conf, effective);
    std::string error;
    consumer_.reset(RdKafka::KafkaConsumer::create(conf.get(), error));
    if (!consumer_) {
      throw std::runtime_error("create Kafka consumer: " + error);
    }
    const std::vector<std::string> topics(1, topic_);
    const RdKafka::ErrorCode code = consumer_->subscribe(topics);
    if (code != RdKafka::ERR_NO_ERROR) {
      throw std::runtime_error("Kafka subscribe: " + RdKafka::err2str(code));
    }
  }

  Options config_;
  std::string topic_;
  std::string subscription_;
  std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
  bool closed_;
};

} // namespace
#endif

class KafkaMessageQueue::Impl {
public:
  explicit Impl(const Options &config) : config(config), closed(false) {}
  Options config;
  bool closed;
  std::mutex mutex;
};

bool kafka_available() {
#ifdef M1104_HAS_RDKAFKA
  return true;
#else
  return false;
#endif
}

KafkaMessageQueue::KafkaMessageQueue(const Options &config)
    : impl_(new Impl(config)) {}

KafkaMessageQueue::~KafkaMessageQueue() { close(); }

std::shared_ptr<AbstractProducer> KafkaMessageQueue::create_producer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const Schema &schema, bool batching_enabled) {
  (void)schema;
  const std::string full_topic =
      build_kafka_topic(tenant, namespace_name, topic);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->closed) {
    throw std::runtime_error("KafkaMessageQueue is closed");
  }
#ifdef M1104_HAS_RDKAFKA
  return std::shared_ptr<AbstractProducer>(new Producer(
      std::shared_ptr<ProducerTransport>(new KafkaProducerTransport(
          impl_->config, full_topic, batching_enabled))));
#else
  (void)full_topic;
  (void)batching_enabled;
  throw std::runtime_error(
      "Kafka support is unavailable; install librdkafka headers and libraries");
#endif
}

std::shared_ptr<AbstractConsumer> KafkaMessageQueue::create_consumer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const std::string &subscription,
    const Schema &schema, SubscriptionType subscription_type,
    const MessageListener &message_listener, const Options &options) {
  (void)schema;
  (void)subscription_type;
  if (subscription.empty()) {
    throw std::invalid_argument("Kafka subscription must not be empty");
  }
  const std::string full_topic =
      build_kafka_topic(tenant, namespace_name, topic);
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->closed) {
    throw std::runtime_error("KafkaMessageQueue is closed");
  }
#ifdef M1104_HAS_RDKAFKA
  Options effective = impl_->config;
  effective.insert(options.begin(), options.end());
  return std::shared_ptr<AbstractConsumer>(new Consumer(
      std::shared_ptr<ConsumerTransport>(
          new KafkaConsumerTransport(effective, full_topic, subscription)),
      message_listener));
#else
  (void)full_topic;
  (void)message_listener;
  (void)options;
  throw std::runtime_error(
      "Kafka support is unavailable; install librdkafka headers and libraries");
#endif
}

void KafkaMessageQueue::close() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->closed = true;
}

} // namespace mq
} // namespace mental1104
