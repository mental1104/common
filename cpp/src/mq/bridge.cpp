#include "mental1104/mq/bridge.h"

#include <atomic>
#include <future>
#include <utility>

namespace mental1104 {
namespace mq {

class ProducerState : public std::enable_shared_from_this<ProducerState> {
public:
  explicit ProducerState(const std::shared_ptr<IProducerBackend> &b)
      : backend(b), closing(false), closed(false), active_sync(0),
        pending_async(0) {}

  SendResult send(const Message &message) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (closing || closed)
        return SendResult::failure(
            MQError(closed ? ErrorCode::Closed : ErrorCode::Closing, "send",
                    "producer is closed"));
      ++active_sync;
    }
    SendResult result;
    try {
      result = backend->send(message);
    } catch (...) {
      result = SendResult::failure(exception_error("send", std::string()));
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      --active_sync;
    }
    cv.notify_all();
    return result;
  }

  OperationResult send_async(Message message,
                             const DeliveryCallback &callback) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (closing || closed)
        return OperationResult::failure(
            MQError(closed ? ErrorCode::Closed : ErrorCode::Closing,
                    "send_async", "producer is closed"));
      ++pending_async;
    }
    struct Completion {
      std::atomic<bool> done;
      std::shared_ptr<ProducerState> state;
      DeliveryCallback callback;
      Completion(const std::shared_ptr<ProducerState> &s,
                 const DeliveryCallback &cb)
          : done(false), state(s), callback(cb) {}
      void finish(const SendResult &result) {
        if (done.exchange(true))
          return;
        if (callback) {
          std::shared_ptr<std::promise<void>> started(new std::promise<void>());
          std::future<void> ready = started->get_future();
          DeliveryCallback user_callback = callback;
          std::thread([started, user_callback, result]() {
            started->set_value();
            try {
              user_callback(result);
            } catch (...) {
            }
          }).detach();
          ready.wait();
        }
        {
          std::lock_guard<std::mutex> lock(state->mutex);
          if (state->pending_async > 0)
            --state->pending_async;
        }
        state->cv.notify_all();
      }
    };
    std::shared_ptr<Completion> completion(
        new Completion(shared_from_this(), callback));
    OperationResult accepted;
    try {
      accepted =
          backend->send_async(message, [completion](const SendResult &r) {
            completion->finish(r);
          });
    } catch (...) {
      accepted = OperationResult::failure(
          exception_error("send_async", std::string()));
    }
    if (!accepted.ok && !completion->done.load()) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (pending_async > 0)
          --pending_async;
      }
      cv.notify_all();
    } else if (!accepted.ok && completion->done.load()) {
      // The backend already completed synchronously; the request was accepted.
      return OperationResult::success();
    }
    return accepted;
  }

  OperationResult close() {
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (closed)
        return close_result;
      if (closing) {
        cv.wait(lock, [this]() { return closed; });
        return close_result;
      }
      closing = true;
      cv.wait(lock, [this]() { return active_sync == 0; });
    }
    OperationResult backend_result;
    try {
      backend_result = backend->close();
    } catch (...) {
      backend_result =
          OperationResult::failure(exception_error("close", std::string()));
    }
    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [this]() { return pending_async == 0; });
      close_result = backend_result;
      closed = true;
      closing = false;
    }
    cv.notify_all();
    return backend_result;
  }

  std::shared_ptr<IProducerBackend> backend;
  std::mutex mutex;
  std::condition_variable cv;
  bool closing;
  bool closed;
  std::size_t active_sync;
  std::size_t pending_async;
  OperationResult close_result;
};

Producer::Producer(std::unique_ptr<IProducerBackend> b)
    : state_(
          new ProducerState(std::shared_ptr<IProducerBackend>(std::move(b)))) {
  if (!state_->backend)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Producer",
                              "backend must not be null"));
}
Producer::Producer(const std::shared_ptr<IProducerBackend> &b)
    : state_(new ProducerState(b)) {
  if (!b)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Producer",
                              "backend must not be null"));
}
Producer::~Producer() noexcept {
  if (state_) {
    try {
      state_->close();
    } catch (...) {
    }
  }
}
Producer::Producer(Producer &&other) noexcept
    : state_(std::move(other.state_)) {}
