#pragma once

#include "mental1104/mq/domain.h"

#include <memory>

namespace mental1104 {
namespace mq {

/// 后端私有确认凭据的多态基类。
///
/// 具体 Kafka/Pulsar Receipt 只在对应 backend 源文件中定义；公共 Message 不保存 SDK 对象。
class Receipt {
public:
  /// 虚析构确保通过基类智能指针释放具体 SDK 凭据。
  virtual ~Receipt() {}
};
/// Receipt 的共享所有权类型；从 receive 成功持续到 ack/nack 或 backend 关闭。
typedef std::shared_ptr<Receipt> ReceiptPtr;

/// ConsumerBackend 返回给 Bridge 的消息和私有确认凭据。
struct BackendMessage {
  Message message;
  ReceiptPtr receipt;
};

/// 一次后端 receive 的结果。
struct ReceiveResult {
  bool ok;
  BackendMessage value;
  MQError error;
  /// @param value 接收消息及凭据的副本。
  /// @return 成功结果。
  static ReceiveResult success(const BackendMessage &value);
  /// @param error 统一错误。
  /// @return 失败结果。
  static ReceiveResult failure(const MQError &error);
};

/// Producer Bridge 依赖的后端实现接口。
///
/// 实现负责 SDK 连接和在途请求；close 必须幂等，并等待所有已接受的异步请求
/// 产生最终结果。接口对象通过 unique_ptr/shared_ptr 管理，不使用拥有所有权的裸指针。
class IProducerBackend {
public:
  virtual ~IProducerBackend() {}
  /// 同步发送消息。
  /// @param message 公共消息只读引用；实现不得保存调用方可变引用。
  /// @return broker 最终发送结果。
  virtual SendResult send(const Message &message) = 0;
  /// 提交异步发送。
  /// @param message 公共消息只读引用；实现需要跨线程使用时必须复制。
  /// @param callback 最终结果回调。请求被接受后必须 exactly once 调用。
  /// @return success 表示请求已接受；同步拒绝时 callback 不应调用。
  virtual OperationResult send_async(const Message &message,
                                     const DeliveryCallback &callback) = 0;
  /// 幂等关闭后端，拒绝新发送并收敛已接受请求。
  virtual OperationResult close() = 0;
};

/// Consumer Bridge 依赖的后端实现接口。
///
/// SDK Message 只通过 Receipt 在本接口内部流转；Bridge 的工作线程负责 start/stop，
/// 后端只提供单次 receive 与确认操作。
class IConsumerBackend {
public:
  virtual ~IConsumerBackend() {}
  /// 拉取一条消息。
  /// @param timeout_millis 等待毫秒数；负值表示使用后端默认阻塞语义。
  /// @return 消息和私有 Receipt，或统一错误。
  virtual ReceiveResult receive(int timeout_millis) = 0;
  /// 确认由当前后端产生的 Receipt。
  virtual OperationResult acknowledge(const ReceiptPtr &receipt) = 0;
  /// 否认由当前后端产生的 Receipt。
  virtual OperationResult negative_acknowledge(const ReceiptPtr &receipt) = 0;
  /// 取消当前订阅；具体 broker 元数据删除语义由后端决定。
  virtual OperationResult unsubscribe() = 0;
  /// 使用原配置重新建立订阅。
  virtual OperationResult resubscribe() = 0;
  /// 幂等关闭 SDK Consumer 和连接。
  virtual OperationResult close() = 0;
};

} // namespace mq
} // namespace mental1104
