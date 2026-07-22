#include "mental1104/mq/domain.h"

#include <sstream>

namespace mental1104 {
namespace mq {

/// 构造无错误状态。
MQError::MQError() : code(ErrorCode::None), retryable(false) {}

/// 保存统一错误字段的值副本。
MQError::MQError(ErrorCode c, const std::string &op, const std::string &msg,
                 const std::string &be, bool retry)
    : code(c), operation(op), backend(be), message(msg), retryable(retry) {}

/// 判断当前错误是否为空。
bool MQError::empty() const { return code == ErrorCode::None; }

/// 把 MQError 格式化为 std::exception 文本并持久化。
MQException::MQException(const MQError &error) : error_(error) {
  std::ostringstream out;
  if (!error.operation.empty())
    out << error.operation << ": ";
  out << (error.message.empty() ? "message queue operation failed"
                                : error.message);
  if (!error.backend.empty())
    out << " [backend=" << error.backend << "]";
  text_ = out.str();
}

/// 返回异常对象持有的稳定 C 字符串。
const char *MQException::what() const noexcept { return text_.c_str(); }

/// 返回原始统一错误。
const MQError &MQException::error() const { return error_; }

/// 创建成功操作结果。
OperationResult OperationResult::success() {
  OperationResult r;
  r.ok = true;
  return r;
}

/// 创建失败操作结果。
OperationResult OperationResult::failure(const MQError &error) {
  OperationResult r;
  r.ok = false;
  r.error = error;
  return r;
}

/// 创建不含分区信息的成功发送结果。
SendResult SendResult::success(const std::string &id) {
  SendResult r;
  r.ok = true;
  r.message_id = id;
  r.has_partition = false;
  r.partition = -1;
  return r;
}

/// 创建含分区信息的成功发送结果。
SendResult SendResult::success(const std::string &id, std::int32_t p) {
  SendResult r = success(id);
  r.has_partition = true;
  r.partition = p;
  return r;
}

/// 创建带 MQError 的失败发送结果。
SendResult SendResult::failure(const MQError &error) {
  SendResult r;
  r.ok = false;
  r.has_partition = false;
  r.partition = -1;
  r.error = error;
  return r;
}

/// 把兼容错误文本转换为 Backend 失败结果。
SendResult SendResult::failure(const std::string &error) {
  return failure(MQError(ErrorCode::Backend, "send", error));
}

/// 初始化无分区消息。
Message::Message() : has_partition(false), partition(-1) {}

/// 创建 ack handler 结果。
HandlerResult HandlerResult::acknowledge() {
  HandlerResult r;
  r.action = ConsumeAction::Acknowledge;
  return r;
}

/// 创建 nack handler 结果。
HandlerResult HandlerResult::negative_acknowledge() {
  HandlerResult r;
  r.action = ConsumeAction::NegativeAcknowledge;
  return r;
}

/// 创建保留未确认状态的 handler 结果。
HandlerResult HandlerResult::leave_unacked() {
  HandlerResult r;
  r.action = ConsumeAction::LeaveUnacked;
  return r;
}

/// 创建失败 handler 结果；Bridge 会按 nack 处理。
HandlerResult HandlerResult::failure(const MQError &error) {
  HandlerResult r = negative_acknowledge();
  r.error = error;
  return r;
}

/// 默认启用 Producer 批处理。
ProducerConfig::ProducerConfig() : batching_enabled(true) {}

/// 默认使用 Shared 消费订阅。
ConsumerConfig::ConsumerConfig()
    : subscription_type(SubscriptionType::Shared) {}

/// 复制字符串字节为 Record。
Record make_record(const std::string &value) {
  return Record(value.begin(), value.end());
}

/// 复制 Record 字节为字符串。
std::string record_to_string(const Record &record) {
  return std::string(record.begin(), record.end());
}

/// 按 Kafka 规则拼接非空主题组成部分。
std::string build_kafka_topic(const TopicAddress &t) {
  std::string result;
  const std::string parts[] = {t.tenant, t.namespace_name, t.topic};
  for (std::size_t i = 0; i < 3; ++i) {
    if (parts[i].empty())
      continue;
    if (!result.empty())
      result += ".";
    result += parts[i];
  }
  if (result.empty())
    throw MQException(MQError(ErrorCode::InvalidConfig, "build_kafka_topic",
                              "topic must not be empty", "kafka"));
  return result;
}

/// 构造 Pulsar persistent 完整主题名。
std::string build_pulsar_topic(const TopicAddress &t) {
  if (t.tenant.empty() || t.namespace_name.empty() || t.topic.empty())
    throw MQException(MQError(ErrorCode::InvalidConfig, "build_pulsar_topic",
                              "tenant, namespace and topic must not be empty",
                              "pulsar"));
  return "persistent://" + t.tenant + "/" + t.namespace_name + "/" + t.topic;
}

/// 兼容字符串参数的 Kafka topic 构造入口。
std::string build_kafka_topic(const std::string &tenant, const std::string &ns,
                              const std::string &topic) {
  TopicAddress t;
  t.tenant = tenant;
  t.namespace_name = ns;
  t.topic = topic;
  return build_kafka_topic(t);
}

/// 兼容字符串参数的 Pulsar topic 构造入口。
std::string build_pulsar_topic(const std::string &tenant, const std::string &ns,
                               const std::string &topic) {
  TopicAddress t;
  t.tenant = tenant;
  t.namespace_name = ns;
  t.topic = topic;
  return build_pulsar_topic(t);
}

/// 把当前异常转换为统一 MQError；本函数必须在 catch 路径中调用。
MQError exception_error(const std::string &operation,
                        const std::string &backend) {
  try {
    throw;
  } catch (const MQException &e) {
    return e.error();
  } catch (const std::exception &e) {
    return MQError(ErrorCode::Backend, operation, e.what(), backend);
  } catch (...) {
    return MQError(ErrorCode::Unknown, operation, "unknown backend exception",
                   backend);
  }
}

} // namespace mq
} // namespace mental1104
