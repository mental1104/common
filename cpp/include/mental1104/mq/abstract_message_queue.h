#pragma once

#include "mental1104/mq/bridge.h"

namespace mental1104 {
namespace mq {

/// 第一版 MQ PR 的兼容工厂 facade。
///
/// 新代码应优先使用 factory.h 和 Producer/AsyncProducer/Consumer Bridge；本接口
/// 仅保留 Python 风格 create_producer/create_consumer 调用方式，不暴露 SDK client。
class AbstractMessageQueue {
public:
  virtual ~AbstractMessageQueue() {}
  AbstractMessageQueue(const AbstractMessageQueue &) = delete;
  AbstractMessageQueue &operator=(const AbstractMessageQueue &) = delete;

  /// 创建共享所有权的 Producer Bridge。
  virtual std::shared_ptr<Producer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) = 0;
  /// 创建共享所有权的 Consumer Bridge。
  virtual std::shared_ptr<Consumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) = 0;
  /// 幂等关闭 facade，后续不再创建对象。
  virtual void close() = 0;

protected:
  /// 仅供具体 Kafka/Pulsar facade 构造。
  AbstractMessageQueue() {}
};

/// 第一版公共名称的兼容别名。
typedef Producer AbstractProducer;
/// 第一版公共名称的兼容别名。
typedef Consumer AbstractConsumer;

} // namespace mq
} // namespace mental1104
