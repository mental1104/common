#include "mental1104/mq/abstract_message_queue.h"

#include <stdexcept>

namespace mental1104 {
namespace mq {

SendResult SendResult::success(const std::string &message_id) {
  SendResult result;
  result.ok = true;
  result.message_id = message_id;
  return result;
}

SendResult SendResult::failure(const std::string &error) {
  SendResult result;
  result.ok = false;
  result.error = error;
  return result;
}

Record make_record(const std::string &value) {
  return Record(value.begin(), value.end());
}

std::string record_to_string(const Record &record) {
  return std::string(record.begin(), record.end());
}

std::string build_kafka_topic(const std::string &tenant,
                              const std::string &namespace_name,
                              const std::string &topic) {
  std::string result;
  const std::string parts[] = {tenant, namespace_name, topic};
  for (std::size_t i = 0; i < 3; ++i) {
    if (parts[i].empty()) {
      continue;
    }
    if (!result.empty()) {
      result += ".";
    }
    result += parts[i];
  }
  if (result.empty()) {
    throw std::invalid_argument("kafka topic must not be empty");
  }
  return result;
}

std::string build_pulsar_topic(const std::string &tenant,
                               const std::string &namespace_name,
                               const std::string &topic) {
  if (tenant.empty() || namespace_name.empty() || topic.empty()) {
    throw std::invalid_argument(
        "pulsar tenant, namespace and topic must not be empty");
  }
  return "persistent://" + tenant + "/" + namespace_name + "/" + topic;
}

} // namespace mq
} // namespace mental1104
