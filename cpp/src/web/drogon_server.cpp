#include "mental1104/web/drogon_server.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpFilter.h>
#include <drogon/WebSocketController.h>
#include <trantor/net/TcpClient.h>

#include <algorithm>
#include <atomic>
#include <future>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef __linux__
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace mental1104 {
namespace web {
namespace detail {
namespace {
/// 进程单例后端只允许运行一次，避免已注册路由及事件循环状态被误用。
std::atomic<bool> runtime_claimed{false};

#ifdef __linux__
/// 为 Drogon 的多个 SO_REUSEPORT 监听器预留同一个临时端口。
/// 只绑定、不 listen；在 SDK 全部绑定完成前持有端口，不接收业务连接。
class ListenerReservation {
public:
    /// 创建独占管理的 IPv4/IPv6 套接字；失败由公共启动边界捕获。
    /// @param family SDK 已解析地址的地址族。
    explicit ListenerReservation(int family)
        : fd_(::socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0)) {
        if (this->fd_ < 0)
            throw std::runtime_error("cannot reserve listener socket");
    }

    /// 释放预留；SDK 已绑定的监听套接字拥有独立生命周期。
    ~ListenerReservation() { ::close(this->fd_); }
    ListenerReservation(const ListenerReservation &) = delete;
    ListenerReservation &operator=(const ListenerReservation &) = delete;

    /// 允许 SDK 共享绑定，并让内核分配实际端口，失败抛 runtime_error。
    /// @param address 端口为零的 SDK 地址，只读借用。
    /// @return 预留成功的实际端口；在本对象销毁前不会出现释放再绑定的空窗。
    std::uint16_t bind(const trantor::InetAddress &address) {
        const int enabled = 1;
        if (::setsockopt(this->fd_, SOL_SOCKET, SO_REUSEPORT, &enabled,
                sizeof(enabled)) != 0)
            throw std::runtime_error("cannot enable listener port sharing");
        const socklen_t length =
            address.isIpV6() ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
        if (::bind(this->fd_, address.getSockAddr(), length) != 0)
            throw std::runtime_error("cannot reserve listener port");
        sockaddr_in6 bound{};
        socklen_t bound_length = sizeof(bound);
        if (::getsockname(this->fd_, reinterpret_cast<sockaddr *>(&bound),
                &bound_length) != 0)
            throw std::runtime_error("cannot query reserved listener port");
        trantor::InetAddress actual;
        actual.setSockAddrInet6(bound);
        return actual.toPort();
    }

private:
    int fd_;
};
#endif

/// 全部异步提交与会话共享的停止标志，不保存 Server 或 SDK 所有权。
struct Runtime {
    std::atomic<bool> stopping{false};
    std::atomic<bool> sdk_running{false};
    std::atomic<bool> quit_submitted{false};
};

/// 管理 SDK 强制创建的上传缓存目录，不在应用工作目录留下 uploads 树。
class UploadWorkspace {
public:
    /// 在系统临时目录原子创建本实例独占的缓存目录，失败通过异常报告。
    UploadWorkspace() {
        const auto prefix =
            "mental1104-web-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            auto candidate = std::filesystem::temp_directory_path() /
                             (prefix + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                this->path_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("cannot create web upload workspace");
    }

    /// SDK 线程退出后删除自己创建的缓存目录，清理失败不传播异常。
    ~UploadWorkspace() noexcept {
        try {
            std::error_code ignored;
            std::filesystem::remove_all(this->path_, ignored);
        } catch (...) {
            // error_code 重载仍可能因资源分配失败抛异常，析构不能向外传播。
        }
    }

    UploadWorkspace(const UploadWorkspace &) = delete;
    UploadWorkspace &operator=(const UploadWorkspace &) = delete;

    /// 返回 SDK 运行期间有效的缓存目录，不转移所有权。
    /// @return 只读路径借用，生命周期与当前对象一致。
    const std::filesystem::path &path() const { return this->path_; }

private:
    std::filesystem::path path_;
};

/// 转换受支持的方法；无效枚举属于调用方配置错误。
/// @param method 公共方法值。
/// @return 对应的 SDK 方法；无效值抛 invalid_argument。
drogon::HttpMethod native_method(Method method) {
    switch (method) {
        case Method::Get:
            return drogon::Get;
        case Method::Head:
            return drogon::Head;
        case Method::Post:
            return drogon::Post;
        case Method::Put:
            return drogon::Put;
        case Method::Delete:
            return drogon::Delete;
        case Method::Patch:
            return drogon::Patch;
        case Method::Options:
            return drogon::Options;
    }
    throw std::invalid_argument("invalid HTTP method");
}

/// 从 SDK 请求复制独立快照，避免异步调用持有后端借用视图。
/// @param request 当前有效的后端请求，不转移所有权。
/// @return 可独立保存的请求值；不支持的 SDK 方法抛 invalid_argument。
Request snapshot(const drogon::HttpRequestPtr &request) {
    Request result;
    switch (request->method()) {
        case drogon::Get:
            result.method = Method::Get;
            break;
        case drogon::Head:
            result.method = Method::Head;
            break;
        case drogon::Post:
            result.method = Method::Post;
            break;
        case drogon::Put:
            result.method = Method::Put;
            break;
        case drogon::Delete:
            result.method = Method::Delete;
            break;
        case drogon::Patch:
            result.method = Method::Patch;
            break;
        case drogon::Options:
            result.method = Method::Options;
            break;
        default:
            throw std::invalid_argument("unsupported HTTP method");
    }
    result.path = request->path();
    result.query = request->query();
    result.body = std::string(request->body());
    result.peer_address = request->peerAddr().toIp();
    for (const auto &field : request->headers()) {
        result.headers.push_back({field.first, field.second});
    }
    return result;
}

/// 构造不会泄漏业务异常细节的固定错误响应。
/// @param status 最终 HTTP 状态。
/// @return 独立后端响应，无业务异常文本。
drogon::HttpResponsePtr error_response(Status status) {
    auto result = drogon::HttpResponse::newHttpResponse();
    result->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
    return result;
}

/// 尽力交付错误响应，连兜底构造或 SDK 回调失败也不穿透网络边界。
/// @param callback SDK 提交入口，只在本次调用期间借用。
/// @param status 要交付的状态。
void send_error(
    const std::function<void(const drogon::HttpResponsePtr &)> &callback,
    Status status) noexcept {
    try {
        callback(error_response(status));
    } catch (...) {
    }
}

/// 尽力关闭异常连接，避免清理动作本身的异常逃出 SDK 回调。
/// @param connection SDK 拥有的连接，不保存额外引用。
void close_connection(
    const drogon::WebSocketConnectionPtr &connection) noexcept {
    try {
        connection->forceClose();
    } catch (...) {
    }
}

/// 将字面路由转换为锚定正则，防止 SDK 将花括号等解释成路由参数。
/// @param path 已校验的字面路径。
/// @return 仅匹配该路径的正则表达式。
std::string literal_pattern(const std::string &path) {
    std::string result = "^";
    for (const char value : path) {
        if (std::string("\\.^$|()[]{}*+?").find(value) != std::string::npos) {
            result += '\\';
        }
        result += value;
    }
    return result + "$";
}

/// 构造单段文件响应，无法满足时交付带实体总长度的 416。
/// @param file 已校验名称且已取得文件大小的下载描述。
/// @param size 文件总字节数。
/// @param range 不含 bytes= 的单段范围文本。
/// @return SDK 范围响应，保留 SDK 打开文件时产生的错误状态。
drogon::HttpResponsePtr ranged_file_response(
    const FileBody &file, std::size_t size, const std::string &range) {
    const auto selected = parse_range(range, size);
    if (!selected) {
        auto result = error_response(Status::RangeNotSatisfiable);
        result->addHeader("content-range", "bytes */" + std::to_string(size));
        return result;
    }
    return drogon::HttpResponse::newFileResponse(file.path.string(),
        selected->offset, selected->length, true, file.download_name);
}

/// 构造完整文件响应，仅在 SDK 成功打开文件时应用业务状态。
/// @param file 已校验的文件描述。
/// @param status 应用指定的响应状态。
/// @return 独立文件响应，不把 SDK 文件错误覆盖为业务成功状态。
drogon::HttpResponsePtr full_file_response(
    const FileBody &file, Status status) {
    auto result = drogon::HttpResponse::newFileResponse(
        file.path.string(), file.download_name);
    if (result->statusCode() == drogon::k200OK) {
        result->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
    }
    return result;
}

/// 校验下载描述并选择完整或范围文件响应。
/// @param file 只读文件描述，非法下载名称抛 invalid_argument。
/// @param status 应用响应状态。
/// @param request 只读取范围请求所需的元信息。
/// @return 文件不存在或大小无法表示时为空，由转换入口直接回复
/// 404；否则交付文件响应。
drogon::HttpResponsePtr file_response(
    const FileBody &file, Status status, const Request &request) {
    const auto size = response_file_size(file);
    if (!size) return {};
    const auto range = single_range_request(request, status);
    auto result = range ? ranged_file_response(file, *size, *range)
                        : full_file_response(file, status);
    result->addHeader("accept-ranges", "bytes");
    return result;
}

/// 按响应体类型分派转换，文件处理与内存响应构造彼此独立。
/// @param response 借用可移动的业务响应，仅移出内存响应体。
/// @param request 只读请求元信息，供文件范围处理使用。
/// @return 独立 SDK 响应，尚未附加应用响应头；文件预检查失败时为空。
drogon::HttpResponsePtr response_body(
    Response &response, const Request &request) {
    if (const auto *file = std::get_if<FileBody>(&response.body)) {
        return file_response(*file, response.status, request);
    }
    auto result = drogon::HttpResponse::newHttpResponse();
    result->setStatusCode(static_cast<drogon::HttpStatusCode>(response.status));
    result->setBody(std::move(std::get<std::string>(response.body)));
    return result;
}

/// 将已校验并规范化的业务头写入 SDK 响应。
/// @param result 借用并修改的 SDK 响应，不转移指针所有权。
/// @param headers response_headers 已校验的只读字段。
void apply_response_headers(
    drogon::HttpResponse &result, const std::vector<Header> &headers) {
    for (const auto &field : headers) {
        if (field.name == "content-type") {
            result.setContentTypeString(field.value);
            continue;
        }
        result.addHeader(field.name, field.value);
    }
}

/// 转换业务响应：检查状态与响应头、构造响应体、附加业务头。
/// @param response 接管响应内容。
/// @param request 只读请求快照。
/// @return 独立 SDK 响应；内部异常由响应提交或握手边界捕获，不进入业务层。
drogon::HttpResponsePtr native_response(
    Response response, const Request &request) {
    if (!is_valid_response_status(response.status)) {
        throw std::invalid_argument("response status must be in 200..599");
    }
    const auto headers = response_headers(response.headers);
    auto result = response_body(response, request);
    // 保留文件预检查失败时直接回复 404 的契约；SDK
    // 打开文件后的错误仍附加业务头。
    if (!result) return error_response(Status::NotFound);
    apply_response_headers(*result, headers);
    return result;
}

/// 共享的一次性响应状态；未完成的最后一个引用释放时兜底返回 500。
struct ReplyState {
    std::atomic<bool> submitted{false};
    std::shared_ptr<Runtime> runtime;
    Request metadata;
    std::function<void(const drogon::HttpResponsePtr &)> callback;

    /// 捕获回调参数并保存共享停止状态。
    /// @param owner 不拥有 Server 的运行状态。
    /// @param request 只保存方法和 Range 等头部，不保留大请求体。
    /// @param send SDK 的响应提交回调，接管其副本。
    ReplyState(std::shared_ptr<Runtime> owner, Request request,
        std::function<void(const drogon::HttpResponsePtr &)> send)
        : runtime(std::move(owner)),
          metadata(std::move(request)),
          callback(std::move(send)) {}

    /// 最后引用释放时确保遗漏响应不会永久占用连接；清理不向外抛异常。
    ~ReplyState() {
        if (!this->submitted.exchange(true) &&
            !this->runtime->stopping.load()) {
            try {
                this->callback(error_response(Status::InternalServerError));
            } catch (...) {
            }
        }
    }

    /// 至多接受一次应用响应，非法响应和转换异常统一变为 500。
    /// @param response 接管应用响应。
    /// @return 首次且服务未停止时为 true，重复提交或停止后为 false。
    bool submit(Response response) noexcept try {
        if (this->submitted.exchange(true) || this->runtime->stopping.load())
            return false;
        drogon::HttpResponsePtr result;
        try {
            result = native_response(std::move(response), this->metadata);
        } catch (...) {
            result = error_response(Status::InternalServerError);
        }
        this->callback(result);
        return true;
    } catch (...) {
        // 包含 SDK 提交和兜底响应构造失败，异步业务线程不需要捕获异常。
        return false;
    }
};

/// 将 HTTP 处理函数绑定到一次性提交状态，不让业务感知 SDK 请求或回调。
/// @param handler 应用回调，借用期间不得修改其函数包装器。
/// @param runtime 跨异步提交共享的运行状态。
/// @param request 当前后端请求。
/// @param callback 后端响应提交回调，接管副本。
void dispatch(const HttpHandler &handler,
    const std::shared_ptr<Runtime> &runtime,
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> callback) {
    try {
        auto value = snapshot(request);
        Request metadata;
        metadata.method = value.method;
        metadata.headers = value.headers;
        auto state = std::make_shared<ReplyState>(
            runtime, std::move(metadata), callback);
        // HTTP 回调可复制此句柄交给任意执行器，原子门保证响应只发送一次。
        Respond respond = [state](Response response) {
            return state->submit(std::move(response));
        };
        try {
            handler(std::move(value), respond);
        } catch (...) {
            Response failure;
            failure.status = Status::InternalServerError;
            respond(std::move(failure));
        }
    } catch (...) {
        // 快照和提交状态创建同样属于适配器边界；尽力回复，失败也不穿透 SDK。
        send_error(callback, Status::InternalServerError);
    }
}

/// SDK 连接的弱引用适配器，互斥保护关闭顺序，不构成连接与上下文的循环所有权。
class DrogonSession final : public WebSocketSession {
public:
    /// 保存弱连接及共享运行状态，不阻止后端释放连接。
    /// @param connection 后端管理的连接。
    /// @param runtime 共享停止标志。
    /// @param limit 单条消息上限，单位字节。
    DrogonSession(const drogon::WebSocketConnectionPtr &connection,
        std::shared_ptr<Runtime> runtime, std::size_t limit)
        : connection_(connection),
          runtime_(std::move(runtime)),
          limit_(limit) {}

    /// 按连接顺序发送；Accepted 仅表示 SDK 已接受，不承诺缓冲区已排空。
    /// @param message 只读完整消息。
    /// @return 关闭、超限或已接受。
    SendResult send(const Message &message) noexcept override try {
        std::lock_guard<std::mutex> lock(this->mutex_);
        const auto connection = this->connection_.lock();
        if (this->closing_ || this->runtime_->stopping.load() || !connection ||
            !connection->connected()) {
            return SendResult::Closed;
        }
        if (message.data.size() > this->limit_) return SendResult::TooLarge;
        connection->send(
            message.data, message.type == MessageType::Text
                              ? drogon::WebSocketMessageType::Text
                              : drogon::WebSocketMessageType::Binary);
        return SendResult::Accepted;
    } catch (...) {
        return SendResult::Failed;
    }

    /// 先阻止新发送，再交给后端完成正常关闭握手。
    void close() noexcept override try {
        std::lock_guard<std::mutex> lock(this->mutex_);
        if (this->closing_) return;
        this->closing_ = true;
        // 停止后 SDK 事件循环可能已经释放，保留的公共句柄不得再调度网络操作。
        if (this->runtime_->stopping.load()) return;
        if (const auto connection = this->connection_.lock())
            connection->shutdown();
    } catch (...) {
        // 关闭为尽力操作，SDK 异常不能进入持有会话的业务线程。
    }

private:
    std::weak_ptr<drogon::WebSocketConnection> connection_;
    std::shared_ptr<Runtime> runtime_;
    std::size_t limit_;
    std::mutex mutex_;
    bool closing_{false};
};

/// 字面路径到不可变 WebSocket 回调的映射，启动后不再修改。
using SocketRoutes = std::map<std::string, WebSocketHandler>;

/// SDK 连接私有上下文，只保留公共会话及本路由回调。
struct SocketContext {
    std::shared_ptr<WebSocketSession> session;
    WebSocketHandler handler;
};

}  // namespace

/// 握手过滤器在升级前执行应用鉴权；运行时注册，不自动创建全局实例。
class HandshakeFilter final
    : public drogon::HttpFilter<HandshakeFilter, false> {
public:
    /// 共享启动后只读的路由表。
    /// @param routes 不可变路由表，不包含服务对象引用。
    explicit HandshakeFilter(std::shared_ptr<const SocketRoutes> routes)
        : routes_(std::move(routes)) {}

    /// 执行对应路由的鉴权，异常映射为 500，未配置鉴权时继续升级。
    /// @param request 后端握手请求，只在回调期间借用。
    /// @param reject 返回 HTTP 响应以拒绝升级。
    /// @param accept 继续后端升级流程，只调用一次。
    void doFilter(const drogon::HttpRequestPtr &request,
        drogon::FilterCallback &&reject,
        drogon::FilterChainCallback &&accept) noexcept override {
        try {
            const auto found = this->routes_->find(request->path());
            if (found == this->routes_->end()) {
                send_error(reject, Status::NotFound);
                return;
            }
            if (found->second.handshake) {
                const auto value = snapshot(request);
                auto denial = found->second.handshake(value);
                if (denial) {
                    reject(native_response(std::move(*denial), value));
                    return;
                }
            }
            accept();
        } catch (...) {
            send_error(reject, Status::InternalServerError);
            return;
        }
    }

private:
    std::shared_ptr<const SocketRoutes> routes_;
};

/// 单个私有 SDK 控制器分派所有字面 WebSocket 路由，不向应用暴露反射机制。
class SocketController final
    : public drogon::WebSocketController<SocketController, false> {
public:
    /// 保存只读路由及共享停止标志。
    /// @param routes 启动后不再修改的回调表。
    /// @param runtime 共享运行状态。
    /// @param limit 单条消息字节上限。
    SocketController(std::shared_ptr<const SocketRoutes> routes,
        std::shared_ptr<Runtime> runtime, std::size_t limit)
        : routes_(std::move(routes)),
          runtime_(std::move(runtime)),
          limit_(limit) {}

    /// 路径由适配器显式注册，此钩子满足 SDK 的运行时控制器协议。
    static void initPathRouting() {}

    /// 建立会话上下文后通知业务；异常关闭连接，禁止穿透 I/O 事件循环。
    /// @param request 后端握手请求，不保存借用。
    /// @param connection 后端管理的连接。
    void handleNewConnection(const drogon::HttpRequestPtr &request,
        const drogon::WebSocketConnectionPtr &connection) noexcept override {
        try {
            const auto found = this->routes_->find(request->path());
            if (found == this->routes_->end()) {
                close_connection(connection);
                return;
            }
            auto context = std::make_shared<SocketContext>();
            context->session = std::make_shared<DrogonSession>(
                connection, this->runtime_, this->limit_);
            context->handler = found->second;
            connection->setContext(context);
            if (context->handler.opened)
                context->handler.opened(snapshot(request), context->session);
        } catch (...) {
            close_connection(connection);
        }
    }

    /// 将完整文本/二进制消息交给应用，控制帧保持由 SDK 管理。
    /// @param connection 当前连接，借用其私有上下文。
    /// @param data 接管完整消息字节。
    /// @param type 后端消息类型。
    void handleNewMessage(const drogon::WebSocketConnectionPtr &connection,
        std::string &&data,
        const drogon::WebSocketMessageType &type) noexcept override {
        if (type != drogon::WebSocketMessageType::Text &&
            type != drogon::WebSocketMessageType::Binary)
            return;
        try {
            const auto context = connection->getContext<SocketContext>();
            if (context && !this->runtime_->stopping.load()) {
                context->handler.message(context->session,
                    Message{type == drogon::WebSocketMessageType::Text
                                ? MessageType::Text
                                : MessageType::Binary,
                        std::move(data)});
            }
        } catch (...) {
            close_connection(connection);
        }
    }

    /// 通知业务连接已断开；应用清理异常不影响其他连接。
    /// @param connection 已关闭连接，不延长其生命周期。
    void handleConnectionClosed(
        const drogon::WebSocketConnectionPtr &connection) noexcept override {
        try {
            const auto context = connection->getContext<SocketContext>();
            if (context && context->handler.closed)
                context->handler.closed(context->session);
            connection->clearContext();
        } catch (...) {
            connection->clearContext();
        }
    }

private:
    std::shared_ptr<const SocketRoutes> routes_;
    std::shared_ptr<Runtime> runtime_;
    std::size_t limit_;
};

/// Drogon 仅实现 SDK 运行钩子，配置与生命周期约束由 Server 固定。
class DrogonServer final : public Server {
public:
    /// 将公共配置校验交给基类。
    /// @param options 只读启动配置，由基类复制保存。
    explicit DrogonServer(const ServerOptions &options) : Server(options) {}

    /// 在派生类状态销毁前执行公共停止与等待流程。
    ~DrogonServer() override { this->shutdown(); }

protected:
    /// 先检查后端保留路由，再配置 SDK；实际绑定留给监听阶段。
    /// @param configuration 基类拥有的冻结配置，仅在运行期间借用。
    /// OPTIONS /* 不受 SDK 路由拦截器控制，配置异常由公共 start 转为 false。
    void do_configure(const Configuration &configuration) override {
        // Drogon 1.8 的 onAsyncRequest 在 pre-routing advice 前直接响应
        // OPTIONS /*，显式 handler 和 CORS 预检均无法执行，因此拒绝启动。
        // 在占用进程单例或修改 SDK 配置前检查，失败不影响其他实例启动；
        // 普通 GET /* 仍是可用的字面路由，不在这里扩大限制。
        for (const auto &route : configuration.http_routes) {
            if (route.method == Method::Options && route.path == "/*")
                throw std::invalid_argument("Drogon reserves OPTIONS /*");
        }
        this->claim_runtime();
        this->configure_options(configuration.options);
        this->configure_listener(configuration.options.listener);
        this->register_http_routes(configuration.http_routes);
        this->register_websocket_routes(configuration.websocket_routes,
            configuration.options.max_websocket_message_bytes);
    }

    /// SDK 只有阻塞 run 接口，因此内部启动 SDK 线程，等真实监听完成再返回。
    /// @return 已绑定的监听地址；SDK 就绪前异常通过内部 future 传播。
    Endpoint do_listen() override {
        auto startup = std::make_shared<Startup>();
        auto ready = startup->ready.get_future();
        const auto runtime = this->runtime_;
        drogon::app().registerBeginningAdvice([runtime, startup] {
            runtime->sdk_running.store(true);
            try {
                // Linux 每个 I/O 线程均有监听记录；它们必须绑定同一公共地址。
                const auto listeners = drogon::app().getListeners();
                if (listeners.empty())
                    throw std::runtime_error("backend has no listener");
                const auto &address = listeners.front();
                if (!std::all_of(listeners.begin(), listeners.end(),
                        [&address](const trantor::InetAddress &entry) {
                            return entry.toIpPort() == address.toIpPort();
                        }))
                    throw std::runtime_error("backend listeners disagree");
                startup->ready.set_value(
                    Endpoint{address.toIp(), address.toPort()});
                startup->announced = true;
            } catch (...) {
                // SDK 的就绪回调可能由事件线程执行，不能依赖 run_sdk 的外层
                // catch。
                startup->announced = true;
                try {
                    startup->ready.set_exception(std::current_exception());
                } catch (...) {
                }
            }
        });
        this->sdk_thread_ =
            std::thread([this, startup] { this->run_sdk(startup); });
        const auto endpoint = ready.get();
        this->wait_for_listener(endpoint);
#ifdef __linux__
        this->listener_reservation_.reset();
#endif
        return endpoint;
    }

    /// 等待已经启动的 SDK 事件循环退出；保持该阶段对基类的阻塞运行语义。
    /// SDK 线程异常在 join 后重抛，交给基类统一保存。
    void do_run_event_loop() override {
        this->sdk_thread_.join();
        if (this->sdk_failure_) std::rethrow_exception(this->sdk_failure_);
    }

    /// 清理部分配置或已启动的后端；基类在成功与失败路径均调用。
    void do_cleanup() noexcept override {
        this->runtime_->stopping.store(true);
        if (this->sdk_thread_.joinable()) {
            this->stop_sdk();
            this->sdk_thread_.join();
        }
        this->uploads_.reset();
#ifdef __linux__
        this->listener_reservation_.reset();
#endif
    }

    /// 公共状态机保证就绪后只提交一次，不在此重复维护停止状态机。
    void do_request_stop() noexcept override {
        this->runtime_->stopping.store(true);
        this->stop_sdk();
    }

    /// 仅在 SDK 运行期间查询 I/O 线程标识，管理线程自身由基类判断。
    /// @return 当前为 SDK 网络线程时返回 true。
    bool do_is_io_thread() const noexcept override try {
        // SDK 尚未启动时，getLoop 可能仍属于创建 app 的调用方线程。
        // 此时没有 SDK 回调需要保护，不能阻止配置失败后的 wait 回收线程。
        if (!this->runtime_->sdk_running.load()) return false;
        return drogon::app().getLoop()->isInLoopThread() ||
               drogon::app().getCurrentThreadIndex() !=
                   std::numeric_limits<std::size_t>::max();
    } catch (...) {
        // 无法确定调用上下文时拒绝等待，避免把 SDK 查询异常变为进程终止。
        return true;
    }

private:
    /// BeginningAdvice 只说明完成绑定，不能证明各监听线程已执行 listen。
    /// 通过 SDK 主循环建立一次本地 TCP 连接，成功后才允许公共层发布就绪。
    /// @param endpoint 已绑定地址；通配地址转换为同地址族的回环地址。
    /// 连接不发送 HTTP 或 TLS 数据，不调用业务 handler；五秒未就绪则启动失败。
    static void wait_for_listener(const Endpoint &endpoint) {
        auto address = endpoint.address;
        if (address == "0.0.0.0") address = "127.0.0.1";
        if (address == "::") address = "::1";
        const auto connected = std::make_shared<std::promise<void>>();
        auto ready = connected->get_future();
        auto probe =
            std::make_shared<trantor::TcpClient>(drogon::app().getLoop(),
                trantor::InetAddress(address, endpoint.port,
                    address.find(':') != std::string::npos),
                "web-listener-readiness");
        // 管理线程等待真实连接结果；探测成功后关闭连接，不触发业务 handler。
        const std::weak_ptr<trantor::TcpClient> weak_probe = probe;
        probe->setConnectionCallback(
            [connected, weak_probe](
                const trantor::TcpConnectionPtr &connection) {
                if (!connection->connected()) return;
                if (const auto client = weak_probe.lock()) client->stop();
                connection->forceClose();
                connected->set_value();
            });
        // Trantor enableRetry 只控制断线重连，不重试首次连接失败。
        // 使用事件循环定时器重试未 listen 的端口；弱引用确保超时销毁后失效。
        probe->setConnectionErrorCallback([weak_probe] {
            drogon::app().getLoop()->runAfter(0.01, [weak_probe] {
                if (const auto client = weak_probe.lock()) client->connect();
            });
        });
        probe->connect();
        const auto status = ready.wait_for(std::chrono::seconds(5));
        probe->stop();
        if (status != std::future_status::ready)
            throw std::runtime_error("listener readiness timed out");
        ready.get();
    }

    /// 占用进程内唯一 SDK 运行机会；重复配置抛 logic_error，失败后也不重试。
    static void claim_runtime() {
        if (runtime_claimed.exchange(true))
            throw std::logic_error(
                "Drogon runtime can only start once per process");
    }

    /// 创建上传工作区并应用线程、请求大小和空闲超时等 SDK 选项。
    /// @param options 基类已校验的只读选项；SDK 数值范围不兼容时抛
    /// invalid_argument。
    void configure_options(const ServerOptions &options) {
        // 公共配置保留 chrono 单位；SDK 边界显式转换，兼容仅接收 size_t
        // 的头文件。 基类已拒绝负数，这里避免较窄 size_t 平台发生截断。
        if (static_cast<std::uintmax_t>(options.idle_timeout.count()) >
            std::numeric_limits<std::size_t>::max()) {
            throw std::invalid_argument("idle timeout exceeds backend range");
        }
        this->uploads_ = std::make_unique<UploadWorkspace>();
        auto &app = drogon::app();
        app.disableSigtermHandling()
            .setUploadPath(this->uploads_->path().string())
            .setThreadNum(options.io_threads)
            .setClientMaxBodySize(options.max_body_bytes)
            .setClientMaxMemoryBodySize(options.max_body_bytes)
            .setClientMaxWebSocketMessageSize(
                options.max_websocket_message_bytes)
            .setIdleConnectionTimeout(
                static_cast<std::size_t>(options.idle_timeout.count()))
            .setFileTypes({});
    }

    /// 注册唯一公共地址；Linux 临时端口先预留，实际监听仍由 SDK 完成。
    /// @param listener 只读地址、端口及可选 TLS 配置；SDK 缺少 TLS 时抛
    /// runtime_error。
    void configure_listener(const Listener &listener) {
        auto &app = drogon::app();
        if (listener.tls && !app.supportSSL())
            throw std::runtime_error("Drogon has no TLS support");
        auto port = listener.port;
#ifdef __linux__
        // Drogon 1.8 在 Linux 为每个 I/O 线程分别 bind；直接传零会得到
        // 不同端口。持续保留绑定，再让所有 SDK 监听器共享同一实际端口。
        if (port == 0) {
            const trantor::InetAddress address(listener.address, 0,
                listener.address.find(':') != std::string::npos);
            this->listener_reservation_ =
                std::make_unique<ListenerReservation>(address.family());
            port = this->listener_reservation_->bind(address);
            app.enableReusePort(true);
        }
#endif
        app.addListener(listener.address, port, listener.tls.has_value(),
            listener.tls ? listener.tls->certificate.string() : "",
            listener.tls ? listener.tls->private_key.string() : "");
    }

    /// 将冻结的 HTTP 路由逐条转换为 SDK 注册结果。
    /// @param routes 基类拥有的只读路由集合；SDK 回调保存所需字段的副本。
    void register_http_routes(const std::vector<HttpRoute> &routes) {
        auto &app = drogon::app();
        auto options =
            std::make_shared<std::unordered_map<std::string, HttpHandler>>();
        for (const auto &route : routes) {
            // Drogon 1.8 将 Options 路由标记为自动 CORS，绕过用户 handler。
            // 单独保存显式 OPTIONS，在路由前精确分派，策略仍由公共 handler
            // 决定。
            if (route.method == Method::Options) {
                options->emplace(route.path, route.handler);
                continue;
            }
            const auto handler = route.handler;
            const auto runtime = this->runtime_;
            const auto path = route.path;
            const auto method = native_method(route.method);
            // 显式再比较路径，保证框架的大小写匹配策略不会改变公共契约。
            app.registerHandlerViaRegex(literal_pattern(path),
                [handler, runtime, path, method](
                    const drogon::HttpRequestPtr &request,
                    std::function<void(const drogon::HttpResponsePtr &)>
                        &&callback) {
                    if (request->path() != path) {
                        send_error(callback, Status::NotFound);
                        return;
                    }
                    // SDK 已将 HEAD 归一为 GET，并负责在传输时省略响应体。
                    // 其余方法仍须与注册方法一致。
                    if (request->method() != method) {
                        send_error(callback, Status::MethodNotAllowed);
                        return;
                    }
                    dispatch(handler, runtime, request, std::move(callback));
                },
                {native_method(route.method)});
        }
        if (options->empty()) return;
        const auto runtime = this->runtime_;
        app.registerPreRoutingAdvice(
            [options, runtime](const drogon::HttpRequestPtr &request,
                drogon::AdviceCallback &&callback,
                drogon::AdviceChainCallback &&next) {
                if (request->method() != drogon::Options) {
                    next();
                    return;
                }
                const auto found = options->find(request->path());
                if (found == options->end()) {
                    next();
                    return;
                }
                dispatch(found->second, runtime, request, std::move(callback));
            });
    }

    /// 共享路由快照给握手鉴权和消息控制器，再注册各 WebSocket 路径。
    /// @param routes 基类拥有的只读路由集合；为空时不安装过滤器或控制器。
    /// @param message_limit 单条完整消息的字节上限。
    void register_websocket_routes(
        const std::vector<WebSocketRoute> &routes, std::size_t message_limit) {
        if (routes.empty()) return;
        auto sockets = std::make_shared<SocketRoutes>();
        for (const auto &route : routes)
            sockets->emplace(route.path, route.handler);
        auto &app = drogon::app();
        app.registerFilter(std::make_shared<HandshakeFilter>(sockets));
        app.registerController(std::make_shared<SocketController>(
            sockets, this->runtime_, message_limit));
        for (const auto &route : routes) {
            app.registerWebSocketController(route.path,
                SocketController::classTypeName(),
                {HandshakeFilter::classTypeName()});
        }
    }

    /// 内部 SDK 启动握手；announced 只由 SDK 线程访问，不进入公共 Server 契约。
    struct Startup {
        std::promise<Endpoint> ready;
        bool announced{false};
    };

    /// 驱动不可拆分的 Drogon run，将 SDK 启动及运行异常跨线程传递。
    /// @param startup 内部共享启动结果，SDK 回调只在本线程运行期间执行。
    void run_sdk(const std::shared_ptr<Startup> &startup) noexcept {
        try {
            drogon::app().run();
            if (!startup->announced)
                throw std::runtime_error("Drogon exited before listening");
        } catch (...) {
            this->sdk_failure_ = std::current_exception();
            if (!startup->announced)
                startup->ready.set_exception(this->sdk_failure_);
        }
        this->runtime_->sdk_running.store(false);
        this->runtime_->stopping.store(true);
    }

    /// 正常停止与异常清理共用一次 SDK 退出提交，防止 SDK 重复清理监听器。
    void stop_sdk() noexcept try {
        if (this->runtime_->sdk_running.load() &&
            !this->runtime_->quit_submitted.exchange(true))
            drogon::app().quit();
    } catch (...) {
        // 不将 SDK 停止失败传播到请求停止的业务线程。
    }

    std::shared_ptr<Runtime> runtime_{std::make_shared<Runtime>()};
    std::unique_ptr<UploadWorkspace> uploads_;
#ifdef __linux__
    std::unique_ptr<ListenerReservation> listener_reservation_;
#endif
    std::thread sdk_thread_;
    std::exception_ptr sdk_failure_;
};

}  // namespace detail

bool make_drogon_server(const ServerOptions &options,
    std::unique_ptr<Server> &server) noexcept try {
    // 禁止隐式替换运行中的实例；失败保持调用方原有所有权不变。
    if (server) return false;
    server = std::make_unique<detail::DrogonServer>(options);
    return true;
} catch (...) {
    return false;
}

}  // namespace web
}  // namespace mental1104
