#include "mental1104/mq/bridge.h"

#include <atomic>
#include <future>
#include <utility>

namespace mental1104 {
namespace mq {

/// Producer 与 AsyncProducer 共享的线程安全状态。
///
/// mutex/cv 保护关闭状态和操作计数；backend 的所有权由 shared_ptr 持有，保证异步
/// completion 在 Producer facade 销毁后仍可安全完成。close 等待同步调用、backend
/// 关闭以及所有已接受 callback 完成派发。
class ProducerState : public std::enable_shared_from_this<ProducerState> {
public:
  /// @param backend 非空 Producer backend 的共享所有权。
  explicit ProducerState(const std::shared_ptr<IProducerBackend> &backend)
      : backend(backend), closing(false), closed(false), active_sync(0),
        pending_async(0) {}

  /// 同步转发一条消息，并把后端异常转换为 SendResult。
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

  /// 提交异步发送并建立 exactly-once completion 门禁。
  ///
  /// backend 同步拒绝时直接减少 pending 计数且不调用用户 callback；一旦 backend
  /// 接受，Completion 保持 ProducerState 所有权并保证最终只派发一次结果。
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

    /// 单个异步请求的完成状态。
    struct Completion {
      std::atomic<bool> done;
      std::shared_ptr<ProducerState> state;
      DeliveryCallback callback;

      /// 保存共享状态，避免 callback 到达前 Bridge 状态悬空。
      Completion(const std::shared_ptr<ProducerState> &state,
                 const DeliveryCallback &callback)
          : done(false), state(state), callback(callback) {}

      /// exactly-once 派发最终结果并释放 pending 计数。
      ///
      /// 用户 callback 在 detached 线程中执行；started promise 只确认线程已启动，
      /// close 不等待用户业务逻辑任意长时间。callback 异常被线程边界捕获。
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
              // 用户 callback 不能破坏 backend callback 线程或关闭状态。
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
          backend->send_async(message, [completion](const SendResult &result) {
            completion->finish(result);
          });
    } catch (...) {
      accepted = OperationResult::failure(
          exception_error("send_async", std::string()));
    }
    if (!accepted.ok && !completion->done.load()) {
      // 同步拒绝不属于“已接受请求”，因此不调用用户 callback。
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (pending_async > 0)
          --pending_async;
      }
      cv.notify_all();
    } else if (!accepted.ok && completion->done.load()) {
      // 后端已经同步完成 callback；该请求按已接受成功处理。
      return OperationResult::success();
    }
    return accepted;
  }

  /// 幂等关闭共享 backend。
  ///
  /// 首个调用负责真正关闭；并发调用等待 closed。顺序为等待同步 send、关闭 backend、
  /// 再等待已接受异步请求完成派发。
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

/// 接管 unique_ptr backend 并转换为异步安全的共享状态。
Producer::Producer(std::unique_ptr<IProducerBackend> backend)
    : state_(new ProducerState(
          std::shared_ptr<IProducerBackend>(std::move(backend)))) {
  if (!state_->backend)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Producer",
                              "backend must not be null"));
}

/// 使用已有共享 backend 创建 Producer。
Producer::Producer(const std::shared_ptr<IProducerBackend> &backend)
    : state_(new ProducerState(backend)) {
  if (!backend)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Producer",
                              "backend must not be null"));
}

/// 析构时尽力关闭，不允许异常越过析构边界。
Producer::~Producer() noexcept {
  if (state_) {
    try {
      state_->close();
    } catch (...) {
    }
  }
}

/// 移动共享状态，源对象变为空 facade。
Producer::Producer(Producer &&other) noexcept
    : state_(std::move(other.state_)) {}

/// 关闭当前状态后接管源对象状态。
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

/// 转发同步发送；空 facade 返回 Closed。
SendResult Producer::send(const Message &message) {
  return state_ ? state_->send(message)
                : SendResult::failure(MQError(ErrorCode::Closed, "send",
                                              "producer has no backend"));
}

/// 兼容 Record 发送入口，失败时抛统一异常。
void Producer::send(const Record &record) {
  Message message;
  message.payload = record;
  SendResult result = send(message);
  if (!result.ok)
    throw MQException(result.error);
}

/// 创建共享同一 ProducerState 的异步 facade。
AsyncProducer Producer::async() const { return AsyncProducer(state_); }

/// 兼容 Record 异步入口，提交失败时抛统一异常。
void Producer::send_async(const Record &record,
                          const SendCallback &callback) const {
  Message message;
  message.payload = record;
  OperationResult result = async().send_async(message, callback);
  if (!result.ok)
    throw MQException(result.error);
}

/// 幂等关闭 ProducerState。
OperationResult Producer::close() {
  return state_ ? state_->close() : OperationResult::success();
}

/// 构造空异步 facade。
AsyncProducer::AsyncProducer() {}

/// 保存共享 ProducerState。
AsyncProducer::AsyncProducer(const std::shared_ptr<ProducerState> &state)
    : state_(state) {}

/// 转发异步发送；空 facade 返回 Closed。
OperationResult AsyncProducer::send_async(
    Message message, const DeliveryCallback &callback) const {
  return state_ ? state_->send_async(message, callback)
                : OperationResult::failure(
                      MQError(ErrorCode::Closed, "send_async",
                              "producer has no backend"));
}

/// 关闭共享 ProducerState。
OperationResult AsyncProducer::close() {
  return state_ ? state_->close() : OperationResult::success();
}

