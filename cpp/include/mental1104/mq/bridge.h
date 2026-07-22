#pragma once

#include "mental1104/mq/backend.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace mental1104 {
namespace mq {

class AsyncProducer;
class ProducerState;

/// 同步 Producer Bridge。
///
/// Producer 独占或共享一个 IProducerBackend，并可通过 async() 创建共享同一状态的
/// AsyncProducer。类型不可复制、可移动；析构函数 noexcept 并尽力幂等关闭。
class Producer {
public:
  /// 接管 backend 的唯一所有权。
  /// @param backend 非空后端；为空时抛 MQException(ErrorCode::InvalidConfig)。
  explicit Producer(std::unique_ptr<IProducerBackend> backend);
  /// 与已有 facade 共享 backend 所有权。
  explicit Producer(const std::shared_ptr<IProducerBackend> &backend);
  /// 尽力关闭共享状态，不向外抛异常。
  ~Producer() noexcept;
  Producer(const Producer &) = delete;
  Producer &operator=(const Producer &) = delete;
  /// 移动后源对象不再拥有 Bridge 状态。
  Producer(Producer &&other) noexcept;
  /// 赋值前关闭当前状态，再接管源对象状态。
  Producer &operator=(Producer &&other) noexcept;

  /// 同步发送公共 Message。
  /// @return 最终 SendResult；后端异常会转换为 MQError。
  SendResult send(const Message &message);
  /// 第一版 MQ PR 的兼容重载；失败时抛 MQException。
  void send(const Record &record);
  /// @return 共享同一 backend、连接和关闭状态的异步 facade。
  AsyncProducer async() const;
  /// 第一版 MQ PR 的兼容异步入口；同步拒绝时抛 MQException。
  void send_async(const Record &record,
                  const SendCallback &callback = SendCallback()) const;
  /// 幂等关闭 backend，并等待已接受发送按接口约定完成。
  OperationResult close();

private:
  std::shared_ptr<ProducerState> state_;
  friend class AsyncProducer;
};

/// Producer 的异步 facade，不单独创建 SDK client。
///
/// callback 在线程边界外执行，异常被隔离；关闭任一 facade 都会关闭共享状态。
class AsyncProducer {
public:
  /// 构造不包含 backend 的空 facade；发送会返回 Closed。
  AsyncProducer();
  /// @param state 与 Producer 共享的生命周期状态。
  explicit AsyncProducer(const std::shared_ptr<ProducerState> &state);
  /// 提交异步消息。
  /// @param message 按值传入，保证提交后调用方可安全复用原消息。
  /// @param callback 被接受请求的最终结果回调，最多调用一次。
  /// @return 是否成功接受请求；同步拒绝时不调用 callback。
  OperationResult send_async(Message message,
                             const DeliveryCallback &callback) const;
  /// 幂等关闭共享 Producer 状态。
  OperationResult close();

private:
  std::shared_ptr<ProducerState> state_;
};

class ConsumerState;

/// Consumer Bridge，统一管理消费线程、确认和生命周期。
///
/// start 非阻塞并创建一个 Bridge 自有线程；同一 Consumer 的 handler 串行执行。
/// stop 等待当前 receive/handler 并允许重启；close 隐含 stop 且关闭后不可重启。
class Consumer {
public:
  /// 接管 backend 的唯一所有权。
  explicit Consumer(std::unique_ptr<IConsumerBackend> backend);
  /// 共享 backend，并可设置兼容手动 receive listener。
  explicit Consumer(const std::shared_ptr<IConsumerBackend> &backend,
                    const MessageListener &listener = MessageListener());
  /// 尽力停止线程并关闭 backend，不向外抛异常。
  ~Consumer() noexcept;
  Consumer(const Consumer &) = delete;
  Consumer &operator=(const Consumer &) = delete;
  Consumer(Consumer &&other) noexcept;
  Consumer &operator=(Consumer &&other) noexcept;

  /// 非阻塞启动消费线程。
  /// @param handler 在 Bridge 线程中串行执行；失败或抛异常时统一 nack。
  /// @return 重复启动返回 AlreadyStarted，关闭后返回 Closed。
  OperationResult start(const MessageHandler &handler);
  /// 幂等停止并等待当前 receive/handler；成功后允许再次 start。
  OperationResult stop();
  /// 幂等 stop 后关闭 backend，并等待工作线程退出。
  OperationResult close();
  /// @return 当前是否有运行中的消费线程。
  bool running() const;

  /// 兼容手动拉取接口；start 活跃时不可调用。
  /// @param timeout_millis 等待毫秒数，负值使用后端默认值。
  /// @return 由当前 Consumer 管理确认凭据的 MessagePtr。
  /// @throws MQException 接收失败、已关闭或 start 活跃。
  MessagePtr receive(int timeout_millis = -1);
  /// 确认当前 Consumer 返回的 MessagePtr；其他消息抛 MQException。
  void acknowledge(const MessagePtr &message);
  /// 否认当前 Consumer 返回的 MessagePtr；其他消息抛 MQException。
  void negative_acknowledge(const MessagePtr &message);
  /// 取消订阅；失败时抛 MQException。
  void unsubscribe();
  /// 重新订阅；失败时抛 MQException。
  void resubscribe();

private:
  std::shared_ptr<ConsumerState> state_;
};

} // namespace mq
} // namespace mental1104
