#include "mental1104/mq/bridge.h"

#include <condition_variable>
#include <iostream>
#include <mutex>

using namespace mental1104::mq;

namespace {

/// 演示 Consumer 确认凭据；示例不依赖真实 broker。
class DemoReceipt : public Receipt {};

/// 只展示公共 Producer/AsyncProducer Bridge 调用形态的内存 backend。
class DemoProducerBackend : public IProducerBackend {
public:
  DemoProducerBackend() : closed_(false), sequence_(0) {}

  /// 返回递增的同步消息标识。
  SendResult send(const Message &) override {
    return SendResult::success("sync-" + next());
  }

  /// 同步触发演示 callback；真实 backend 可在 SDK callback 线程完成。
  OperationResult send_async(const Message &,
                             const DeliveryCallback &callback) override {
    if (closed_)
      return OperationResult::failure(
          MQError(ErrorCode::Closed, "send_async", "closed"));
    callback(SendResult::success("async-" + next()));
    return OperationResult::success();
  }

  /// 幂等标记关闭。
  OperationResult close() override {
    closed_ = true;
    return OperationResult::success();
  }

private:
  /// 生成本示例独占的递增标识。
  std::string next() { return std::to_string(++sequence_); }

  bool closed_;
  int sequence_;
};

/// 只返回一条消息，用于展示 Consumer start/handler/stop。
class DemoConsumerBackend : public IConsumerBackend {
public:
  DemoConsumerBackend() : sent_(false) {}

  /// 第一次返回消息，之后返回 Timeout 让 Bridge 继续轮询。
  ReceiveResult receive(int) override {
    if (!sent_) {
      sent_ = true;
      BackendMessage backend_message;
      backend_message.message.payload = make_record("consumed");
      backend_message.receipt.reset(new DemoReceipt());
      return ReceiveResult::success(backend_message);
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

/// 运行同步发送、异步 callback 和 Consumer handler 三个最小 Bridge 示例。
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
      // consumed 由 Consumer worker 写、main 线程读，必须在同一 mutex 下访问。
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
