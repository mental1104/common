#include "mental1104/mq/kafka.h"
#include "mental1104/mq/pulsar.h"
#include "mental1104/mq/transport.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace {

class FakeProducerTransport : public mental1104::mq::ProducerTransport {
public:
  FakeProducerTransport() : fail(false), close_count(0) {}
  ~FakeProducerTransport() {
    if (worker.joinable()) {
      worker.join();
    }
  }

  mental1104::mq::SendResult
  send(const mental1104::mq::Record &) override {
    return fail ? mental1104::mq::SendResult::failure("send failed")
                : mental1104::mq::SendResult::success("sync-id");
  }

  void send_async(const mental1104::mq::Record &,
                  const mental1104::mq::SendCallback &callback) override {
    if (worker.joinable()) {
      worker.join();
    }
    worker = std::thread([this, callback]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      callback(fail ? mental1104::mq::SendResult::failure("send failed")
                    : mental1104::mq::SendResult::success("async-id"));
    });
  }

  void close() override {
    ++close_count;
    if (worker.joinable()) {
      worker.join();
    }
  }

  bool fail;
  std::atomic<int> close_count;
  std::thread worker;
};

class FakeConsumerTransport : public mental1104::mq::ConsumerTransport {
public:
  FakeConsumerTransport()
      : ack_count(0), nack_count(0), unsubscribe_count(0),
        resubscribe_count(0), close_count(0) {}

  mental1104::mq::MessagePtr receive(int timeout_millis) override {
    if (timeout_millis == 0) {
      throw std::runtime_error("message receive timed out");
    }
    mental1104::mq::MessagePtr message(new mental1104::mq::Message());
    message->payload = mental1104::mq::make_record("payload");
    return message;
  }
  void acknowledge(const mental1104::mq::MessagePtr &) override {
    ++ack_count;
  }
  void negative_acknowledge(const mental1104::mq::MessagePtr &) override {
    ++nack_count;
  }
  void unsubscribe() override { ++unsubscribe_count; }
  void resubscribe() override { ++resubscribe_count; }
  void close() override { ++close_count; }

  int ack_count;
  int nack_count;
  int unsubscribe_count;
  int resubscribe_count;
  int close_count;
};

TEST(MessageQueue, TopicBuildersMatchPythonSemantics) {
  EXPECT_EQ("tenant.namespace.topic",
            mental1104::mq::build_kafka_topic("tenant", "namespace", "topic"));
  EXPECT_EQ("persistent://tenant/namespace/topic",
            mental1104::mq::build_pulsar_topic("tenant", "namespace", "topic"));
  EXPECT_THROW(mental1104::mq::build_kafka_topic("", "", ""),
               std::invalid_argument);
  EXPECT_THROW(mental1104::mq::build_pulsar_topic("", "namespace", "topic"),
               std::invalid_argument);
}

TEST(MessageQueue, ProducerAsyncCallbackAndCloseAreComplete) {
  std::shared_ptr<FakeProducerTransport> transport(new FakeProducerTransport());
  mental1104::mq::Producer producer(transport);
  producer.send(mental1104::mq::make_record("sync"));

  std::atomic<int> callbacks(0);
  mental1104::mq::SendResult result;
  producer.send_async(
      mental1104::mq::make_record("async"),
      [&callbacks, &result](const mental1104::mq::SendResult &value) {
        result = value;
        ++callbacks;
      });
  producer.close();
  producer.close();

  EXPECT_EQ(1, callbacks.load());
  EXPECT_TRUE(result.ok);
  EXPECT_EQ("async-id", result.message_id);
  EXPECT_EQ(1, transport->close_count.load());
  EXPECT_THROW(producer.send(mental1104::mq::make_record("late")),
               std::runtime_error);
}

TEST(MessageQueue, ProducerPropagatesSyncAndAsyncFailure) {
  std::shared_ptr<FakeProducerTransport> transport(new FakeProducerTransport());
  transport->fail = true;
  mental1104::mq::Producer producer(transport);
  EXPECT_THROW(producer.send(mental1104::mq::make_record("sync")),
               std::runtime_error);

  std::atomic<int> callbacks(0);
  bool ok = true;
  producer.send_async(
      mental1104::mq::make_record("async"),
      [&callbacks, &ok](const mental1104::mq::SendResult &result) {
        ok = result.ok;
        ++callbacks;
      });
  producer.close();
  EXPECT_EQ(1, callbacks.load());
  EXPECT_FALSE(ok);
}

TEST(MessageQueue, ConsumerForwardsLifecycleOperations) {
  std::shared_ptr<FakeConsumerTransport> transport(new FakeConsumerTransport());
  int listener_calls = 0;
  mental1104::mq::Consumer consumer(
      transport, [&listener_calls](const mental1104::mq::MessagePtr &) {
        ++listener_calls;
      });

  mental1104::mq::MessagePtr message = consumer.receive(10);
  EXPECT_EQ("payload", mental1104::mq::record_to_string(message->payload));
  consumer.acknowledge(message);
  consumer.negative_acknowledge(message);
  consumer.unsubscribe();
  consumer.resubscribe();
  consumer.close();
  consumer.close();

  EXPECT_EQ(1, listener_calls);
  EXPECT_EQ(1, transport->ack_count);
  EXPECT_EQ(1, transport->nack_count);
  EXPECT_EQ(1, transport->unsubscribe_count);
  EXPECT_EQ(1, transport->resubscribe_count);
  EXPECT_EQ(1, transport->close_count);
  EXPECT_THROW(consumer.receive(10), std::runtime_error);
}

TEST(MessageQueue, OptionalBackendsFailClearlyWhenUnavailable) {
  if (!mental1104::mq::kafka_available()) {
    mental1104::mq::KafkaMessageQueue queue;
    EXPECT_THROW(queue.create_producer("t", "n", "topic"),
                 std::runtime_error);
  }
  if (!mental1104::mq::pulsar_available()) {
    mental1104::mq::PulsarMessageQueue queue;
    EXPECT_THROW(queue.create_producer("t", "n", "topic"),
                 std::runtime_error);
  }
}

} // namespace
