#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mental1104 {
namespace web {

/// HTTP 请求方法；路由按方法和字面路径匹配，不解释框架专属占位符。
enum class Method { Get, Head, Post, Put, Delete, Patch, Options };

/// 常用 HTTP 状态；其他标准状态可以显式转换，发送时校验 200..599。
enum class Status : std::uint16_t {
    Ok = 200,
    Created = 201,
    NoContent = 204,
    PartialContent = 206,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    RangeNotSatisfiable = 416,
    InternalServerError = 500,
    ServiceUnavailable = 503
};

/// 一个 HTTP 头字段；第一版响应要求名称唯一，重复名称转换为 500。
struct Header {
    std::string name;
    std::string value;
};

/// 一次请求的独立快照，可移动到异步任务中，不借用后端请求内存。
/// headers 的名称统一为小写；重复请求头的合并规则由 HTTP 后端决定。
struct Request {
    Method method{Method::Get};
    std::string path;
    std::string query;
    std::vector<Header> headers;
    std::string body;
    std::string peer_address;

    /// 不区分 ASCII 大小写查询第一个头字段。
    /// @param name 只读字段名称，不保存调用方引用。
    /// @return 找到时复制字段值；缺失时为空，空字符串与缺失可区分。
    std::optional<std::string> header(const std::string &name) const;
};

/// 下载文件的描述；路径由应用鉴权后提供，适配层不解释业务文件标识。
/// 文件须在传输期间保持可读且内容不变；空 download_name 表示内联展示。
struct FileBody {
    std::filesystem::path path;
    std::string download_name;
};

/// 一个完整响应；字符串按原始字节发送，FileBody 由后端处理文件传输。
/// Content-Length/Transfer-Encoding/Connection/Upgrade 由后端管理，不可手写。
/// 无效状态、非法头或后端转换异常统一返回 500，不泄漏异常文本。
struct Response {
    Status status{Status::Ok};
    std::vector<Header> headers;
    std::variant<std::string, FileBody> body{std::string{}};
};

/// 可跨线程复制的响应提交入口，接管 Response；首次提交返回
/// true，重复、停止后或 SDK 提交异常返回 false。 true
/// 仅表示交给后端，不表示对端收到；最后一个未提交句柄释放时尝试发送 500。
using Respond = std::function<bool(Response)>;

/// HTTP 回调接管请求快照与提交句柄，在 I/O 线程执行；阻塞任务需转交应用执行器。
using HttpHandler = std::function<void(Request, Respond)>;

/// 单个路径的 CORS 白名单；同路径业务方法与 OPTIONS 应复用相同策略。
/// origins 使用完整序列化来源（含协议及非默认端口），不支持通配符；
/// 不替代鉴权。预检不调用业务，实际请求仍由业务鉴权。
struct CorsPolicy {
    /// 一次构造完整策略，字段含义见下方成员；不在构造时校验。
    /// @param origins 精确来源白名单，不支持通配符。
    /// @param methods 同路径允许的方法；Get 同时允许 HEAD。
    /// @param request_headers 允许的额外请求头。
    /// @param exposed_headers 暴露给浏览器的额外响应头。
    /// @param allow_credentials 是否允许凭据。
    /// @param max_age 预检缓存时间；负值在注册时拒绝。
    explicit CorsPolicy(std::vector<std::string> origins,
        std::vector<Method> methods = {Method::Get},
        std::vector<std::string> request_headers = {},
        std::vector<std::string> exposed_headers = {},
        bool allow_credentials = false,
        std::chrono::seconds max_age = std::chrono::seconds{600});
    std::vector<std::string> origins;
    std::vector<Method> methods{Method::Get};  ///< Get 同时允许 HEAD。
    std::vector<std::string>
        request_headers;  ///< 允许的非简单请求头，忽略大小写。
    std::vector<std::string> exposed_headers;  ///< 浏览器可读取的额外响应头。
    bool allow_credentials{false};             ///< 是否允许携带凭据。
    std::chrono::seconds max_age{600};  ///< 浏览器预检缓存秒数，零表示不缓存。
};

/// 同一策略生成的业务与预检处理函数；普通接入由 add_route 自动使用。
struct CorsHandlers {
    HttpHandler request;
    HttpHandler preflight;
};

/// 一次生成业务装饰器和配套预检；包装 Respond，支持异步提交和文件响应。
/// @param policy 复制策略；来源必须精确匹配，空白名单或非法字段返回空。
/// @param handler 复制非空业务处理函数；无 Origin 时照常执行，有 Origin 但
/// 不获允许时返回 403。业务异常转为带许可头的 500，后端自行生成的错误不覆盖。
/// @return 成功返回两个处理函数，非法输入或内部异常返回 nullopt。
/// 策略拥有 CORS 响应头，
/// 合并 Vary，覆盖业务写入的 Access-Control-* 字段。
std::optional<CorsHandlers> with_cors(
    const CorsPolicy &policy, const HttpHandler &handler) noexcept;

/// WebSocket 应用消息类型；协议控制帧由后端管理。
enum class MessageType { Text, Binary };

/// 一条完整的 WebSocket 应用消息，拥有其全部内容。
struct Message {
    MessageType type{MessageType::Text};
    std::string data;
};

/// WebSocket 发送的本地接受结果，不代表已写出或已被对端确认。
enum class SendResult { Accepted, Closed, TooLarge, Failed };

/// 一个可跨异步任务共享的连接句柄；实现负责序列化发送与关闭操作。
/// 持有句柄不会使断开的连接复活，也不会延长服务运行时间。
class WebSocketSession {
public:
    /// 释放句柄；实际网络连接由服务端管理。
    virtual ~WebSocketSession();

