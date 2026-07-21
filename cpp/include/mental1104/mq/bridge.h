#pragma once

#include "mental1104/mq/backend.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace mental1104 {
namespace mq {

class AsyncProducer;
class ProducerState;

class Producer {
public:
  explicit Producer(std::unique_ptr<IProducerBackend> backend);
  explicit Producer(const std::shared_ptr<IProducerBackend> &backend);
  ~Producer() noexcept;
  Producer(const Producer &) = delete;
  Producer &operator=(const Producer &) = delete;
  Producer(Producer &&other) noexcept;
  Producer &operator=(Producer &&other) noexcept;

  SendResult send(const Message &message);
  // Compatibility overload for the first MQ PR.
  void send(const Record &record);
  AsyncProducer async() const;
  void send_async(const Record &record,
                  const SendCallback &callback = SendCallback()) const;
  OperationResult close();

private:
  std::shared_ptr<ProducerState> state_;
  friend class AsyncProducer;
};

class AsyncProducer {
public:
  AsyncProducer();
  explicit AsyncProducer(const std::shared_ptr<ProducerState> &state);
  OperationResult send_async(Message message,
                             const DeliveryCallback &callback) const;
  OperationResult close();

private:
  std::shared_ptr<ProducerState> state_;
};

class ConsumerState;

class Consumer {
public:
  explicit Consumer(std::unique_ptr<IConsumerBackend> backend);
  explicit Consumer(const std::shared_ptr<IConsumerBackend> &backend,
                    const MessageListener &listener = MessageListener());
  ~Consumer() noexcept;
  Consumer(const Consumer &) = delete;
  Consumer &operator=(const Consumer &) = delete;
  Consumer(Consumer &&other) noexcept;
  Consumer &operator=(Consumer &&other) noexcept;

  // Non-blocking. One bridge-owned worker thread invokes handler serially.
  OperationResult start(const MessageHandler &handler);
  // Idempotent. Waits for the current receive/handler call and permits restart.
  OperationResult stop();
  // close() implies stop(), is idempotent, and waits for the worker.
  OperationResult close();
  bool running() const;

  // Compatibility/manual receive API. It cannot be used while start() is
  // active.
  MessagePtr receive(int timeout_millis = -1);
  void acknowledge(const MessagePtr &message);
  void negative_acknowledge(const MessagePtr &message);
  void unsubscribe();
  void resubscribe();

private:
  std::shared_ptr<ConsumerState> state_;
};

} // namespace mq
} // namespace mental1104