/// Consumer Bridge 的共享线程与确认状态。
///
/// worker 只由 start 创建，stop/close 负责 join；handler 在 worker 中串行执行。
/// 手动 receive 使用 receipts 将 MessagePtr 映射为 backend 私有 Receipt。
class ConsumerState : public std::enable_shared_from_this<ConsumerState> {
public:
  /// 保存 backend 和兼容 listener。
  explicit ConsumerState(const std::shared_ptr<IConsumerBackend> &backend,
                         const MessageListener &listener)
      : backend(backend), listener(listener), running(false), stopping(false),
        closed(false) {}

  /// 析构时尽力停止并关闭，不向外抛异常。
  ~ConsumerState() {
    try {
      close();
    } catch (...) {
    }
  }

  /// 非阻塞启动唯一 worker 线程。
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

  /// worker 主循环：短超时 receive、串行 handler、按结果执行 ack/nack。
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

  /// 幂等停止 worker，并等待当前 receive/handler 返回。
  /// worker 内部调用 stop 时 detach 自身，避免 self-join 死锁。
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

  /// 幂等停止 worker 后关闭 backend。
  OperationResult close() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (closed)
        return close_result;
      closed = true;
      stopping = true;
    }
    stop();
    OperationResult result;
    try {
      result = backend->close();
    } catch (...) {
      result =
          OperationResult::failure(exception_error("close", std::string()));
    }
    std::lock_guard<std::mutex> lock(mutex);
    close_result = result;
    return result;
  }

  /// 在 start 未运行时同步拉取消息，并登记 MessagePtr 到 Receipt 的映射。
  MessagePtr receive_manual(int timeout_millis) {
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
    ReceiveResult result = backend->receive(timeout_millis);
    if (!result.ok)
      throw MQException(result.error);
    MessagePtr message(new Message(result.value.message));
    {
      std::lock_guard<std::mutex> lock(mutex);
      receipts[message.get()] = result.value.receipt;
    }
    if (listener)
      listener(message);
    return message;
  }

  /// 取出并删除 MessagePtr 对应的 Receipt，防止重复确认。
  ReceiptPtr take_receipt(const MessagePtr &message) {
    if (!message)
      throw MQException(MQError(ErrorCode::InvalidMessage, "receipt",
                                "message must not be null"));
    std::lock_guard<std::mutex> lock(mutex);
    std::map<const Message *, ReceiptPtr>::iterator it =
        receipts.find(message.get());
    if (it == receipts.end())
      throw MQException(MQError(ErrorCode::InvalidMessage, "receipt",
                                "message does not belong to this consumer"));
    ReceiptPtr receipt = it->second;
    receipts.erase(it);
    return receipt;
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

/// 接管 unique_ptr backend 创建 ConsumerState。
Consumer::Consumer(std::unique_ptr<IConsumerBackend> backend)
    : state_(new ConsumerState(
          std::shared_ptr<IConsumerBackend>(std::move(backend)),
          MessageListener())) {
  if (!state_->backend)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Consumer",
                              "backend must not be null"));
}

/// 使用已有共享 backend 和兼容 listener 创建 Consumer。
Consumer::Consumer(const std::shared_ptr<IConsumerBackend> &backend,
                   const MessageListener &listener)
    : state_(new ConsumerState(backend, listener)) {
  if (!backend)
    throw MQException(MQError(ErrorCode::InvalidConfig, "Consumer",
                              "backend must not be null"));
}

/// 析构时尽力关闭，不允许异常越过析构边界。
Consumer::~Consumer() noexcept {
  if (state_) {
    try {
      state_->close();
    } catch (...) {
    }
  }
}

/// 移动 ConsumerState，源对象变为空 facade。
Consumer::Consumer(Consumer &&other) noexcept
    : state_(std::move(other.state_)) {}

/// 关闭当前状态后接管源对象状态。
Consumer &Consumer::operator=(Consumer &&other) noexcept {
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

/// 非阻塞启动消费线程；空 facade 返回 Closed。
OperationResult Consumer::start(const MessageHandler &handler) {
  return state_ ? state_->start(handler)
                : OperationResult::failure(MQError(ErrorCode::Closed, "start",
                                                   "consumer has no backend"));
}

/// 幂等停止消费线程。
OperationResult Consumer::stop() {
  return state_ ? state_->stop() : OperationResult::success();
}

/// 幂等关闭 ConsumerState。
OperationResult Consumer::close() {
  return state_ ? state_->close() : OperationResult::success();
}

/// 在线程安全锁下读取 running 状态。
bool Consumer::running() const {
  if (!state_)
    return false;
  std::lock_guard<std::mutex> lock(state_->mutex);
  return state_->running;
}

/// 转发兼容手动 receive。
MessagePtr Consumer::receive(int timeout_millis) {
  return state_->receive_manual(timeout_millis);
}

/// 消费并确认 MessagePtr 对应的 Receipt。
void Consumer::acknowledge(const MessagePtr &message) {
  OperationResult result =
      state_->backend->acknowledge(state_->take_receipt(message));
  if (!result.ok)
    throw MQException(result.error);
}

/// 消费并否认 MessagePtr 对应的 Receipt。
void Consumer::negative_acknowledge(const MessagePtr &message) {
  OperationResult result =
      state_->backend->negative_acknowledge(state_->take_receipt(message));
  if (!result.ok)
    throw MQException(result.error);
}

/// 转发取消订阅，失败时抛统一异常。
void Consumer::unsubscribe() {
  OperationResult result = state_->backend->unsubscribe();
  if (!result.ok)
    throw MQException(result.error);
}

/// 转发重新订阅，失败时抛统一异常。
void Consumer::resubscribe() {
  OperationResult result = state_->backend->resubscribe();
  if (!result.ok)
    throw MQException(result.error);
}

} // namespace mq
} // namespace mental1104