    /// 提交一条完整消息，允许跨线程调用；后端输出缓冲没有消费确认语义。
    /// @param message 只读消息，返回前复制需要的字节。
    /// @return 本地接受、已关闭、超限或内部异常对应的 Failed，不抛异常。
    virtual SendResult send(const Message &message) noexcept = 0;

    /// 发起正常关闭，幂等且允许跨线程调用；不等待关闭握手。
    virtual void close() noexcept = 0;
};

/// 一条 WebSocket 路由的回调，运行于连接 I/O 线程；不同连接可能并发执行。
struct WebSocketHandler {
    /// 握手前鉴权：空结果表示接受，否则发送返回的 HTTP 响应并拒绝升级。
    std::function<std::optional<Response>(const Request &)> handshake;
    /// 建连通知；共享会话可保存到业务对象，Request 只在当前回调期间有效。
    std::function<void(const Request &, std::shared_ptr<WebSocketSession>)>
        opened;
    /// 收到完整消息，消息与共享会话均可转交异步任务。
    std::function<void(std::shared_ptr<WebSocketSession>, Message)> message;
    /// 观察到连接断开时通知；服务强制退出不承诺逐连接执行此通知。
    std::function<void(std::shared_ptr<WebSocketSession>)> closed;
};

/// 单个 HTTPS 监听器的证书与私钥文件；缺失或后端无 TLS 支持时启动失败。
struct TlsOptions {
    std::filesystem::path certificate;
    std::filesystem::path private_key;
};

/// 一个监听地址；port=0 请求操作系统分配端口，start 返回实际地址。
struct Listener {
    std::string address{"127.0.0.1"};
    std::uint16_t port{0};
    std::optional<TlsOptions> tls;
};

/// 单监听器的服务启动选项；限制单位为字节，超时单位通过 chrono 表达。
struct ServerOptions {
    Listener listener;
    std::size_t io_threads{1};
    std::size_t max_body_bytes{16 * 1024 * 1024};
    std::size_t max_websocket_message_bytes{1024 * 1024};
    std::chrono::seconds idle_timeout{60};
};

/// 已绑定的监听地址；只描述运行结果，不包含后端对象或证书材料。
struct Endpoint {
    std::string address;
    std::uint16_t port;
};

/// 框架无关的服务端边界；配置在 start 前完成，start 后路由不再可变。
/// 实现拥有运行线程；销毁前会停止并等待，禁止在服务回调内销毁或 wait。
class Server {
public:
    /// 释放公共状态；派生类析构必须先调用
    /// shutdown，不能依赖基类析构分派虚函数。
    virtual ~Server();
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    /// 注册一个字面路径和方法，失败不向外抛异常。
    /// @param method HTTP 方法；Get 同时服务 HEAD，暂不接受独立 Head 注册。
    /// @param path 以 / 开头、不包含查询或片段的字面路径，区分大小写。
    /// @param handler 非空回调，复制后在服务生命周期内保存；HEAD 复用 Get
    /// 回调并以 Get 方法交付，后端负责省略响应体，不应依赖回调区分二者。
    /// @param cors 可选跨域策略；提供后自动装饰业务并原子注册同路径 OPTIONS。
    /// 同路径多个方法须使用等价策略；已有手工 OPTIONS 或策略冲突返回 false。
    /// method 须在策略中，不能给 Options/Head 本身添加该参数。
    /// @return 注册成功为 true；非法、重复、配置冻结或内部异常为 false。
    bool add_route(Method method, const std::string &path,
        const HttpHandler &handler,
        const std::optional<CorsPolicy> &cors = std::nullopt) noexcept;

