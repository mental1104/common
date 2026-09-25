#include "mental1104/mqtt/client.h"

#include "mental1104/log.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <sstream>

#ifdef M1104_HAS_MOSQUITTO
#include <mosquitto.h>
#endif

namespace mental1104 {
namespace mqtt {

/// 初始化一个完整 MQTT 操作结果。
Result::Result(bool ok_value, ErrorCode code_value,
               const std::string &message_value)
    : ok(ok_value), code(code_value), message(message_value) {}

/// 创建公共成功结果。
Result Result::success() {
  return Result(true, ErrorCode::None, std::string());
}

/// 创建公共失败结果。
Result Result::failure(ErrorCode code, const std::string &message) {
  return Result(false, code, message);
}

/// 初始化 MQTT 默认连接参数。
ClientOptions::ClientOptions()
    : host("127.0.0.1"), port(1883), keep_alive_seconds(60),
      clean_session(true), connect_timeout(std::chrono::seconds(5)) {}

/// 初始化空 MQTT 消息。
Message::Message() : qos(QoS::AtMostOnce), retained(false) {}

#ifdef M1104_HAS_MOSQUITTO
namespace {

/// 管理 libmosquitto 进程级初始化与清理。
///
/// libmosquitto 要求库级 init/cleanup 包住所有 client 生命周期；函数静态对象保证
/// 第一次实际使用时初始化，并在进程退出阶段最后清理。
class MosquittoLibrary {
public:
  /// 初始化 libmosquitto，并保存稳定初始化结果。
  MosquittoLibrary() : init_code_(mosquitto_lib_init()) {}

  /// 仅在初始化成功时执行全局清理。
  ~MosquittoLibrary() {
    if (this->init_code_ == MOSQ_ERR_SUCCESS)
      mosquitto_lib_cleanup();
  }

  /// 查询全局初始化是否成功。
  ///
  /// @return libmosquitto 可创建 client 时返回 true。
  bool ready() const { return this->init_code_ == MOSQ_ERR_SUCCESS; }

  /// 返回初始化失败说明。
  ///
  /// @return 初始化成功时返回空字符串，否则返回 SDK 错误文本。
  std::string error() const {
    if (this->ready())
      return std::string();
    const char *message = mosquitto_strerror(this->init_code_);
    return message ? std::string(message) : std::string("unknown error");
  }

private:
  int init_code_;
};

/// 返回进程唯一的 libmosquitto 生命周期守卫。
///
/// @return 与进程同生命周期的初始化守卫引用。
MosquittoLibrary &mosquitto_library() {
  static MosquittoLibrary library;
  return library;
}

/// 把 libmosquitto 返回码转换为公共 Backend 错误。
///
/// @param operation 当前失败的 MQTT 操作名称。
/// @param code libmosquitto 返回码。
/// @return 包含稳定错误类型和 SDK 文本的失败结果。
Result mosquitto_failure(const std::string &operation, int code) {
  const char *reason = mosquitto_strerror(code);
  std::ostringstream message;
  message << operation << ": "
          << (reason ? reason : "unknown libmosquitto error");
  return Result::failure(ErrorCode::Backend, message.str());
}

} // namespace
#endif

/// Client 的 SDK 隔离实现。
class Client::Impl {
public:
  /// 保存配置并初始化线程状态；不创建网络连接。
  ///
  /// @param options 调用方传入的连接配置，复制到实现内部。
  explicit Impl(const ClientOptions &options)
      : options_(options), connected_(false)
#ifdef M1104_HAS_MOSQUITTO
        ,
        client_(NULL), loop_started_(false), connect_completed_(false),
        connect_code_(MOSQ_ERR_SUCCESS)
#endif
  {
  }

  /// 确保网络线程和 SDK client 在实现析构前已经释放。
  ~Impl() noexcept {
    try {
      this->disconnect();
    } catch (...) {
    }
#ifdef M1104_HAS_MOSQUITTO
    if (this->client_)
      mosquitto_destroy(this->client_);
#endif
  }

