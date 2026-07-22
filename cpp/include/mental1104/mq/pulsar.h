#pragma once

#include "mental1104/mq/abstract_message_queue.h"

namespace mental1104 {
namespace mq {

/// pulsar-client-cpp 专属配置。
/// service_url 必填；authentication_token 可为空；options 保存 SDK 附加配置。
struct PulsarBackendConfig : public BackendConfig {
  std::string service_url;
  std::string authentication_token;
  Options options;
  int close_timeout_millis;
  /// 默认关闭等待为后端实现规定值。
  PulsarBackendConfig();
  /// @return BackendType::Pulsar。
  BackendType backend_type() const override;
};

/// @return 当前构建是否检测并链接了 pulsar-client-cpp。
bool pulsar_available();
/// 创建 Pulsar Producer backend；调用方取得唯一所有权。
std::unique_ptr<IProducerBackend>
create_pulsar_producer_backend(const ProducerConfig &config,
                               const PulsarBackendConfig &backend);
/// 创建 Pulsar Consumer backend；调用方取得唯一所有权。
std::unique_ptr<IConsumerBackend>
create_pulsar_consumer_backend(const ConsumerConfig &config,
                               const PulsarBackendConfig &backend);

/// 第一版 API 的 Pulsar 兼容工厂 facade。
class PulsarMessageQueue : public AbstractMessageQueue {
public:
  /// @param config Pulsar 附加配置键值；service.url/token 可从其中解析。
  explicit PulsarMessageQueue(const Options &config = Options());
  /// 尽力幂等关闭，不向外抛异常。
  ~PulsarMessageQueue() noexcept;
  /// 创建 Pulsar Producer Bridge；schema 当前仅作为兼容扩展位。
  std::shared_ptr<Producer>
  create_producer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const Schema &schema = Schema(),
                  bool batching_enabled = true) override;
  /// 创建 Pulsar Consumer Bridge；subscription 映射为 Pulsar subscription name。
  std::shared_ptr<Consumer>
  create_consumer(const std::string &tenant, const std::string &namespace_name,
                  const std::string &topic, const std::string &subscription,
                  const Schema &schema = Schema(),
                  SubscriptionType subscription_type = SubscriptionType::Shared,
                  const MessageListener &message_listener = MessageListener(),
                  const Options &options = Options()) override;
  /// 幂等关闭 facade；已创建 Bridge 继续管理自己的 backend 生命周期。
  void close() override;

private:
  Options options_;
  bool closed_;
};

} // namespace mq
} // namespace mental1104
