#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mental1104 {
namespace mq {

/// 消息载荷使用拥有独立存储的字节数组。
typedef std::vector<std::uint8_t> Payload;
/// 第一版 MQ API 使用的兼容记录类型。
typedef Payload Record;
/// 消息头采用字符串键值；调用方和 Bridge 各自持有副本。
typedef std::map<std::string, std::string> MessageHeaders;
/// 后端兼容配置项集合。
typedef std::map<std::string, std::string> Options;
/// 预留的 schema 描述；当前后端发送原始字节载荷。
typedef std::string Schema;

/// 调用方可稳定判断的消息队列错误类别。
enum class ErrorCode {
  None,
  Unknown,
  InvalidConfig,
  InvalidMessage,
  Closed,
  Closing,
  AlreadyStarted,
  Timeout,
  Canceled,
  Backend,
  Handler,
};

/// 跨后端统一错误，不暴露 Kafka/Pulsar SDK 异常类型。
struct MQError {
  ErrorCode code;
  std::string operation;
  std::string backend;
  std::string message;
  bool retryable;

  /// 构造无错误状态。
  MQError();
  /// 构造带上下文的统一错误。
  /// @param code 稳定错误类别。
  /// @param operation 失败操作名称。
  /// @param message 面向调用方的错误说明。
  /// @param backend 后端名称，可为空。
  /// @param retryable 调用方是否可安全重试。
  MQError(ErrorCode code, const std::string &operation,
          const std::string &message,
          const std::string &backend = std::string(), bool retryable = false);
  /// @return true 表示没有错误。
  bool empty() const;
};

/// 兼容旧抛异常 API 的统一异常包装器。
///
/// 新 Bridge 优先返回 OperationResult/SendResult；兼容入口使用本类型抛出 MQError。
class MQException : public std::exception {
public:
  /// @param error 要保存的统一错误副本。
  explicit MQException(const MQError &error);
  /// @return 构造期间持久化的错误文本，生命周期与异常对象一致。
  const char *what() const noexcept override;
  /// @return 保存的统一错误只读引用。
  const MQError &error() const;

private:
  MQError error_;
  std::string text_;
};

/// 不携带业务值的操作结果。
struct OperationResult {
  bool ok;
  MQError error;
  /// @return 成功结果。
  static OperationResult success();
  /// @param error 失败原因。
  /// @return 失败结果副本。
  static OperationResult failure(const MQError &error);
};

/// 一次发送的最终结果。
/// message_id 和 partition 只在后端能够确定时有效。
struct SendResult {
  bool ok;
  std::string message_id;
  bool has_partition;
  std::int32_t partition;
  MQError error;
  /// @param message_id 后端消息标识，可为空。
  /// @return 不含 partition 的成功结果。
  static SendResult success(const std::string &message_id = std::string());
  /// @param message_id 后端消息标识。
  /// @param partition 实际写入分区。
  /// @return 含 partition 的成功结果。
  static SendResult success(const std::string &message_id,
                            std::int32_t partition);
  /// @param error 统一错误。
  /// @return 失败结果。
  static SendResult failure(const MQError &error);
  /// @param error 兼容调用方传入的错误文本。
  /// @return ErrorCode::Backend 失败结果。
  static SendResult failure(const std::string &error);
};

/// 跨后端逻辑主题地址。
/// Kafka 将非空部分用点号拼接；Pulsar 要求三个部分都非空。
struct TopicAddress {
  std::string tenant;
  std::string namespace_name;
  std::string topic;
};

/// 公共消息快照，不包含任何 SDK 对象或确认句柄。
///
/// key、payload、headers 由值语义管理；发送前可设置 partition，发送完成或接收后
/// message_id/partition 由后端填充。确认凭据保存在私有 Receipt 中。
struct Message {
  std::string topic;
  Payload key;
  Payload payload;
  MessageHeaders headers;
  bool has_partition;
  std::int32_t partition;
  std::string message_id;

  /// 构造无分区消息。
  Message();
};

/// 兼容手动 receive API 的共享消息所有权。
typedef std::shared_ptr<Message> MessagePtr;
/// 异步发送完成回调。
///
/// 每个被接受的请求最多调用一次；回调参数只在本次调用期间有效，需跨线程保存时应复制。
typedef std::function<void(const SendResult &)> DeliveryCallback;
/// 第一版 MQ API 的兼容回调别名。
typedef DeliveryCallback SendCallback;

/// handler 对消息的确认决策。
enum class ConsumeAction { LeaveUnacked, Acknowledge, NegativeAcknowledge };

/// 消费 handler 的结果。
struct HandlerResult {
  ConsumeAction action;
  MQError error;
  /// @return 确认消息处理成功。
  static HandlerResult acknowledge();
  /// @return 否认消息并请求后端重投或重新分配。
  static HandlerResult negative_acknowledge();
  /// @return 不执行确认操作。
  static HandlerResult leave_unacked();
  /// @param error handler 失败原因；Bridge 会执行 negative acknowledge。
  /// @return 失败结果。
  static HandlerResult failure(const MQError &error);
};

/// 在 Consumer Bridge 自有工作线程中串行执行的消息处理函数。
typedef std::function<HandlerResult(const Message &)> MessageHandler;
/// 第一版手动 receive API 的兼容监听器。
typedef std::function<void(const MessagePtr &)> MessageListener;

/// 当前仓库支持的消息队列后端。
enum class BackendType { Kafka, Pulsar };
/// 跨后端订阅策略。
enum class SubscriptionType { Shared, Exclusive, Failover, KeyShared };

/// 后端专属配置的多态基类。
class BackendConfig {
public:
  /// 虚析构确保通过基类智能指针释放具体配置。
  virtual ~BackendConfig() {}
  /// @return 具体后端类型。
  virtual BackendType backend_type() const = 0;
};

/// Producer 的公共配置和类型安全后端配置。
struct ProducerConfig {
  TopicAddress topic;
  bool batching_enabled;
  std::shared_ptr<const BackendConfig> backend;
  /// 默认启用批处理。
  ProducerConfig();
};

/// Consumer 的公共配置和类型安全后端配置。
struct ConsumerConfig {
  TopicAddress topic;
  std::string subscription;
  SubscriptionType subscription_type;
  std::shared_ptr<const BackendConfig> backend;
  /// 默认使用 Shared 订阅。
  ConsumerConfig();
};

/// 把字符串复制为 Record 字节数组。
/// @param value 输入字符串。
/// @return 独立拥有存储的 Record。
Record make_record(const std::string &value);
/// 把 Record 字节转换为字符串副本。
/// @param record 输入记录。
/// @return 字符串副本。
std::string record_to_string(const Record &record);
/// 构造 Kafka topic；所有部分为空时抛 MQException。
std::string build_kafka_topic(const TopicAddress &topic);
/// 构造 persistent://tenant/namespace/topic；字段缺失时抛 MQException。
std::string build_pulsar_topic(const TopicAddress &topic);
/// Kafka topic 构造兼容重载。
std::string build_kafka_topic(const std::string &tenant,
                              const std::string &namespace_name,
                              const std::string &topic);
/// Pulsar topic 构造兼容重载。
std::string build_pulsar_topic(const std::string &tenant,
                               const std::string &namespace_name,
                               const std::string &topic);
/// 把当前捕获中的异常转换为 MQError；必须在 catch 路径中调用。
/// @param operation 失败操作名称。
/// @param backend 后端名称。
/// @return 统一错误。
MQError exception_error(const std::string &operation,
                        const std::string &backend);

} // namespace mq
} // namespace mental1104
