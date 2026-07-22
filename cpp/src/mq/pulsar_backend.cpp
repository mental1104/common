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

/// 默认等待 Pulsar Producer 刷新十秒。
PulsarBackendConfig::PulsarBackendConfig() : close_timeout_millis(10000) {}

/// 返回 Pulsar 后端标识。
BackendType PulsarBackendConfig::backend_type() const {
  return BackendType::Pulsar;
}

#ifdef M1104_HAS_PULSAR
namespace {

/// 把 Pulsar Result 转换为统一 MQError。
MQError pulsar_error(const std::string &operation, pulsar::Result result,
                     bool retryable = false) {
  return MQError(result == pulsar::ResultTimeout ? ErrorCode::Timeout
                                                 : ErrorCode::Backend,
                 operation, pulsar::strResult(result), "pulsar", retryable);
}

/// 序列化 Pulsar MessageId 为公共字符串标识。
std::string message_id(const pulsar::MessageId &id) {
  std::string value;
  id.serialize(value);
  return value;
}

/// 把公共 Message 复制为 Pulsar Message。
pulsar::Message build_message(const Message &message) {
  pulsar::MessageBuilder builder;
  builder.setContent(
      std::string(message.payload.begin(), message.payload.end()));
  if (!message.key.empty())
    builder.setPartitionKey(std::string(message.key.begin(), message.key.end()));
  if (!message.headers.empty())
    builder.setProperties(message.headers);
  return builder.build();
}

/// 把公共订阅类型映射为 Pulsar ConsumerType。
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

/// pulsar-client-cpp Producer 的 Bridge backend。
///
/// client_ 和 producer_ 由本对象独占。Pulsar SDK 自行管理异步 I/O 线程；用户 callback
/// 由 SDK callback 进入 Bridge completion，再由 Bridge 隔离线程执行。
class PulsarProducerBackend : public IProducerBackend {
public:
  /// 创建 Pulsar Client 和 Producer。
  PulsarProducerBackend(const ProducerConfig &config,
                        const PulsarBackendConfig &backend)
      : client_(backend.service_url), closed_(false) {
    pulsar::ProducerConfiguration producer_config;
    producer_config.setBatchingEnabled(config.batching_enabled);
    pulsar::Result result = client_.createProducer(
        build_pulsar_topic(config.topic), producer_config, producer_);
    if (result != pulsar::ResultOk)
      throw MQException(pulsar_error("create_producer", result));
  }

  /// 尽力刷新并关闭，不允许异常越过析构边界。
  ~PulsarProducerBackend() {
    try {
      close();
    } catch (...) {
    }
  }

  /// 同步发送并返回 broker MessageId/partition。
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

  /// 使用 Pulsar 原生 sendAsync 提交请求。
  /// SDK 同步抛异常时返回失败且不调用 callback；接受后 SDK 最终回调一次。
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

  /// 幂等刷新 Producer，再关闭 Producer 和 Client。
  /// mutex_ 串行化多个 close 调用；closed_ 在开始关闭时立即拒绝新发送。
  OperationResult close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_.exchange(true))
      return OperationResult::success();
    pulsar::Result flush_result = producer_.flush();
    pulsar::Result producer_result = producer_.close();
    pulsar::Result client_result = client_.close();
    if (flush_result != pulsar::ResultOk)
      return OperationResult::failure(
          pulsar_error("flush", flush_result, true));
    if (producer_result != pulsar::ResultOk)
      return OperationResult::failure(
          pulsar_error("close_producer", producer_result));
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

/// 保存一条 Pulsar 原生 Message，供 ack/nack 使用。
class PulsarReceipt : public Receipt {
public:
  explicit PulsarReceipt(const pulsar::Message &value) : message(value) {}
  pulsar::Message message;
};

/// pulsar-client-cpp Consumer 的 Bridge backend。
///
/// client_ 与 consumer_ 由本对象独占；mutex_ 串行化 receive、确认、重订阅和关闭，
/// 防止 SDK Consumer 与 Receipt 确认操作并发销毁。
class PulsarConsumerBackend : public IConsumerBackend {
public:
  /// 保存订阅配置并立即创建 Consumer。
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

  /// 尽力关闭 Consumer 和 Client，不允许异常越过析构边界。
  ~PulsarConsumerBackend() {
    try {
      close();
    } catch (...) {
    }
  }

  /// 拉取一条 Pulsar Message，并复制为公共 Message + PulsarReceipt。
  ReceiveResult receive(int timeout_millis) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return ReceiveResult::failure(MQError(ErrorCode::Closed, "receive",
                                            "consumer is closed", "pulsar"));
    pulsar::Message native;
    pulsar::Result result = timeout_millis < 0
                                ? consumer_.receive(native)
                                : consumer_.receive(native, timeout_millis);
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