    /// 注册 WebSocket 路由，握手与普通 HTTP 路由分开分派。
    /// @param path 与 HTTP 路由相同的字面路径约定，不可重复注册。
    /// @param handler 至少提供 message 回调，其余通知与鉴权可省略。
    /// @return 注册成功为 true；非法、重复、配置冻结或内部异常为 false。
    bool add_websocket(
        const std::string &path, const WebSocketHandler &handler) noexcept;

    /// 启动拥有的线程并等待监听器就绪；每个实例只能启动一次。
    /// 启动期间取消时先清理再报告失败；内部异常不向调用方传播。
    /// @param endpoint 仅成功时写入唯一监听地址，失败保持原值。
    /// @return 成功为 true；重复启动、取消或后端失败为 false。
    bool start(Endpoint &endpoint) noexcept;

    /// 请求停止，允许从回调或其他线程调用；不等待在途请求完成，重复调用无副作用。
    /// 尚未 start 时调用表示取消启动，此后 start 返回 false。
    /// @return 请求已记录（包括重复请求）为 true；内部异常为 false。
    bool request_stop() noexcept;

    /// 等待运行线程退出；只允许服务回调之外的单个管理线程调用。
    /// 不主动请求停止，内部异常不向调用方传播。
    /// @return 成功回收为 true；非法等待线程、启动或运行失败为 false。
    bool wait() noexcept;

protected:
    /// 一条已校验的 HTTP 路由，由基类拥有并在启动后冻结。
    struct HttpRoute {
        Method method;
        std::string path;
        HttpHandler handler;
    };
    /// 一条已校验的 WebSocket 路由。
    struct WebSocketRoute {
        std::string path;
        WebSocketHandler handler;
    };
    /// 后端只读配置，生命周期覆盖整个服务运行，禁止保存可变引用。
    struct Configuration {
        ServerOptions options;
        std::vector<HttpRoute> http_routes;
        std::vector<WebSocketRoute> websocket_routes;
    };
    /// 校验并保存框架无关配置，不调用派生类钩子。
    /// @param options 只读启动选项，复制保存；非法配置抛 invalid_argument。
    explicit Server(const ServerOptions &options);

