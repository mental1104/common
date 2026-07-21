#pragma once

#include "mental1104/mq/abstract_message_queue.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace mental1104 {
namespace mq {

class ProducerTransport {
public:
  virtual ~ProducerTransport() {}
  virtual SendResult send(const Record &record) = 0;
  virtual void send_async(const Record &record, const SendCallback &callback) = 0;
  virtual void close() = 0;
};

class ConsumerTransport {
public:
  virtual ~ConsumerTransport() {}
  virtual MessagePtr receive(int timeout_millis) = 0;
  virtual void acknowledge(const MessagePtr &message) = 0;
  virtual void negative_acknowledge(const MessagePtr &message) = 0;
  virtual void unsubscribe() = 0;
  virtual void resubscribe() = 0;
  virtual void close() = 0;
};

class Producer : public AbstractProducer {
public:
  explicit Producer(const std::shared_ptr<ProducerTransport> &transport);
  ~Producer();

  void send(const Record &record) override;
  void send_async(const Record &record,
                  const SendCallback &callback = SendCallback()) override;
  void close() override;

private:
  std::shared_ptr<ProducerTransport> transport_;
  std::mutex mutex_;
  std::condition_variable pending_cv_;
  bool closed_;
  std::size_t pending_;
};

class Consumer : public AbstractConsumer {
public:
  Consumer(const std::shared_ptr<ConsumerTransport> &transport,
           const MessageListener &listener = MessageListener());
  ~Consumer();

  MessagePtr receive(int timeout_millis = -1) override;
  void acknowledge(const MessagePtr &message) override;
  void negative_acknowledge(const MessagePtr &message) override;
  void unsubscribe() override;
  void resubscribe() override;
  void close() override;

private:
  void ensure_open() const;

  std::shared_ptr<ConsumerTransport> transport_;
  MessageListener listener_;
  mutable std::mutex mutex_;
  bool closed_;
};

} // namespace mq
} // namespace mental1104