Producer &Producer::operator=(Producer &&other) noexcept {
  if (this != &other) {
    if (state_) {
      try {
        state_->close();
      } catch (...) {
      }
    }
    state_ = std::move(other.state_);
  }
  return *this;
}
SendResult Producer::send(const Message &message) {
  return state_ ? state_->send(message)
                : SendResult::failure(MQError(ErrorCode::Closed, "send",
                                              "producer has no backend"));
}
void Producer::send(const Record &record) {
  Message m;
  m.payload = record;
  SendResult r = send(m);
  if (!r.ok)
    throw MQException(r.error);
}
AsyncProducer Producer::async() const { return AsyncProducer(state_); }
void Producer::send_async(const Record &record,
                          const SendCallback &callback) const {
  Message m;
  m.payload = record;
  OperationResult r = async().send_async(m, callback);
  if (!r.ok)
    throw MQException(r.error);
}
OperationResult Producer::close() {
  return state_ ? state_->close() : OperationResult::success();
}

AsyncProducer::AsyncProducer() {}
AsyncProducer::AsyncProducer(const std::shared_ptr<ProducerState> &s)
    : state_(s) {}
OperationResult AsyncProducer::send_async(Message m,
                                          const DeliveryCallback &cb) const {
  return state_
             ? state_->send_async(m, cb)
             : OperationResult::failure(MQError(ErrorCode::Closed, "send_async",
                                                "producer has no backend"));
}
OperationResult AsyncProducer::close() {
  return state_ ? state_->close() : OperationResult::success();
}

class ConsumerState : public std::enable_shared_from_this<ConsumerState> {
public:
  explicit ConsumerState(const std::shared_ptr<IConsumerBackend> &b,
                         const MessageListener &l)
      : backend(b), listener(l), running(false), stopping(false),
        closed(false) {}
  ~ConsumerState() {
    try {
      close();
    } catch (...) {
    }
  }

  OperationResult start(const MessageHandler &handler) {
    std::lock_guard<std::mutex> lock(mutex);
    if (closed)
      return OperationResult::failure(
          MQError(ErrorCode::Closed, "start", "consumer is closed"));
    if (running)
      return OperationResult::failure(MQError(
          ErrorCode::AlreadyStarted, "start", "consumer is already running"));
    if (!handler)
      return OperationResult::failure(MQError(ErrorCode::InvalidConfig, "start",
                                              "handler must not be empty"));
    if (worker.joinable())
      worker.join();
    current_handler = handler;
    stopping = false;
    running = true;
    std::shared_ptr<ConsumerState> self = shared_from_this();
    worker = std::thread([self]() { self->run_loop(); });
    return OperationResult::success();
  }

