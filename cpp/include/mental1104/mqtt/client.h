#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace mental1104 {
namespace mqtt {

/// MQTT QoS 等级。
///
/// 类型直接表达 MQTT 协议的三个合法等级，避免调用方通过裸整数传入无效值。
enum class QoS {
  AtMostOnce = 0,
  AtLeastOnce = 1,
  ExactlyOnce = 2,
};

/// MQTT 客户端操作错误类型。
enum class ErrorCode {
  None,
  InvalidArgument,
  Unavailable,
  NotConnected,
  Backend,
  Timeout,
};

/// MQTT 客户端操作结果。
///
/// Result 使用值语义返回稳定错误信息，不把 libmosquitto 的错误码泄漏到公共接口。
struct Result {
  bool ok;
  ErrorCode code;
  std::string message;

  /// 构造成功结果。
  ///
  /// @return code 为 ErrorCode::None 的成功结果。
  static Result success();

  /// 构造失败结果。
  ///
  /// @param code 稳定的公共错误类型，不能为 ErrorCode::None。
  /// @param message 面向调用方的失败原因。
  /// @return 包含错误类型和说明的失败结果。
  static Result failure(ErrorCode code, const std::string &message);

private:
  /// 仅允许通过 success/failure 创建结果，避免默认构造出未初始化状态。
  ///
  /// @param ok_value 操作是否成功。
  /// @param code_value 稳定错误类型；成功时必须为 ErrorCode::None。
  /// @param message_value 失败说明；成功时通常为空。
  Result(bool ok_value, ErrorCode code_value,
         const std::string &message_value);
};

/// MQTT 客户端连接参数。
struct ClientOptions {
  std::string host;
  int port;
  std::string client_id;
  int keep_alive_seconds;
  bool clean_session;
  std::chrono::milliseconds connect_timeout;

  /// 使用本地标准 MQTT 端口和保守连接超时构造默认配置。
  ///
  /// client_id 默认留空；调用 connect 前必须显式设置，避免多个测试进程
  /// 意外复用同一 client id 并互相踢下线。
  ClientOptions();
};

/// 一条已经从 broker 接收并复制到本地存储的 MQTT 消息。
struct Message {
  std::string topic;
  std::string payload;
  QoS qos;
  bool retained;

  /// 构造 QoS 0、非 retained 的空消息。
  Message();
};

/// MQTT 消息回调。
///
/// 回调由客户端网络线程触发；需要跨线程保存消息时可直接复制 Message。
typedef std::function<void(const Message &)> MessageHandler;

/// 查询当前构建是否包含可用的 libmosquitto 后端。
///
/// @return libmosquitto 已链接且全局初始化成功时返回 true。
bool available();

/// 面向业务和测试公共复用的 MQTT 原语客户端。
///
/// Client 只负责连接、发布、订阅、取消订阅和消息回调，不提供等待消息、
/// 请求应答、断言或故障注入等测试语义。底层 SDK 被隐藏在实现文件中。
///
/// Client 不可复制和移动，避免网络线程 callback 持有的 userdata 在对象迁移时
/// 出现生命周期歧义。析构会尽力断开连接并回收网络线程。
class Client {
public:
  /// 保存连接配置；构造过程不访问网络。
  ///
  /// @param options broker 地址、client id 和连接超时配置，按值复制保存。
  explicit Client(const ClientOptions &options);

  /// 尽力幂等断开连接，不向析构调用方抛出异常。
  ~Client() noexcept;

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;
  Client(Client &&) = delete;
  Client &operator=(Client &&) = delete;

  /// 连接 broker 并启动底层网络循环。
  ///
  /// @return 成功收到 CONNACK 时返回成功；配置非法、SDK 不可用、超时或
  ///         broker 拒绝连接时返回对应错误。
  Result connect();

  /// 幂等断开 broker 并等待网络线程退出。
  ///
  /// @return 已断开时仍返回成功；底层 disconnect 失败时返回 Backend。
  Result disconnect();

  /// 发布一条 MQTT 消息。
  ///
  /// @param topic 发布主题，不能为空。
  /// @param payload MQTT 二进制载荷；std::string 可包含空字节。
  /// @param qos MQTT QoS 等级。
  /// @param retained 是否设置 retained 标志。
  /// @return 请求被底层客户端接受时返回成功；不等待 QoS 1/2 最终确认。
  Result publish(const std::string &topic, const std::string &payload,
                 QoS qos = QoS::AtMostOnce, bool retained = false);

  /// 订阅一个 MQTT topic filter。
  ///
  /// @param topic_filter MQTT 主题或通配 filter，不能为空。
  /// @param qos 请求的最大 QoS。
  /// @return SUBSCRIBE 请求被底层客户端接受时返回成功；不等待 SUBACK。
  Result subscribe(const std::string &topic_filter,
                   QoS qos = QoS::AtMostOnce);

  /// 取消一个 MQTT topic filter 的订阅。
  ///
  /// @param topic_filter 之前订阅的主题或通配 filter，不能为空。
  /// @return UNSUBSCRIBE 请求被底层客户端接受时返回成功；不等待 UNSUBACK。
  Result unsubscribe(const std::string &topic_filter);

  /// 设置当前消息回调。
  ///
  /// @param handler 新回调；传入空 std::function 表示清除回调。
  /// @note 可以在连接期间替换回调。回调在 libmosquitto 网络线程执行，
  ///       实现会复制 handler 后再离开内部锁调用它。
  void set_message_handler(const MessageHandler &handler);

  /// 查询当前客户端是否已经完成 MQTT 连接。
  ///
  /// @return 已收到成功 CONNACK 且尚未断开时返回 true。
  bool connected() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace mqtt
} // namespace mental1104
