#pragma once

#include "mental1104/mq/bridge.h"

namespace mental1104 {
namespace mq {

/// 根据 ProducerConfig 的多态 backend 配置创建具体 Producer backend。
/// @param config 公共 topic/批处理配置和非空后端配置。
/// @return 由调用方独占的 backend。
/// @throws MQException 配置类型不匹配、字段无效或 SDK 构造失败。
std::unique_ptr<IProducerBackend>
create_producer_backend(const ProducerConfig &config);

/// 根据 ConsumerConfig 的多态 backend 配置创建具体 Consumer backend。
/// @param config 公共 topic/订阅配置和非空后端配置。
/// @return 由调用方独占的 backend。
/// @throws MQException 配置类型不匹配、字段无效或 SDK 构造失败。
std::unique_ptr<IConsumerBackend>
create_consumer_backend(const ConsumerConfig &config);

/// 创建 Producer Bridge；Factory 只选择实现，不承载发送或关闭逻辑。
Producer create_producer(const ProducerConfig &config);
/// 创建 Consumer Bridge；消费线程由返回对象管理。
Consumer create_consumer(const ConsumerConfig &config);

} // namespace mq
} // namespace mental1104