  /// 校验配置并建立 MQTT 连接。
  ///
  /// @return 完成 CONNACK 握手后返回成功，否则返回稳定错误。
  Result connect() {
    const Result validation = this->validate_options();
    if (!validation.ok)
      return validation;

#ifndef M1104_HAS_MOSQUITTO
    return Result::failure(ErrorCode::Unavailable,
                           "libmosquitto is unavailable in this build");
#else
    if (!mosquitto_library().ready()) {
      return Result::failure(
          ErrorCode::Unavailable,
          "libmosquitto initialization failed: " + mosquitto_library().error());
    }

    std::lock_guard<std::mutex> lifecycle_lock(this->lifecycle_mutex_);
    if (this->connected_.load())
      return Result::success();

    if (!this->client_) {
      this->client_ =
          mosquitto_new(this->options_.client_id.c_str(),
                        this->options_.clean_session, this);
      if (!this->client_)
        return Result::failure(ErrorCode::Backend,
                               "mosquitto_new failed to create client");

      mosquitto_connect_callback_set(this->client_, &Impl::on_connect);
      mosquitto_disconnect_callback_set(this->client_, &Impl::on_disconnect);
      mosquitto_message_callback_set(this->client_, &Impl::on_message);
    }

    {
      std::lock_guard<std::mutex> state_lock(this->connect_mutex_);
      this->connect_completed_ = false;
      this->connect_code_ = MOSQ_ERR_SUCCESS;
    }

    int code = mosquitto_connect(this->client_, this->options_.host.c_str(),
                                 this->options_.port,
                                 this->options_.keep_alive_seconds);
    if (code != MOSQ_ERR_SUCCESS)
      return mosquitto_failure("connect", code);

    code = mosquitto_loop_start(this->client_);
    if (code != MOSQ_ERR_SUCCESS) {
      mosquitto_disconnect(this->client_);
      return mosquitto_failure("loop_start", code);
    }
    this->loop_started_ = true;

    std::unique_lock<std::mutex> state_lock(this->connect_mutex_);
    const bool completed = this->connect_cv_.wait_for(
        state_lock, this->options_.connect_timeout,
        [this]() { return this->connect_completed_; });
    if (!completed) {
      state_lock.unlock();
      this->stop_loop();
      return Result::failure(ErrorCode::Timeout,
                             "timed out waiting for MQTT CONNACK");
    }

    const int connect_code = this->connect_code_;
    state_lock.unlock();
    if (connect_code != 0) {
      this->stop_loop();
      std::ostringstream message;
      message << "broker rejected MQTT connection, CONNACK=" << connect_code;
      return Result::failure(ErrorCode::Backend, message.str());
    }

    return Result::success();
#endif
  }

  /// 幂等断开 MQTT 连接并停止网络线程。
  ///
  /// @return 已经停止时返回成功；底层断开失败时返回 Backend。
  Result disconnect() {
#ifndef M1104_HAS_MOSQUITTO
    this->connected_.store(false);
    return Result::success();
#else
    std::lock_guard<std::mutex> lifecycle_lock(this->lifecycle_mutex_);
    if (!this->client_ || !this->loop_started_) {
      this->connected_.store(false);
      return Result::success();
    }

    const int disconnect_code = mosquitto_disconnect(this->client_);
    const int loop_code = mosquitto_loop_stop(this->client_, true);
    this->loop_started_ = false;
    this->connected_.store(false);

    if (disconnect_code != MOSQ_ERR_SUCCESS &&
        disconnect_code != MOSQ_ERR_NO_CONN) {
      return mosquitto_failure("disconnect", disconnect_code);
    }
    if (loop_code != MOSQ_ERR_SUCCESS)
      return mosquitto_failure("loop_stop", loop_code);
    return Result::success();
#endif
  }

  /// 发布 MQTT 消息，不等待 QoS 完成确认。
  ///
  /// @param topic 非空发布主题。
  /// @param payload 可包含空字节的消息载荷。
  /// @param qos MQTT QoS。
  /// @param retained 是否设置 retained 标志。
  /// @return SDK 接受 publish 请求时返回成功。
  Result publish(const std::string &topic, const std::string &payload, QoS qos,
                 bool retained) {
    if (topic.empty())
      return Result::failure(ErrorCode::InvalidArgument,
                             "publish topic must not be empty");
    if (payload.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      return Result::failure(ErrorCode::InvalidArgument,
                             "publish payload exceeds libmosquitto size limit");
    }
    if (!this->connected_.load())
      return Result::failure(ErrorCode::NotConnected,
                             "MQTT client is not connected");

#ifndef M1104_HAS_MOSQUITTO
    (void)payload;
    (void)qos;
    (void)retained;
    return Result::failure(ErrorCode::Unavailable,
                           "libmosquitto is unavailable in this build");
#else
    const int code = mosquitto_publish(
        this->client_, NULL, topic.c_str(), static_cast<int>(payload.size()),
        payload.empty() ? NULL : payload.data(), static_cast<int>(qos),
        retained);
    return code == MOSQ_ERR_SUCCESS ? Result::success()
                                    : mosquitto_failure("publish", code);
#endif
  }

