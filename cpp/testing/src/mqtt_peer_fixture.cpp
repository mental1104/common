#include "mental1104/testing/mqtt_peer_fixture.h"

#include <algorithm>

namespace mental1104 {
namespace testing {

/// 初始化一份完整等待结果。
MqttWaitResult::MqttWaitResult(const mqtt::Result &status_value,
                               const mqtt::Message &message_value)
    : status(status_value), message(message_value) {}

/// 创建成功消息等待结果。
MqttWaitResult MqttWaitResult::success(const mqtt::Message &message) {
  return MqttWaitResult(mqtt::Result::success(), message);
}

/// 创建失败消息等待结果。
MqttWaitResult MqttWaitResult::failure(const mqtt::Result &status) {
  return MqttWaitResult(status, mqtt::Message());
}

/// 注册内部 callback，把网络线程消息转换为可等待的测试队列。
MqttPeerFixture::MqttPeerFixture(const mqtt::ClientOptions &options)
    : client_(options) {
  this->client_.set_message_handler(
      [this](const mqtt::Message &message) { this->on_message(message); });
}

/// 先停止网络线程，再清除 callback，确保 callback 不会访问已经析构的 fixture。
MqttPeerFixture::~MqttPeerFixture() noexcept {
  try {
    this->stop();
  } catch (...) {
  }
  this->client_.set_message_handler(mqtt::MessageHandler());
}

/// 连接底层 MQTT client。
mqtt::Result MqttPeerFixture::start() { return this->client_.connect(); }

/// 断开底层 client，并重置 fixture 对 broker 订阅状态的本地认知。
mqtt::Result MqttPeerFixture::stop() {
  const mqtt::Result result = this->client_.disconnect();
  {
    std::lock_guard<std::mutex> lock(this->subscriptions_mutex_);
    this->subscriptions_.clear();
  }
  return result;
}

/// 转发 MQTT publish 原语。
mqtt::Result MqttPeerFixture::publish(const std::string &topic,
                                      const std::string &payload,
                                      mqtt::QoS qos, bool retained) {
  return this->client_.publish(topic, payload, qos, retained);
}

/// 转发 MQTT subscribe 原语，并仅在 SDK 接受请求后更新本地订阅集合。
mqtt::Result MqttPeerFixture::subscribe(const std::string &topic_filter,
                                        mqtt::QoS qos) {
  const mqtt::Result result = this->client_.subscribe(topic_filter, qos);
  if (result.ok) {
    std::lock_guard<std::mutex> lock(this->subscriptions_mutex_);
    this->subscriptions_.insert(topic_filter);
  }
  return result;
}

/// 转发 MQTT unsubscribe 原语，并仅在 SDK 接受请求后更新本地订阅集合。
mqtt::Result MqttPeerFixture::unsubscribe(const std::string &topic_filter) {
  const mqtt::Result result = this->client_.unsubscribe(topic_filter);
  if (result.ok) {
    std::lock_guard<std::mutex> lock(this->subscriptions_mutex_);
    this->subscriptions_.erase(topic_filter);
  }
  return result;
}

/// 从缓存中等待并消费第一条精确 topic 匹配的消息。
MqttWaitResult
MqttPeerFixture::wait_message(const std::string &topic,
                              std::chrono::milliseconds timeout) {
  if (topic.empty()) {
    return MqttWaitResult::failure(mqtt::Result::failure(
        mqtt::ErrorCode::InvalidArgument,
        "wait_message topic must not be empty"));
  }
  if (timeout.count() <= 0) {
    return MqttWaitResult::failure(mqtt::Result::failure(
        mqtt::ErrorCode::InvalidArgument,
        "wait_message timeout must be positive"));
  }

  std::unique_lock<std::mutex> lock(this->messages_mutex_);
  const bool ready = this->messages_cv_.wait_for(lock, timeout, [this, &topic]() {
    return std::find_if(
               this->messages_.begin(), this->messages_.end(),
               [&topic](const mqtt::Message &message) {
                 return message.topic == topic;
               }) != this->messages_.end();
  });
  if (!ready) {
    return MqttWaitResult::failure(mqtt::Result::failure(
        mqtt::ErrorCode::Timeout,
        "timed out waiting for MQTT message on topic: " + topic));
  }

  std::deque<mqtt::Message>::iterator found = std::find_if(
      this->messages_.begin(), this->messages_.end(),
      [&topic](const mqtt::Message &message) { return message.topic == topic; });
  mqtt::Message message = *found;
  this->messages_.erase(found);
  return MqttWaitResult::success(message);
}

/// 通过 subscribe -> publish -> wait_message 组合原语执行顺序请求应答。
MqttWaitResult
MqttPeerFixture::request(const std::string &request_topic,
                         const std::string &response_topic,
                         const std::string &payload,
                         std::chrono::milliseconds timeout, mqtt::QoS qos) {
  std::lock_guard<std::mutex> request_lock(this->request_mutex_);

  if (request_topic.empty() || response_topic.empty()) {
    return MqttWaitResult::failure(mqtt::Result::failure(
        mqtt::ErrorCode::InvalidArgument,
        "request and response topics must not be empty"));
  }
  if (timeout.count() <= 0) {
    return MqttWaitResult::failure(mqtt::Result::failure(
        mqtt::ErrorCode::InvalidArgument,
        "request timeout must be positive"));
  }

  bool already_subscribed = false;
  {
    std::lock_guard<std::mutex> lock(this->subscriptions_mutex_);
    already_subscribed =
        this->subscriptions_.find(response_topic) != this->subscriptions_.end();
  }
  if (!already_subscribed) {
    const mqtt::Result subscribed = this->subscribe(response_topic, qos);
    if (!subscribed.ok)
      return MqttWaitResult::failure(subscribed);
  }

  // 顺序 request 只消费本轮响应。历史同 topic 消息先移除，避免旧响应污染当前用例。
  {
    std::lock_guard<std::mutex> lock(this->messages_mutex_);
    this->messages_.erase(
        std::remove_if(this->messages_.begin(), this->messages_.end(),
                       [&response_topic](const mqtt::Message &message) {
                         return message.topic == response_topic;
                       }),
        this->messages_.end());
  }

  const mqtt::Result published =
      this->publish(request_topic, payload, qos, false);
  if (!published.ok)
    return MqttWaitResult::failure(published);

  return this->wait_message(response_topic, timeout);
}

/// 复制未消费消息快照，避免把内部容器引用暴露给测试调用方。
std::vector<mqtt::Message> MqttPeerFixture::received_messages() const {
  std::lock_guard<std::mutex> lock(this->messages_mutex_);
  return std::vector<mqtt::Message>(this->messages_.begin(),
                                    this->messages_.end());
}

/// 清空未消费消息队列。
void MqttPeerFixture::clear_messages() {
  std::lock_guard<std::mutex> lock(this->messages_mutex_);
  this->messages_.clear();
}

/// 网络线程 callback 只做最小复制和通知，不执行测试断言或业务解析。
void MqttPeerFixture::on_message(const mqtt::Message &message) {
  {
    std::lock_guard<std::mutex> lock(this->messages_mutex_);
    this->messages_.push_back(message);
  }
  this->messages_cv_.notify_all();
}

} // namespace testing
} // namespace mental1104
