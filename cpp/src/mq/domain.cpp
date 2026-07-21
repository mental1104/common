#include "mental1104/mq/domain.h"

#include <sstream>

namespace mental1104 {
namespace mq {

MQError::MQError() : code(ErrorCode::None), retryable(false) {}
MQError::MQError(ErrorCode c, const std::string &op, const std::string &msg,
                 const std::string &be, bool retry)
    : code(c), operation(op), backend(be), message(msg), retryable(retry) {}
bool MQError::empty() const { return code == ErrorCode::None; }

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
const char *MQException::what() const noexcept { return text_.c_str(); }
const MQError &MQException::error() const { return error_; }

OperationResult OperationResult::success() {
  OperationResult r;
  r.ok = true;
  return r;
}
OperationResult OperationResult::failure(const MQError &error) {
  OperationResult r;
  r.ok = false;
  r.error = error;
  return r;
}

SendResult SendResult::success(const std::string &id) {
  SendResult r;
  r.ok = true;
  r.message_id = id;
  r.has_partition = false;
  r.partition = -1;
  return r;
}
SendResult SendResult::success(const std::string &id, std::int32_t p) {
  SendResult r = success(id);
  r.has_partition = true;
  r.partition = p;
  return r;
}
SendResult SendResult::failure(const MQError &error) {
  SendResult r;
  r.ok = false;
  r.has_partition = false;
  r.partition = -1;
  r.error = error;
  return r;
}
SendResult SendResult::failure(const std::string &error) {
  return failure(MQError(ErrorCode::Backend, "send", error));
}

Message::Message() : has_partition(false), partition(-1) {}

HandlerResult HandlerResult::acknowledge() {
  HandlerResult r;
  r.action = ConsumeAction::Acknowledge;
  return r;
}
HandlerResult HandlerResult::negative_acknowledge() {
  HandlerResult r;
  r.action = ConsumeAction::NegativeAcknowledge;
  return r;
}
HandlerResult HandlerResult::leave_unacked() {
  HandlerResult r;
  r.action = ConsumeAction::LeaveUnacked;
  return r;
}
HandlerResult HandlerResult::failure(const MQError &error) {
  HandlerResult r = negative_acknowledge();
  r.error = error;
  return r;
}

ProducerConfig::ProducerConfig() : batching_enabled(true) {}
ConsumerConfig::ConsumerConfig()
    : subscription_type(SubscriptionType::Shared) {}

Record make_record(const std::string &value) {
  return Record(value.begin(), value.end());
}
std::string record_to_string(const Record &record) {
  return std::string(record.begin(), record.end());
}

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
std::string build_pulsar_topic(const TopicAddress &t) {
  if (t.tenant.empty() || t.namespace_name.empty() || t.topic.empty())
    throw MQException(MQError(ErrorCode::InvalidConfig, "build_pulsar_topic",
                              "tenant, namespace and topic must not be empty",
                              "pulsar"));
  return "persistent://" + t.tenant + "/" + t.namespace_name + "/" + t.topic;
}
std::string build_kafka_topic(const std::string &tenant, const std::string &ns,
                              const std::string &topic) {
  TopicAddress t;
  t.tenant = tenant;
  t.namespace_name = ns;
  t.topic = topic;
  return build_kafka_topic(t);
}
std::string build_pulsar_topic(const std::string &tenant, const std::string &ns,
                               const std::string &topic) {
  TopicAddress t;
  t.tenant = tenant;
  t.namespace_name = ns;
  t.topic = topic;
  return build_pulsar_topic(t);
}

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