  /// 提交 MQTT SUBSCRIBE。
  ///
  /// @param topic_filter 非空主题或通配 filter。
  /// @param qos 请求的最大 QoS。
  /// @return SDK 接受订阅请求时返回成功。
  Result subscribe(const std::string &topic_filter, QoS qos) {
    if (topic_filter.empty())
      return Result::failure(ErrorCode::InvalidArgument,
                             "subscribe topic filter must not be empty");
    if (!this->connected_.load())
      return Result::failure(ErrorCode::NotConnected,
                             "MQTT client is not connected");

#ifndef M1104_HAS_MOSQUITTO
    (void)qos;
    return Result::failure(ErrorCode::Unavailable,
                           "libmosquitto is unavailable in this build");
#else
    const int code = mosquitto_subscribe(this->client_, NULL,
                                         topic_filter.c_str(),
                                         static_cast<int>(qos));
    return code == MOSQ_ERR_SUCCESS ? Result::success()
                                    : mosquitto_failure("subscribe", code);
#endif
  }

  /// 提交 MQTT UNSUBSCRIBE。
  ///
  /// @param topic_filter 非空主题或通配 filter。
  /// @return SDK 接受取消订阅请求时返回成功。
  Result unsubscribe(const std::string &topic_filter) {
    if (topic_filter.empty())
      return Result::failure(ErrorCode::InvalidArgument,
                             "unsubscribe topic filter must not be empty");
    if (!this->connected_.load())
      return Result::failure(ErrorCode::NotConnected,
                             "MQTT client is not connected");

#ifndef M1104_HAS_MOSQUITTO
    return Result::failure(ErrorCode::Unavailable,
                           "libmosquitto is unavailable in this build");
#else
    const int code =
        mosquitto_unsubscribe(this->client_, NULL, topic_filter.c_str());
    return code == MOSQ_ERR_SUCCESS ? Result::success()
                                    : mosquitto_failure("unsubscribe", code);
#endif
  }

  /// 原子替换消息回调。
  ///
  /// @param handler 新回调；空回调表示停止向调用方分发消息。
  void set_message_handler(const MessageHandler &handler) {
    std::lock_guard<std::mutex> lock(this->handler_mutex_);
    this->handler_ = handler;
  }

  /// 查询握手后的连接状态。
  ///
  /// @return 当前已连接时返回 true。
  bool connected() const { return this->connected_.load(); }

private:
  /// 校验不依赖 SDK 的连接参数。
  ///
  /// @return 配置可用于连接时返回成功。
  Result validate_options() const {
    if (this->options_.host.empty())
      return Result::failure(ErrorCode::InvalidArgument,
                             "MQTT host must not be empty");
    if (this->options_.port <= 0 || this->options_.port > 65535)
      return Result::failure(ErrorCode::InvalidArgument,
                             "MQTT port must be in range 1..65535");
    if (this->options_.client_id.empty())
      return Result::failure(ErrorCode::InvalidArgument,
                             "MQTT client_id must not be empty");
    if (this->options_.keep_alive_seconds <= 0)
      return Result::failure(ErrorCode::InvalidArgument,
                             "MQTT keep_alive_seconds must be positive");
    if (this->options_.connect_timeout.count() <= 0)
      return Result::failure(ErrorCode::InvalidArgument,
                             "MQTT connect_timeout must be positive");
    return Result::success();
  }

#ifdef M1104_HAS_MOSQUITTO
  /// 在连接失败或超时路径中停止网络循环并恢复未连接状态。
  void stop_loop() {
    if (this->client_)
      mosquitto_disconnect(this->client_);
    if (this->client_ && this->loop_started_)
      mosquitto_loop_stop(this->client_, true);
    this->loop_started_ = false;
    this->connected_.store(false);
  }