  void run_loop() {
    for (;;) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping || closed)
          break;
      }
      ReceiveResult received;
      try {
        received = backend->receive(100);
      } catch (...) {
        received =
            ReceiveResult::failure(exception_error("receive", std::string()));
      }
      if (!received.ok) {
        if (received.error.code == ErrorCode::Timeout ||
            received.error.code == ErrorCode::Canceled)
          continue;
        std::lock_guard<std::mutex> lock(mutex);
        last_error = received.error;
        break;
      }
      HandlerResult handled;
      try {
        handled = current_handler(received.value.message);
      } catch (const std::exception &e) {
        handled = HandlerResult::failure(
            MQError(ErrorCode::Handler, "handler", e.what()));
      } catch (...) {
        handled = HandlerResult::failure(
            MQError(ErrorCode::Handler, "handler",
                    "handler threw an unknown exception"));
      }
      OperationResult action = OperationResult::success();
      if (!handled.error.empty() ||
          handled.action == ConsumeAction::NegativeAcknowledge)
        action = backend->negative_acknowledge(received.value.receipt);
      else if (handled.action == ConsumeAction::Acknowledge)
        action = backend->acknowledge(received.value.receipt);
      if (!action.ok) {
        std::lock_guard<std::mutex> lock(mutex);
        last_error = action.error;
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      running = false;
      stopping = false;
    }
    cv.notify_all();
  }

  OperationResult stop() {
    std::thread joiner;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!running && !worker.joinable())
        return OperationResult::success();
      stopping = true;
      if (worker.joinable())
        joiner = std::move(worker);
    }
    if (joiner.joinable()) {
      if (joiner.get_id() == std::this_thread::get_id()) {
        joiner.detach();
        return OperationResult::success();
      }
      joiner.join();
    }
    return OperationResult::success();
  }

  OperationResult close() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (closed)
        return close_result;
      closed = true;
      stopping = true;
    }
    stop();
    OperationResult r;
    try {
      r = backend->close();
    } catch (...) {
      r = OperationResult::failure(exception_error("close", std::string()));
    }
    std::lock_guard<std::mutex> lock(mutex);
    close_result = r;
    return r;
  }

  MessagePtr receive_manual(int timeout) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (closed)
        throw MQException(
            MQError(ErrorCode::Closed, "receive", "consumer is closed"));
      if (running)
        throw MQException(
            MQError(ErrorCode::AlreadyStarted, "receive",
                    "manual receive is unavailable while consumer is started"));
    }
    ReceiveResult r = backend->receive(timeout);
    if (!r.ok)
      throw MQException(r.error);
    MessagePtr m(new Message(r.value.message));
    {
      std::lock_guard<std::mutex> lock(mutex);
      receipts[m.get()] = r.value.receipt;
    }
    if (listener)
      listener(m);
    return m;
  }

  ReceiptPtr take_receipt(const MessagePtr &m) {
    if (!m)
      throw MQException(MQError(ErrorCode::InvalidMessage, "receipt",
                                "message must not be null"));
    std::lock_guard<std::mutex> lock(mutex);
    std::map<const Message *, ReceiptPtr>::iterator it = receipts.find(m.get());
    if (it == receipts.end())
      throw MQException(MQError(ErrorCode::InvalidMessage, "receipt",
                                "message does not belong to this consumer"));
    ReceiptPtr r = it->second;
    receipts.erase(it);
    return r;
  }

  std::shared_ptr<IConsumerBackend> backend;
  MessageListener listener;
  MessageHandler current_handler;
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::thread worker;
  bool running;
  bool stopping;
  bool closed;
  MQError last_error;
  OperationResult close_result;
  std::map<const Message *, ReceiptPtr> receipts;
};

Consumer::Consumer(std::unique_ptr<IConsumerBackend> b)
    : state_(new ConsumerState(std::shared_ptr<IConsumerBackend>(std::move(b)),
                               MessageListener())) {
  if (!state_->backend)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Consumer",
                              "backend must not be null"));
}
Consumer::Consumer(const std::shared_ptr<IConsumerBackend> &b,
                   const MessageListener &l)
    : state_(new ConsumerState(b, l)) {
  if (!b)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Consumer",
                              "backend must not be null"));
}
Consumer::~Consumer() noexcept {
  if (state_) {
    try {
      state_->close();
    } catch (...) {
    }
  }
}
Consumer::Consumer(Consumer &&o) noexcept : state_(std::move(o.state_)) {}
Consumer &Consumer::operator=(Consumer &&o) noexcept {
  if (this != &o) {
    if (state_) {
      try {
        state_->close();
      } catch (...) {
      }
    }
    state_ = std::move(o.state_);
  }
  return *this;
}
OperationResult Consumer::start(const MessageHandler &h) {
  return state_ ? state_->start(h)
                : OperationResult::failure(MQError(ErrorCode::Closed, "start",
                                                   "consumer has no backend"));
}
OperationResult Consumer::stop() {
  return state_ ? state_->stop() : OperationResult::success();
}
OperationResult Consumer::close() {
  return state_ ? state_->close() : OperationResult::success();
}
bool Consumer::running() const {
  if (!state_)
    return false;
  std::lock_guard<std::mutex> lock(state_->mutex);
  return state_->running;
}
MessagePtr Consumer::receive(int t) { return state_->receive_manual(t); }
void Consumer::acknowledge(const MessagePtr &m) {
  OperationResult r = state_->backend->acknowledge(state_->take_receipt(m));
  if (!r.ok)
    throw MQException(r.error);
}
void Consumer::negative_acknowledge(const MessagePtr &m) {
  OperationResult r =
      state_->backend->negative_acknowledge(state_->take_receipt(m));
  if (!r.ok)
    throw MQException(r.error);
}
void Consumer::unsubscribe() {
  OperationResult r = state_->backend->unsubscribe();
  if (!r.ok)
    throw MQException(r.error);
}
void Consumer::resubscribe() {
  OperationResult r = state_->backend->resubscribe();
  if (!r.ok)
    throw MQException(r.error);
}

} // namespace mq
} // namespace mental1104
