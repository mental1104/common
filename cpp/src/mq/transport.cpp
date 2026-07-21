#include "mental1104/mq/transport.h"

#include <exception>
#include <stdexcept>

namespace mental1104 {
namespace mq {

Producer::Producer(const std::shared_ptr<ProducerTransport> &transport)
    : transport_(transport), closed_(false), pending_(0) {
  if (!transport_) {
    throw std::invalid_argument("producer transport must not be null");
  }
}

Producer::~Producer() {
  try {
    close();
  } catch (...) {
  }
}

void Producer::send(const Record &record) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      throw std::runtime_error("producer is closed");
    }
  }
  const SendResult result = transport_->send(record);
  if (!result.ok) {
    throw std::runtime_error(result.error.empty() ? "message send failed"
                                                  : result.error);
  }
}

void Producer::send_async(const Record &record, const SendCallback &callback) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      throw std::runtime_error("producer is closed");
    }
    ++pending_;
  }

  const SendCallback completion = [this, callback](const SendResult &result) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (pending_ > 0) {
        --pending_;
      }
    }
    pending_cv_.notify_all();
    if (callback) {
      try {
        callback(result);
      } catch (...) {
      }
    }
  };

  try {
    transport_->send_async(record, completion);
  } catch (...) {
    completion(SendResult::failure("asynchronous send submission failed"));
    throw;
  }
}

void Producer::close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (closed_) {
      return;
    }
    closed_ = true;
  }

  std::exception_ptr close_error;
  try {
    transport_->close();
  } catch (...) {
    close_error = std::current_exception();
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    pending_cv_.wait(lock, [this]() { return pending_ == 0; });
  }

  if (close_error) {
    std::rethrow_exception(close_error);
  }
}

Consumer::Consumer(const std::shared_ptr<ConsumerTransport> &transport,
                   const MessageListener &listener)
    : transport_(transport), listener_(listener), closed_(false) {
  if (!transport_) {
    throw std::invalid_argument("consumer transport must not be null");
  }
}

Consumer::~Consumer() {
  try {
    close();
  } catch (...) {
  }
}

void Consumer::ensure_open() const {
  if (closed_) {
    throw std::runtime_error("consumer is closed");
  }
}

MessagePtr Consumer::receive(int timeout_millis) {
  MessagePtr message;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ensure_open();
    message = transport_->receive(timeout_millis);
  }
  if (listener_ && message) {
    listener_(message);
  }
  return message;
}

void Consumer::acknowledge(const MessagePtr &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_open();
  transport_->acknowledge(message);
}

void Consumer::negative_acknowledge(const MessagePtr &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_open();
  transport_->negative_acknowledge(message);
}

void Consumer::unsubscribe() {
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_open();
  transport_->unsubscribe();
}

void Consumer::resubscribe() {
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_open();
  transport_->resubscribe();
}

void Consumer::close() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) {
    return;
  }
  closed_ = true;
  transport_->close();
}

} // namespace mq
} // namespace mental1104
