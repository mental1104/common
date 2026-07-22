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

/// 默认等待 librdkafka flush 十秒。
KafkaBackendConfig::KafkaBackendConfig() : close_timeout_millis(10000) {}

/// 返回 Kafka 后端标识。
BackendType KafkaBackendConfig::backend_type() const {
  return BackendType::Kafka;
}

#ifdef M1104_HAS_RDKAFKA
namespace {

/// 构造带 Kafka 后端名称的统一错误。
MQError kafka_error(const std::string &operation, const std::string &message,
                    bool retryable = false) {
  return MQError(ErrorCode::Backend, operation, message, "kafka", retryable);
}

/// 把字符串配置写入 librdkafka Conf。
/// 任一配置键无效时立即抛 MQException，避免创建部分配置的 client。
void apply_options(RdKafka::Conf &conf, const Options &options) {
  std::string error;
  for (Options::const_iterator it = options.begin(); it != options.end(); ++it)
    if (conf.set(it->first, it->second, error) != RdKafka::Conf::CONF_OK)
      throw MQException(MQError(ErrorCode::InvalidConfig, "kafka_config",
                                it->first + ": " + error, "kafka"));
}

/// 单次异步发送随 msg_opaque 交给 librdkafka 的 callback 所有权对象。
/// produce 成功后由 dr_cb 通过 unique_ptr 接管并释放；同步拒绝时由提交路径释放。
struct Delivery {
  explicit Delivery(const DeliveryCallback &callback) : callback(callback) {}
  DeliveryCallback callback;
};

/// librdkafka Producer 的 Bridge backend。
///
/// producer_ 和 poller_ 由本对象独占；poller_ 持续驱动 delivery report。mutex_ 只保护
/// closed_ 与提交/关闭边界，delivery callback 由 librdkafka/poller 线程触发。
class KafkaProducerBackend : public IProducerBackend,
                             public RdKafka::DeliveryReportCb {
public:
  /// 创建配置、Producer 和 poll 线程。
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
      // 退出前再处理一次已经进入本地队列的 callback。
      producer_->poll(0);
    });
  }

  /// 尽力 flush 并 join poller，不允许异常越过析构边界。
  ~KafkaProducerBackend() {
    try {
      close();
    } catch (...) {
    }
  }

  /// 复用 send_async 与 promise/future 实现同步发送，保持同一 delivery 语义。
  SendResult send(const Message &message) override {
    std::promise<SendResult> promise;
    std::future<SendResult> future = promise.get_future();
    OperationResult accepted = send_async(
        message, [&promise](const SendResult &result) {
          promise.set_value(result);
        });
    if (!accepted.ok)
      return SendResult::failure(accepted.error);
    return future.get();
  }

  /// 把公共 Message 复制到 librdkafka，并提交一次异步 produce。
  /// produce 同步失败时不调用 callback；成功后 Delivery 的所有权转交 dr_cb。
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
      // SDK 未接受请求，提交路径仍拥有 headers 与 Delivery。
      delete raw_headers;
      delete delivery;
      return OperationResult::failure(
          kafka_error("send_async", RdKafka::err2str(code),
                      code == RdKafka::ERR__QUEUE_FULL));
    }
    producer_->poll(0);
    return OperationResult::success();
  }

  /// librdkafka delivery report 回调。
  /// msg_opaque 的 Delivery 在本调用内唯一释放，callback 最多执行一次。
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
    } else {
      delivery->callback(
          SendResult::failure(kafka_error("delivery", message.errstr(), true)));
    }
  }

  /// 幂等 flush producer，并停止、join poller 线程。
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

/// 保存一条已消费的 librdkafka Message，供 ack/nack 使用。
class KafkaReceipt : public Receipt {
public:
  explicit KafkaReceipt(RdKafka::Message *value) : message(value) {}
  std::unique_ptr<RdKafka::Message> message;
};

/// librdkafka Consumer 的 Bridge backend。
///
/// consumer_ 由本对象独占，mutex_ 串行化 receive、ack/nack、订阅与关闭，确保
/// Receipt 中的原生 Message 不与 client 销毁并发访问。
class KafkaConsumerBackend : public IConsumerBackend {
public:
  /// 保存配置并立即建立 consumer group 订阅。
  KafkaConsumerBackend(const ConsumerConfig &config,
                       const KafkaBackendConfig &backend)
      : options_(backend.options), topic_(build_kafka_topic(config.topic)),
        subscription_(config.subscription), closed_(false) {
    subscribe_new();
  }

  /// 尽力关闭 consumer，不允许异常越过析构边界。
  ~KafkaConsumerBackend() {
    try {
      close();
    } catch (...) {
    }
  }

