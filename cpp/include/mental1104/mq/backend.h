#pragma once

#include "mental1104/mq/domain.h"

#include <memory>

namespace mental1104 {
namespace mq {

class Receipt {
public:
  virtual ~Receipt() {}
};
typedef std::shared_ptr<Receipt> ReceiptPtr;

struct BackendMessage {
  Message message;
  ReceiptPtr receipt;
};

struct ReceiveResult {
  bool ok;
  BackendMessage value;
  MQError error;
  static ReceiveResult success(const BackendMessage &value);
  static ReceiveResult failure(const MQError &error);
};

class IProducerBackend {
public:
  virtual ~IProducerBackend() {}
  virtual SendResult send(const Message &message) = 0;
  // success means the request was accepted; every accepted request must invoke
  // callback exactly once before close() returns.
  virtual OperationResult send_async(const Message &message,
                                     const DeliveryCallback &callback) = 0;
  virtual OperationResult close() = 0;
};

class IConsumerBackend {
public:
  virtual ~IConsumerBackend() {}
  virtual ReceiveResult receive(int timeout_millis) = 0;
  virtual OperationResult acknowledge(const ReceiptPtr &receipt) = 0;
  virtual OperationResult negative_acknowledge(const ReceiptPtr &receipt) = 0;
  virtual OperationResult unsubscribe() = 0;
  virtual OperationResult resubscribe() = 0;
  virtual OperationResult close() = 0;
};

} // namespace mq
} // namespace mental1104