    /// 应用后端配置并注册路由，不启动监听；异常由基类捕获。
    /// @param configuration 基类拥有的冻结配置，服务运行期间只读有效。
    virtual void do_configure(const Configuration &configuration) = 0;
    /// 启动监听并等待监听器实际就绪；无法启动时抛异常。
    /// @return 唯一的实际监听地址；返回后基类统一发布启动结果。
    virtual Endpoint do_listen() = 0;
    /// 驱动或等待后端事件循环，阻塞到退出；后端运行异常向基类传播。
    virtual void do_run_event_loop() = 0;
    /// 清理配置、监听或运行阶段的资源，成功或异常路径都会调用一次。
    /// 须处理部分初始化状态，返回前停止并回收全部后端线程和回调，不得抛异常。
    virtual void do_cleanup() noexcept = 0;
    /// 唤醒并停止后端；就绪后至多调用一次，允许与事件循环结束竞争，不得抛异常。
    virtual void do_request_stop() noexcept = 0;
    /// 判断当前调用线程是否为 SDK I/O 线程；不得抛异常。
    /// @return I/O 回调线程为 true，供公共 wait 阻止自等待。
    virtual bool do_is_io_thread() const noexcept = 0;
    /// 派生类析构的最后运行阶段调用：停止并等待，不传播运行异常。
    /// 必须在派生类成员销毁前调用；从回调内析构属于契约违例并终止进程。
    void shutdown() noexcept;

private:
    /// 公共配置、状态机、同步原语和线程的已编译实现。
    class Impl;
    std::unique_ptr<Impl> impl_;
    /// 检查 HTTP 与 WebSocket 共用的字面路径约束，不读取或修改服务状态。
    /// @param path 只读路径，不保存调用方借用。
    /// @return 非空、以 / 开头且不含查询、片段、换行或 NUL 时返回 true。
    static bool is_valid_route_path(const std::string &path) noexcept;
    /// 在持有配置锁时查询是否仍允许注册或启动，本方法不自行加锁。
    /// @return 处于 Configuring 时为 true，否则为 false。
    [[nodiscard]] bool is_configuring() const noexcept;
    /// 原子裁决启动取消或成功：取消时抛异常交由统一清理，否则交付监听地址。
    /// @param endpoint 接管实际监听地址，只由基类运行流程调用一次。
    void complete_startup(Endpoint endpoint);
    /// 记录停止意图并完成状态迁移，不调用后端。
    /// @return 仅首次从 Running 转为 Stopping 时返回
    /// true，调用方须在锁外停止后端。
    [[nodiscard]] bool prepare_stop();
    /// 在线程可 join 时检查等待上下文；自等待或 I/O 线程等待抛 logic_error。
    void require_wait_allowed() const;
    /// 管理线程入口：运行后端、报告未就绪退出并保存异常。
    void run();
};

namespace detail {
/// 后端共用的响应头校验与名称规范化，不依赖任何 Web SDK。
/// @param headers 只读业务字段，输入保持不变。
/// @return 名称小写化的独立字段；重复、非法或消息定界保留字段抛
/// invalid_argument。
/// 内部辅助函数的异常必须由后端响应提交边界捕获，不能传播至业务调用方。
std::vector<Header> response_headers(const std::vector<Header> &headers);

/// 判断应用状态是否属于可提交的最终响应范围，排除临时响应。
/// @param status 应用提供的状态，允许由整数显式转换得到。
/// @return 数值位于 200..599 时返回 true。
bool is_valid_response_status(Status status) noexcept;

/// 检查下载文件名，避免响应拆分或 C 字符串截断。
/// @param name 只读下载名称，空字符串表示内联展示。
/// @return 不含 CR、LF、NUL 时为 true。
bool is_valid_download_name(const std::string &name) noexcept;

/// 一个可满足的连续字节范围，与底层文件传输 API 无关。
struct ByteRange {
    std::size_t offset;
    std::size_t length;
};

/// 解析单段字节范围，包括后缀及开放结尾形式。
/// @param text 不包含 bytes= 前缀的单段文本。
/// @param size 当前文件总字节数。
/// @return 可满足的偏移和长度；格式错误或无法满足时为空。
std::optional<ByteRange> parse_range(const std::string &text, std::size_t size);

/// 筛选可由公共单段范围实现处理的请求，不检查范围能否满足。
/// @param request 只读请求快照；If-Range 与多段范围沿用完整响应。
/// @param status 应用状态，只有 200 的 GET 响应参与范围处理。
/// @return 去除 bytes= 前缀的范围文本；不参与处理时为空。
std::optional<std::string> single_range_request(
    const Request &request, Status status);

/// 校验下载文件名并查询公共字节范围类型可表示的文件大小。
/// @param file 只读文件描述；非法下载名称抛 invalid_argument。
/// @return 文件大小，文件不存在、无法读取大小或大小不可表示时为空。
std::optional<std::size_t> response_file_size(const FileBody &file);
}  // namespace detail

}  // namespace web
}  // namespace mental1104