  /// 同步拉取一条消息并复制公共字段；原生 Message 转交 KafkaReceipt 所有。
  ReceiveResult receive(int timeout_millis) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_ || !consumer_)
      return ReceiveResult::failure(
          MQError(ErrorCode::Closed, "receive", "consumer is closed", "kafka"));
    RdKafka::Message *native =
        consumer_->consume(timeout_millis < 0 ? 1000 : timeout_millis);
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
      std::string error = native->errstr();
      delete native;
      return ReceiveResult::failure(kafka_error("receive", error, true));
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

  /// 同步提交 Receipt 对应 offset。
  OperationResult acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<KafkaReceipt> kafka_receipt =
        std::dynamic_pointer_cast<KafkaReceipt>(receipt);
    if (!kafka_receipt || !kafka_receipt->message)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "acknowledge",
                  "invalid Kafka receipt", "kafka"));
    RdKafka::ErrorCode code =
        consumer_->commitSync(kafka_receipt->message.get());
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(
                     kafka_error("acknowledge", RdKafka::err2str(code), true));
  }

  /// seek 回 Receipt 对应 offset，实现 Kafka 的否认/重消费语义。
  OperationResult negative_acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<KafkaReceipt> kafka_receipt =
        std::dynamic_pointer_cast<KafkaReceipt>(receipt);
    if (!kafka_receipt || !kafka_receipt->message)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "negative_acknowledge",
                  "invalid Kafka receipt", "kafka"));
    std::unique_ptr<RdKafka::TopicPartition> partition(
        RdKafka::TopicPartition::create(
            kafka_receipt->message->topic_name(),
            kafka_receipt->message->partition(),
            kafka_receipt->message->offset()));
    RdKafka::ErrorCode code = consumer_->seek(*partition, 5000);
    return code == RdKafka::ERR_NO_ERROR
               ? OperationResult::success()
               : OperationResult::failure(kafka_error(
                     "negative_acknowledge", RdKafka::err2str(code), true));
  }

  /// 从当前 topic 取消订阅；consumer group 元数据仍由 broker 管理。
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

  /// 使用同一 consumer 重新订阅原 topic。
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

  /// 幂等关闭 KafkaConsumer，并释放 client 所有权。
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
  /// 创建配置、KafkaConsumer，并订阅原 topic。
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

/// 报告当前构建是否包含 librdkafka。
bool kafka_available() {
#ifdef M1104_HAS_RDKAFKA
  return true;
#else
  return false;
#endif
}

/// 创建 Kafka Producer backend；未编译 librdkafka 时抛统一配置异常。
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

/// 创建 Kafka Consumer backend；未编译 librdkafka 时抛统一配置异常。
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

/// 保存兼容 facade 的全局 Kafka 配置。
KafkaMessageQueue::KafkaMessageQueue(const Options &options)
    : options_(options), closed_(false) {}

/// 析构时幂等关闭 facade，不向外抛异常。
KafkaMessageQueue::~KafkaMessageQueue() noexcept {
  try {
    close();
  } catch (...) {
  }
}

/// 从兼容参数构造 Kafka ProducerConfig 和 Producer Bridge。
std::shared_ptr<Producer> KafkaMessageQueue::create_producer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const Schema &, bool batching_enabled) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_producer",
                              "message queue is closed", "kafka"));
  std::shared_ptr<KafkaBackendConfig> backend(new KafkaBackendConfig());
  backend->options = options_;
  ProducerConfig config;
  config.topic.tenant = tenant;
  config.topic.namespace_name = namespace_name;
  config.topic.topic = topic;
  config.batching_enabled = batching_enabled;
  config.backend = backend;
  return std::shared_ptr<Producer>(new Producer(create_producer_backend(config)));
}

/// 从兼容参数构造 Kafka ConsumerConfig 和 Consumer Bridge。
std::shared_ptr<Consumer> KafkaMessageQueue::create_consumer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const std::string &subscription, const Schema &,
    SubscriptionType type, const MessageListener &listener,
    const Options &options) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_consumer",
                              "message queue is closed", "kafka"));
  std::shared_ptr<KafkaBackendConfig> backend(new KafkaBackendConfig());
  backend->options = options_;
  backend->options.insert(options.begin(), options.end());
  ConsumerConfig config;
  config.topic.tenant = tenant;
  config.topic.namespace_name = namespace_name;
  config.topic.topic = topic;
  config.subscription = subscription;
  config.subscription_type = type;
  config.backend = backend;
  return std::shared_ptr<Consumer>(new Consumer(
      std::shared_ptr<IConsumerBackend>(
          create_consumer_backend(config).release()),
      listener));
}

/// 幂等关闭兼容 facade；已创建 Bridge 的资源由各对象自行管理。
void KafkaMessageQueue::close() { closed_ = true; }

} // namespace mq
} // namespace mental1104
