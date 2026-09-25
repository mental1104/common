#include "mental1104/testing/mqtt_peer_fixture.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

namespace {
using mental1104::mqtt::ClientOptions;
using mental1104::mqtt::ErrorCode;
using mental1104::mqtt::QoS;
using mental1104::testing::MqttPeerFixture;
using mental1104::testing::MqttWaitResult;

/// 读取非空环境变量。
///
/// @param name 环境变量名称，必须是空字符结尾字符串。
/// @return 环境变量存在且非空时返回其值，否则返回空字符串。
std::string env_value(const char *name) {
  const char *value = std::getenv(name);
  return value && value[0] != '\0' ? std::string(value) : std::string();
}

/// 生成当前进程内足够唯一的 MQTT client id 或 topic 后缀。
///
/// @param prefix 便于区分测试角色的前缀。
/// @return 前缀和高精度时钟计数组成的字符串。
std::string unique_name(const std::string &prefix) {
  std::ostringstream value;
  value << prefix << "-"
        << std::chrono::high_resolution_clock::now().time_since_epoch().count();
  return value.str();
}

/// 从测试环境构造 MQTT 连接参数。
///
/// @param client_prefix 当前 fixture 的 client id 前缀。
/// @return MQTT_HOST/MQTT_PORT 完整时返回可连接配置；否则 client_id 仍有效，
///         host/port 保持默认值。
ClientOptions mqtt_options(const std::string &client_prefix) {
  ClientOptions options;
  options.client_id = unique_name(client_prefix);

  const std::string host = env_value("MQTT_HOST");
  const std::string port = env_value("MQTT_PORT");
  if (!host.empty())
    options.host = host;
  if (!port.empty())
    options.port = std::atoi(port.c_str());
  return options;
}

/// 检查当前构建和环境是否允许运行真实 MQTT 集成测试。
///
/// 缺失 libmosquitto 或 MQTT_HOST/MQTT_PORT 时跳过，而不是把可选基础设施
/// 缺失误报为公共代码回归。
void require_mqtt_integration_environment() {
  if (!mental1104::mqtt::available())
    GTEST_SKIP() << "libmosquitto is unavailable in this build";
  if (env_value("MQTT_HOST").empty() || env_value("MQTT_PORT").empty())
    GTEST_SKIP() << "MQTT integration requires MQTT_HOST and MQTT_PORT";
}

/// 验证无 broker 时也能稳定检查原语参数，不把 SDK 错误覆盖到参数错误之上。
TEST(MqttClientDomain, RejectsEmptyPublishTopicBeforeConnection) {
  ClientOptions options;
  options.client_id = unique_name("mqtt-domain");
  mental1104::mqtt::Client client(options);

  const mental1104::mqtt::Result result =
      client.publish("", "payload", QoS::AtMostOnce, false);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(ErrorCode::InvalidArgument, result.code);
}

/// 验证 fixture 仍完整暴露 subscribe/publish/wait_message 原语。
TEST(MqttPeerFixtureIntegration, PrimitivePublishSubscribeRoundTrip) {
  require_mqtt_integration_environment();

  MqttPeerFixture peer(mqtt_options("mqtt-peer-primitive"));
  ASSERT_TRUE(peer.start().ok);

  const std::string topic = "common/test/" + unique_name("primitive");
  ASSERT_TRUE(peer.subscribe(topic, QoS::AtLeastOnce).ok);
  ASSERT_TRUE(peer.publish(topic, "hello", QoS::AtLeastOnce, false).ok);

  const MqttWaitResult received =
      peer.wait_message(topic, std::chrono::seconds(5));
  ASSERT_TRUE(received.status.ok) << received.status.message;
  EXPECT_EQ("hello", received.message.payload);
  EXPECT_EQ(topic, received.message.topic);
}

/// 验证 request 显式接收 request/response topic，并由原语组合完成闭环。
TEST(MqttPeerFixtureIntegration, RequestUsesExplicitTopicPair) {
  require_mqtt_integration_environment();

  MqttPeerFixture requester(mqtt_options("mqtt-requester"));
  MqttPeerFixture responder(mqtt_options("mqtt-responder"));
  ASSERT_TRUE(requester.start().ok);
  ASSERT_TRUE(responder.start().ok);

  const std::string base = "common/test/" + unique_name("rpc");
  const std::string request_topic = base + "/request";
  const std::string response_topic = base + "/response";
  ASSERT_TRUE(responder.subscribe(request_topic, QoS::AtLeastOnce).ok);

  MqttWaitResult request_received;
  mental1104::mqtt::Result response_sent =
      mental1104::mqtt::Result::failure(
          ErrorCode::Backend, "responder thread did not run");

  std::thread responder_thread([&]() {
    request_received =
        responder.wait_message(request_topic, std::chrono::seconds(5));
    if (request_received.status.ok) {
      response_sent = responder.publish(
          response_topic, "pong:" + request_received.message.payload,
          QoS::AtLeastOnce, false);
    }
  });

  const MqttWaitResult response =
      requester.request(request_topic, response_topic, "ping",
                        std::chrono::seconds(5), QoS::AtLeastOnce);
  responder_thread.join();

  ASSERT_TRUE(request_received.status.ok) << request_received.status.message;
  ASSERT_TRUE(response_sent.ok) << response_sent.message;
  ASSERT_TRUE(response.status.ok) << response.status.message;
  EXPECT_EQ("pong:ping", response.message.payload);
}

} // namespace
