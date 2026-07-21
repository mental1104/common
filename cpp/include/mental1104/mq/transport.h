#pragma once

#include "mental1104/mq/backend.h"
#include "mental1104/mq/bridge.h"

namespace mental1104 {
namespace mq {
// Compatibility aliases for the initial implementation. They now represent
// the implementation side of the Bridge rather than SDK-facing transports.
typedef IProducerBackend ProducerTransport;
typedef IConsumerBackend ConsumerTransport;
} // namespace mq
} // namespace mental1104
