#pragma once

#include "mental1104/mqtt/client.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace mental1104 {
namespace testing {

/// 等待一条 MQTT 消息的测试结果。
///
/// status.ok 为 true 时 message 有效；失败时 message 保持默认值。
struct MqttWaitResult {
  mqtt::Result status;
  mqtt::Message message;

  /// 构造成功等待结果。
  ///
  /// @param message 已经复制到测试资产中的消息。
  /// @return status 为成功且持有消息副本的结果。
  static MqttWaitResult success(const mqtt::Message &message);

  /// 构造失败等待结果。
  ///
  /// @param status 必须是失败状态，通常为 InvalidArgument、Timeout 或 MQTT 原语错误。
  /// @return 不包含有效消息的失败结果。
  static MqttWaitResult failure(const mqtt::Result &status);
};

/// 可组合的 MQTT 对端测试资产。
///
/// MqttPeerFixture 不继承 ::testing::Test。它通过组合 mqtt::Client 提供两层能力：
/// 1. publish/subscribe/unsubscribe 等 MQTT 原语；
/// 2. wait_message/request 等测试专属等待与请求应答便利接口。
///
/// 收到的消息由 fixture 复制并缓存，wait_message 会消费第一条精确匹配 topic 的消息。
/// request 是顺序测试的便利封装，会按 subscribe(response) -> publish(request)
/// -> wait(response) 执行；并发请求或业务 TID 关联应由调用方使用原语自行表达。
class MqttPeerFixture {
public:
  /// 构造测试对端并注册内部消息采集回调；不建立网络连接。
  ///
  /// @param options MQTT broker 和 client 配置，复制给底层 mqtt::Client。
  explicit MqttPeerFixture(const mqtt::ClientOptions &options);

  /// 尽力断开客户端，不向测试清理阶段抛异常。
  ~MqttPeerFixture() noexcept;

  MqttPeerFixture(const MqttPeerFixture &) = delete;
  MqttPeerFixture &operator=(const MqttPeerFixture &) = delete;
  MqttPeerFixture(MqttPeerFixture &&) = delete;
  MqttPeerFixture &operator=(MqttPeerFixture &&) = delete;

  /// 连接 broker 并开始采集消息。
  ///
  /// @return 底层 mqtt::Client::connect 的操作结果。
  mqtt::Result start();

  /// 幂等断开 broker，并清除本地订阅记录。
  ///
  /// @return 底层 mqtt::Client::disconnect 的操作结果。
  mqtt::Result stop();

  /// 直接使用 MQTT publish 原语。
  ///
  /// @param topic 发布主题。
  /// @param payload 消息载荷。
  /// @param qos MQTT QoS。
  /// @param retained 是否设置 retained 标志。
  /// @return 底层 publish 结果。
  mqtt::Result publish(const std::string &topic, const std::string &payload,
                       mqtt::QoS qos = mqtt::QoS::AtMostOnce,
                       bool retained = false);

  /// 直接使用 MQTT subscribe 原语，并记录本 fixture 已订阅的 filter。
  ///
  /// @param topic_filter 订阅主题或通配 filter。
  /// @param qos 请求的最大 QoS。
  /// @return 底层 subscribe 结果。
  mqtt::Result subscribe(const std::string &topic_filter,
                         mqtt::QoS qos = mqtt::QoS::AtMostOnce);

  /// 直接使用 MQTT unsubscribe 原语，并移除本地订阅记录。
  ///
  /// @param topic_filter 要取消的主题或通配 filter。
  /// @return 底层 unsubscribe 结果。
  mqtt::Result unsubscribe(const std::string &topic_filter);

  /// 等待并消费第一条精确 topic 匹配的已接收消息。
  ///
  /// @param topic 要等待的实际消息 topic，不按 MQTT wildcard 规则解释。
  /// @param timeout 最长等待时间，必须大于 0。
  /// @return 收到消息时返回成功及消息副本；超时或参数非法时返回失败。
  MqttWaitResult wait_message(const std::string &topic,
                              std::chrono::milliseconds timeout);

  /// 使用 MQTT 原语执行一次顺序请求应答。
  ///
  /// @param request_topic 请求发布 topic，由当前测试显式传入。
  /// @param response_topic 响应订阅 topic，由当前测试显式传入。
  /// @param payload 请求载荷。
  /// @param timeout 最长响应等待时间，必须大于 0。
  /// @param qos 请求发布和响应订阅使用的 QoS。
  /// @return 第一条 response_topic 消息；订阅或发布失败时直接返回对应错误。
  ///
  /// @note 本方法不解析 payload，也不假设 tid/request_id 字段。需要同一 response
  ///       topic 并发关联多个请求时，请使用 publish/subscribe/wait_message 原语
  ///       在业务测试中显式匹配消息内容。
  MqttWaitResult request(const std::string &request_topic,
                         const std::string &response_topic,
                         const std::string &payload,
                         std::chrono::milliseconds timeout,
                         mqtt::QoS qos = mqtt::QoS::AtMostOnce);

  /// 获取当前尚未被 wait_message 消费的消息快照。
  ///
  /// @return 按接收顺序复制出的消息列表；调用方拥有返回值。
  std::vector<mqtt::Message> received_messages() const;

  /// 清空当前缓存的所有未消费消息。
  void clear_messages();

private:
  /// 把底层网络线程收到的消息复制到等待队列并唤醒测试线程。
  ///
  /// @param message 底层 callback 提供的只读消息，只在调用期间借用。
  void on_message(const mqtt::Message &message);

  mqtt::Client client_;
  mutable std::mutex messages_mutex_;
  std::condition_variable messages_cv_;
  std::deque<mqtt::Message> messages_;

  mutable std::mutex subscriptions_mutex_;
  std::set<std::string> subscriptions_;

  // request 是便利串行路径；加锁避免同一 fixture 的两个 request 相互消费响应。
  std::mutex request_mutex_;
};

} // namespace testing
} // namespace mental1104
