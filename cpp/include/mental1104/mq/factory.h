#pragma once

#include "mental1104/mq/bridge.h"

namespace mental1104 {
namespace mq {

std::unique_ptr<IProducerBackend>
create_producer_backend(const ProducerConfig &config);
std::unique_ptr<IConsumerBackend>
create_consumer_backend(const ConsumerConfig &config);
Producer create_producer(const ProducerConfig &config);
Consumer create_consumer(const ConsumerConfig &config);

} // namespace mq
} // namespace mental1104
