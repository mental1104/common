#pragma once

#include "mental1104/mq/backend.h"
#include "mental1104/mq/bridge.h"

namespace mental1104 {
namespace mq {

/// 第一版实现名称的兼容别名；现在表示 Bridge 的 Producer 实现接口，非 SDK transport。
typedef IProducerBackend ProducerTransport;
/// 第一版实现名称的兼容别名；现在表示 Bridge 的 Consumer 实现接口，非 SDK transport。
typedef IConsumerBackend ConsumerTransport;

} // namespace mq
} // namespace mental1104
