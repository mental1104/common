#include "mental1104/mq/bridge.h"

#include <condition_variable>
#include <iostream>
#include <mutex>

using namespace mental1104::mq;

namespace {
class DemoReceipt : public Receipt {};
class DemoProducerBackend : public IProducerBackend {
public:
  DemoProducerBackend() : closed_(false), sequence_(0) {}
  SendResult send(const Message &) override {
    return SendResult::success("sync-" + next());
  }
  OperationResult send_async(const Message &,
                             const DeliveryCallback &callback) override {
    if (closed_)
      return OperationResult::failure(
          MQError(ErrorCode::Closed, "send_async", "closed"));
    callback(SendResult::success("async-" + next()));
    return OperationResult::success();
  }
  OperationResult close() override {
    closed_ = true;
    return OperationResult::success();
  }

private:
  std::string next() { return std::to_string(++sequence_); }
  bool closed_;
  int sequence_;
};
class DemoConsumerBackend : public IConsumerBackend {
public:
  DemoConsumerBackend() : sent_(false) {}
  ReceiveResult receive(int) override {
    if (!sent_) {
      sent_ = true;
      BackendMessage b;
      b.message.payload = make_record("consumed");
      b.receipt.reset(new DemoReceipt());
      return ReceiveResult::success(b);
    }
    return ReceiveResult::failure(
        MQError(ErrorCode::Timeout, "receive", "timeout"));
  }
  OperationResult acknowledge(const ReceiptPtr &) override {
    return OperationResult::success();
  }
  OperationResult negative_acknowledge(const ReceiptPtr &) override {
    return OperationResult::success();
  }
  OperationResult unsubscribe() override { return OperationResult::success(); }
  OperationResult resubscribe() override { return OperationResult::success(); }
  OperationResult close() override { return OperationResult::success(); }

private:
  bool sent_;
};
} // namespace

int main() {
  Producer producer(
      std::unique_ptr<IProducerBackend>(new DemoProducerBackend()));
  Message message;
  message.payload = make_record("hello");
  SendResult sync = producer.send(message);
  std::cout << "sync: " << sync.message_id << "\n";
  producer.async().send_async(message, [](const SendResult &result) {
    std::cout << "async callback: " << result.message_id << "\n";
  });
  producer.close();

  Consumer consumer(
      std::unique_ptr<IConsumerBackend>(new DemoConsumerBackend()));
  std::mutex mutex;
  std::condition_variable cv;
  bool consumed = false;
  consumer.start([&](const Message &value) {
    std::cout << "consumer: " << record_to_string(value.payload) << "\n";
    {
      std::lock_guard<std::mutex> lock(mutex);
      consumed = true;
    }
    cv.notify_all();
    return HandlerResult::acknowledge();
  });
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]() { return consumed; });
  }
  consumer.stop();
  consumer.close();
}