  /// libmosquitto CONNACK 回调入口。
  ///
  /// @param client 触发回调的 SDK client，本实现不直接使用。
  /// @param userdata 指向创建 client 的 Impl，不能为空。
  /// @param code MQTT CONNACK 返回码，0 表示成功。
  static void on_connect(struct mosquitto *client, void *userdata, int code) {
    (void)client;
    Impl *self = static_cast<Impl *>(userdata);
    if (!self)
      return;

    {
      std::lock_guard<std::mutex> lock(self->connect_mutex_);
      self->connect_code_ = code;
      self->connect_completed_ = true;
    }
    self->connected_.store(code == 0);
    self->connect_cv_.notify_all();
  }

  /// libmosquitto 断开回调入口。
  ///
  /// @param client 触发回调的 SDK client，本实现不直接使用。
  /// @param userdata 指向创建 client 的 Impl。
  /// @param code 断开原因码，本层只维护连接状态。
  static void on_disconnect(struct mosquitto *client, void *userdata, int code) {
    (void)client;
    (void)code;
    Impl *self = static_cast<Impl *>(userdata);
    if (self)
      self->connected_.store(false);
  }

  /// libmosquitto 消息回调入口。
  ///
  /// @param client 触发回调的 SDK client，本实现不直接使用。
  /// @param userdata 指向创建 client 的 Impl。
  /// @param native_message SDK 消息，只在本回调期间有效。
  static void on_message(struct mosquitto *client, void *userdata,
                         const struct mosquitto_message *native_message) {
    (void)client;
    Impl *self = static_cast<Impl *>(userdata);
    if (!self || !native_message)
      return;

    Message value;
    if (native_message->topic)
      value.topic = native_message->topic;
    if (native_message->payload && native_message->payloadlen > 0) {
      value.payload.assign(static_cast<const char *>(native_message->payload),
                           static_cast<std::size_t>(native_message->payloadlen));
    }
    value.qos = static_cast<QoS>(native_message->qos);
    value.retained = native_message->retain;

    MessageHandler handler;
    {
      std::lock_guard<std::mutex> lock(self->handler_mutex_);
      handler = self->handler_;
    }
    if (!handler)
      return;

    // C SDK callback 边界不能允许 C++ 异常逃逸，否则会穿过 C ABI。
    try {
      handler(value);
    } catch (const std::exception &error) {
      M1104_LOG_ERROR("MQTT message handler threw exception: ", error.what());
    } catch (...) {
      M1104_LOG_ERROR("MQTT message handler threw unknown exception");
    }
  }
#endif

  ClientOptions options_;
  std::atomic<bool> connected_;
  mutable std::mutex handler_mutex_;
  MessageHandler handler_;

#ifdef M1104_HAS_MOSQUITTO
  std::mutex lifecycle_mutex_;
  struct mosquitto *client_;
  bool loop_started_;
  std::mutex connect_mutex_;
  std::condition_variable connect_cv_;
  bool connect_completed_;
  int connect_code_;
#endif
};

/// 查询 libmosquitto 后端可用性。
bool available() {
#ifdef M1104_HAS_MOSQUITTO
  return mosquitto_library().ready();
#else
  return false;
#endif
}

/// 保存实现对象，不在构造阶段访问网络。
Client::Client(const ClientOptions &options) : impl_(new Impl(options)) {}

/// 通过 Impl 的 RAII 路径释放网络资源。
Client::~Client() noexcept {}

/// 转发连接操作。
Result Client::connect() { return this->impl_->connect(); }

/// 转发断开操作。
Result Client::disconnect() { return this->impl_->disconnect(); }

/// 转发发布原语。
Result Client::publish(const std::string &topic, const std::string &payload,
                       QoS qos, bool retained) {
  return this->impl_->publish(topic, payload, qos, retained);
}

/// 转发订阅原语。
Result Client::subscribe(const std::string &topic_filter, QoS qos) {
  return this->impl_->subscribe(topic_filter, qos);
}

/// 转发取消订阅原语。
Result Client::unsubscribe(const std::string &topic_filter) {
  return this->impl_->unsubscribe(topic_filter);
}

/// 转发消息回调替换。
void Client::set_message_handler(const MessageHandler &handler) {
  this->impl_->set_message_handler(handler);
}

/// 返回当前连接状态。
bool Client::connected() const { return this->impl_->connected(); }

} // namespace mqtt
} // namespace mental1104
