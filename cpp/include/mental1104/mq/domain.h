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

typedef std::vector<std::uint8_t> Payload;
typedef Payload Record;
typedef std::map<std::string, std::string> MessageHeaders;
typedef std::map<std::string, std::string> Options;
typedef std::string Schema;

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

struct MQError {
  ErrorCode code;
  std::string operation;
  std::string backend;
  std::string message;
  bool retryable;

  MQError();
  MQError(ErrorCode code, const std::string &operation,
          const std::string &message,
          const std::string &backend = std::string(), bool retryable = false);
  bool empty() const;
};

class MQException : public std::exception {
public:
  explicit MQException(const MQError &error);
  const char *what() const noexcept override;
  const MQError &error() const;

private:
  MQError error_;
  std::string text_;
};

struct OperationResult {
  bool ok;
  MQError error;
  static OperationResult success();
  static OperationResult failure(const MQError &error);
};

struct SendResult {
  bool ok;
  std::string message_id;
  bool has_partition;
  std::int32_t partition;
  MQError error;
  static SendResult success(const std::string &message_id = std::string());
  static SendResult success(const std::string &message_id,
                            std::int32_t partition);
  static SendResult failure(const MQError &error);
  static SendResult failure(const std::string &error);
};

struct TopicAddress {
  std::string tenant;
  std::string namespace_name;
  std::string topic;
};

struct Message {
  std::string topic;
  Payload key;
  Payload payload;
  MessageHeaders headers;
  bool has_partition;
  std::int32_t partition;
  std::string message_id;

  Message();
};

typedef std::shared_ptr<Message> MessagePtr;
typedef std::function<void(const SendResult &)> DeliveryCallback;
typedef DeliveryCallback SendCallback;

enum class ConsumeAction { LeaveUnacked, Acknowledge, NegativeAcknowledge };

struct HandlerResult {
  ConsumeAction action;
  MQError error;
  static HandlerResult acknowledge();
  static HandlerResult negative_acknowledge();
  static HandlerResult leave_unacked();
  static HandlerResult failure(const MQError &error);
};

typedef std::function<HandlerResult(const Message &)> MessageHandler;
typedef std::function<void(const MessagePtr &)> MessageListener;

enum class BackendType { Kafka, Pulsar };
enum class SubscriptionType { Shared, Exclusive, Failover, KeyShared };

class BackendConfig {
public:
  virtual ~BackendConfig() {}
  virtual BackendType backend_type() const = 0;
};

struct ProducerConfig {
  TopicAddress topic;
  bool batching_enabled;
  std::shared_ptr<const BackendConfig> backend;
  ProducerConfig();
};

struct ConsumerConfig {
  TopicAddress topic;
  std::string subscription;
  SubscriptionType subscription_type;
  std::shared_ptr<const BackendConfig> backend;
  ConsumerConfig();
};

Record make_record(const std::string &value);
std::string record_to_string(const Record &record);
std::string build_kafka_topic(const TopicAddress &topic);
std::string build_pulsar_topic(const TopicAddress &topic);
std::string build_kafka_topic(const std::string &tenant,
                              const std::string &namespace_name,
                              const std::string &topic);
std::string build_pulsar_topic(const std::string &tenant,
                               const std::string &namespace_name,
                               const std::string &topic);
MQError exception_error(const std::string &operation,
                        const std::string &backend);

} // namespace mq
} // namespace mental1104