  /// 确认 PulsarReceipt 中保存的原生消息。
  OperationResult acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<PulsarReceipt> pulsar_receipt =
        std::dynamic_pointer_cast<PulsarReceipt>(receipt);
    if (!pulsar_receipt)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "acknowledge",
                  "invalid Pulsar receipt", "pulsar"));
    pulsar::Result result = consumer_.acknowledge(pulsar_receipt->message);
    return result == pulsar::ResultOk
               ? OperationResult::success()
               : OperationResult::failure(
                     pulsar_error("acknowledge", result, true));
  }

  /// 否认 PulsarReceipt 中保存的原生消息。
  /// negativeAcknowledge 不返回结果，重投时机由 broker 配置决定。
  OperationResult negative_acknowledge(const ReceiptPtr &receipt) override {
    std::lock_guard<std::mutex> lock(mutex_);
    std::shared_ptr<PulsarReceipt> pulsar_receipt =
        std::dynamic_pointer_cast<PulsarReceipt>(receipt);
    if (!pulsar_receipt)
      return OperationResult::failure(
          MQError(ErrorCode::InvalidMessage, "negative_acknowledge",
                  "invalid Pulsar receipt", "pulsar"));
    consumer_.negativeAcknowledge(pulsar_receipt->message);
    return OperationResult::success();
  }

  /// 删除当前 Pulsar subscription。
  OperationResult unsubscribe() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "unsubscribe",
                                              "consumer is closed", "pulsar"));
    pulsar::Result result = consumer_.unsubscribe();
    return result == pulsar::ResultOk
               ? OperationResult::success()
               : OperationResult::failure(
                     pulsar_error("unsubscribe", result));
  }

  /// 关闭旧 Consumer，并按原 topic/subscription/type 创建新 Consumer。
  OperationResult resubscribe() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::failure(MQError(ErrorCode::Closed, "resubscribe",
                                              "consumer is closed", "pulsar"));
    consumer_.close();
    return subscribe_new_result();
  }

  /// 幂等关闭 Consumer 和 Client。
  OperationResult close() override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_)
      return OperationResult::success();
    closed_ = true;
    pulsar::Result consumer_result = consumer_.close();
    pulsar::Result client_result = client_.close();
    if (consumer_result != pulsar::ResultOk)
      return OperationResult::failure(
          pulsar_error("close_consumer", consumer_result));
    if (client_result != pulsar::ResultOk)
      return OperationResult::failure(
          pulsar_error("close_client", client_result));
    return OperationResult::success();
  }

private:
  /// 使用保存的 topic/subscription/type 创建 Consumer，并以结果形式返回。
  OperationResult subscribe_new_result() {
    pulsar::ConsumerConfiguration consumer_config;
    consumer_config.setConsumerType(consumer_type(type_));
    pulsar::Result result =
        client_.subscribe(topic_, subscription_, consumer_config, consumer_);
    return result == pulsar::ResultOk
               ? OperationResult::success()
               : OperationResult::failure(
                     pulsar_error("subscribe", result));
  }

  /// 创建 Consumer，失败时转换为兼容 MQException。
  void subscribe_new() {
    OperationResult result = subscribe_new_result();
    if (!result.ok)
      throw MQException(result.error);
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

/// 报告当前构建是否包含 pulsar-client-cpp。
bool pulsar_available() {
#ifdef M1104_HAS_PULSAR
  return true;
#else
  return false;
#endif
}

/// 创建 Pulsar Producer backend；未编译 SDK 时抛统一配置异常。
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

/// 创建 Pulsar Consumer backend；未编译 SDK 时抛统一配置异常。
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

/// 保存兼容 facade 的 Pulsar 配置。
PulsarMessageQueue::PulsarMessageQueue(const Options &options)
    : options_(options), closed_(false) {}

/// 析构时幂等关闭 facade，不向外抛异常。
PulsarMessageQueue::~PulsarMessageQueue() noexcept {
  try {
    close();
  } catch (...) {
  }
}

/// 从兼容参数构造 Pulsar ProducerConfig 和 Producer Bridge。
std::shared_ptr<Producer> PulsarMessageQueue::create_producer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const Schema &, bool batching_enabled) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_producer",
                              "message queue is closed", "pulsar"));
  std::shared_ptr<PulsarBackendConfig> backend(new PulsarBackendConfig());
  Options::const_iterator it = options_.find("service.url");
  if (it != options_.end())
    backend->service_url = it->second;
  backend->options = options_;
  ProducerConfig config;
  config.topic.tenant = tenant;
  config.topic.namespace_name = namespace_name;
  config.topic.topic = topic;
  config.batching_enabled = batching_enabled;
  config.backend = backend;
  return std::shared_ptr<Producer>(new Producer(create_producer_backend(config)));
}

/// 从兼容参数构造 Pulsar ConsumerConfig 和 Consumer Bridge。
std::shared_ptr<Consumer> PulsarMessageQueue::create_consumer(
    const std::string &tenant, const std::string &namespace_name,
    const std::string &topic, const std::string &subscription, const Schema &,
    SubscriptionType type, const MessageListener &listener,
    const Options &options) {
  if (closed_)
    throw MQException(MQError(ErrorCode::Closed, "create_consumer",
                              "message queue is closed", "pulsar"));
  std::shared_ptr<PulsarBackendConfig> backend(new PulsarBackendConfig());
  Options merged = options_;
  merged.insert(options.begin(), options.end());
  Options::const_iterator it = merged.find("service.url");
  if (it != merged.end())
    backend->service_url = it->second;
  backend->options = merged;
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
void PulsarMessageQueue::close() { closed_ = true; }

} // namespace mq
} // namespace mental1104
