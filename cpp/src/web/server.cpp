/// @file
/// 框架无关的请求查询与 Server 生命周期实现。
/// 调用线程通过 start/wait 与服务管理线程协作；SDK
/// 运行细节只通过受保护钩子接入。
#include "mental1104/web/server.h"

#include <algorithm>
#include <charconv>
#include <future>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace mental1104 {
namespace web {
namespace {
/// 将 ASCII 字母转为小写，不依赖进程 locale。
/// @param value 一个字段名称字节。
/// @return 小写字节，其他字节保持原样。
char ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A'))
                                        : value;
}
/// 解析十进制无符号字节位置，拒绝符号、空白及溢出。
/// @param text 待解析的只读字节序列。
/// @return 完整解析成功时返回数值，否则为空。
std::optional<std::size_t> byte_position(const std::string &text) {
    std::size_t result = 0;
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return result;
}
}  // namespace

/// 解析单段 Range；调用者只在 units=bytes 且单段时调用。
/// @param text 不含 bytes= 的范围说明。
/// @param size 当前文件字节长度。
/// @return 可满足的范围；格式错误或不可满足时为空。
std::optional<detail::ByteRange> detail::parse_range(
    const std::string &text, std::size_t size) {
    const auto dash = text.find('-');
    if (dash == std::string::npos || size == 0) {
        return std::nullopt;
    }
    if (dash == 0) {
        const auto suffix = byte_position(text.substr(1));
        if (!suffix || *suffix == 0) return std::nullopt;
        const auto length = *suffix < size ? *suffix : size;
        return ByteRange{size - length, length};
    }
    const auto first = byte_position(text.substr(0, dash));
    const auto last = dash + 1 == text.size()
                          ? std::optional<std::size_t>(size - 1)
                          : byte_position(text.substr(dash + 1));
    if (!first || !last || *first >= size || *last < *first) {
        return std::nullopt;
    }
    const auto end = *last < size ? *last : size - 1;
    return ByteRange{*first, end - *first + 1};
}

/// 判断状态是否属于公共接口接受的最终响应范围，不接受 1xx 临时响应。
/// @param status 应用提供的状态，可能由整数强制转换得到。
/// @return 数值位于 200..599 时返回 true；不要求枚举列出该范围内的每个状态。
bool detail::is_valid_response_status(Status status) noexcept {
    const auto code = static_cast<unsigned>(status);
    return code >= 200 && code <= 599;
}

/// 检查下载文件名能否安全用于 HTTP 响应头。
/// @param name 只读下载名称；空字符串表示内联展示。
/// @return 不含 CR、LF 或 NUL 时返回 true，避免响应拆分或 C 字符串截断。
bool detail::is_valid_download_name(const std::string &name) noexcept {
    return std::all_of(name.begin(), name.end(), [](char value) {
        switch (value) {
            case '\r':
            case '\n':
            case '\0':
                return false;
            default:
                return true;
        }
    });
}

/// 选出当前实现支持的单段字节范围请求，尚不检查范围能否满足。
/// @param request 只读请求快照；If-Range 验证器匹配尚未实现。
/// @param status 应用响应状态，仅 200 的 GET 响应参与范围处理。
/// @return 去掉 bytes= 的范围文本；缺失、不支持或条件请求均退回完整实体。
std::optional<std::string> detail::single_range_request(
    const Request &request, Status status) {
    if (status != Status::Ok || request.method != Method::Get ||
        request.header("if-range"))
        return std::nullopt;
    const auto range = request.header("range");
    if (!range) return std::nullopt;
    // 未知单位和多段范围沿用完整响应；单段格式错误交由 parse_range 返回 416。
    if (range->compare(0, 6, "bytes=") != 0 ||
        range->find(',') != std::string::npos)
        return std::nullopt;
    return range->substr(6);
}

/// 统一校验下载文件名和文件大小，供任意后端构造文件响应。
/// @param file 只读文件描述，非法下载名称抛 invalid_argument。
/// @return 可表示的文件字节数；查询失败或大小超限时为空。
std::optional<std::size_t> detail::response_file_size(const FileBody &file) {
    if (!is_valid_download_name(file.download_name)) {
        throw std::invalid_argument("invalid download name");
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(file.path, error);
    if (error || size > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(size);
}

/// 校验响应头名称、值与唯一性，并拒绝由后端管理的消息定界字段。
/// @param headers 应用提供的只读字段。
/// @return 规范化后的独立字段；非法或重复名称抛
/// invalid_argument，由后端边界捕获。
std::vector<Header> detail::response_headers(
    const std::vector<Header> &headers) {
    std::vector<Header> result;
    std::set<std::string> names;
    for (const auto &field : headers) {
        std::string name;
        for (const unsigned char value : field.name) {
            const bool alpha = (value >= 'A' && value <= 'Z') ||
                               (value >= 'a' && value <= 'z');
            const bool digit = value >= '0' && value <= '9';
            if (!alpha && !digit &&
                std::string("!#$%&'*+-.^_`|~").find(value) ==
                    std::string::npos) {
                throw std::invalid_argument("invalid response header name");
            }
            name += ascii_lower(static_cast<char>(value));
        }
        if (name.empty() || !names.insert(name).second ||
            name == "content-length" || name == "transfer-encoding" ||
            name == "connection" || name == "upgrade") {
            throw std::invalid_argument(
                "duplicate or reserved response header");
        }
        for (const unsigned char value : field.value) {
            // 允许 HTAB 和非 ASCII 字节，拒绝其他控制字符及 DEL。
            if ((value < 32 && value != '\t') || value == 127) {
                throw std::invalid_argument("invalid response header value");
            }
        }
        result.push_back({std::move(name), field.value});
    }
    return result;
}

/// 按 ASCII 大小写无关规则查找第一个头字段，并复制其值。
/// @param name 只读字段名称，不保存借用。
/// @return 匹配值的独立副本；没有匹配字段时为空。
std::optional<std::string> Request::header(const std::string &name) const {
    const auto found = std::find_if(this->headers.begin(), this->headers.end(),
        [&name](const Header &field) {
            // 四迭代器重载同时检查长度，名称前缀不能被当成完整匹配。
            return std::equal(field.name.begin(), field.name.end(),
                name.begin(), name.end(), [](char left, char right) {
                    return ascii_lower(left) == ascii_lower(right);
                });
        });
    if (found == this->headers.end()) {
        return std::nullopt;
    }
    return found->value;
}

/// 提供公共会话接口的虚析构入口；具体连接的释放由派生实现负责。
WebSocketSession::~WebSocketSession() = default;

/// 独占公共配置、生命周期状态与服务管理线程，不包含任何 Web SDK 类型。
/// mutex 保护状态检查和迁移；线程句柄只由调用方的单个管理线程启动、等待和销毁。
/// 服务运行期间配置已冻结，可无锁只读访问；failure 在 join
/// 后由调用线程读取。
class Server::Impl {
public:
    /// HTTP 唯一键，拥有路径副本，不借用可能扩容的路由列表。
    struct RouteKey {
        Method method;
        std::string path;
        /// 比较方法与路径，大小写规则与原有字面路由一致。
        /// @param other 只读候选键。
        /// @return 方法和路径均相同时为 true。
        bool operator==(const RouteKey &other) const noexcept {
            return this->method == other.method && this->path == other.path;
        }
    };
    /// 将方法与路径组合为哈希值，冲突仍由 RouteKey 的完整相等比较裁决。
    struct RouteKeyHash {
        /// 计算键的哈希，不分配额外路径字符串。
        /// @param key 只读路由键。
        /// @return 用于哈希桶定位的数值。
        std::size_t operator()(const RouteKey &key) const noexcept {
            return std::hash<std::string>{}(key.path) ^
                   (std::hash<unsigned>{}(static_cast<unsigned>(key.method))
                       << 1);
        }
    };
    /// 仅配置阶段需要的重复检查索引，冻结配置时整体销毁以释放桶和路径副本。
    struct RouteIndexes {
        std::unordered_set<RouteKey, RouteKeyHash> http;
        std::unordered_set<std::string> websocket;
    };
    /// 公共生命周期；Starting 阶段收到停止请求时取消启动并统一清理。
    enum class State { Configuring, Starting, Running, Stopping, Stopped };
    /// 保存配置副本，随后由 Server 构造函数完成公共校验。
    /// @param options 只读配置，不保存调用方借用。
    explicit Impl(const ServerOptions &options)
        : configuration{options, {}, {}} {}
    /// 拥有选项及路由，离开 Configuring 后不再修改。
    Configuration configuration;
    /// 与路由列表在同一状态锁内更新；只有 Configuring 状态允许访问。
    std::optional<RouteIndexes> route_indexes{std::in_place};
    /// 同步路由注册、启动冻结和停止请求的状态迁移。
    std::mutex mutex;
    State state{State::Configuring};
    /// 记录停止意图，包括尚未就绪时收到的取消。
    bool stop_requested{false};
    /// 基类是否执行过就绪发布阶段，不表示后端现在仍在运行。
    bool announced{false};
    /// 向 start 的 future 交付地址或启动异常。
    std::promise<Endpoint> ready;
    /// 拥有执行 run 的 的服务管理线程。
    std::thread thread;
    /// 保存启动或运行异常，供 wait 在 join 后转换为 false。
    std::exception_ptr failure;
};

/// 在任何后端副作用发生前，复制配置并检查框架无关的参数约束。
/// @param options 只读启动选项；复制后不再依赖调用方对象的生命周期。
/// 配置非法时抛 invalid_argument；文件系统检查失败也可能传播 filesystem_error。
Server::Server(const ServerOptions &options)
    : impl_(std::make_unique<Impl>(options)) {
    if (options.io_threads == 0 || options.max_body_bytes == 0 ||
        options.max_websocket_message_bytes == 0 ||
        options.idle_timeout.count() < 0) {
        throw std::invalid_argument("invalid web server options");
    }
    const auto &listener = options.listener;
    if (listener.address.empty())
        throw std::invalid_argument("empty listener address");
    if (listener.tls &&
        (!std::filesystem::is_regular_file(listener.tls->certificate) ||
            !std::filesystem::is_regular_file(listener.tls->private_key))) {
        throw std::invalid_argument(
            "TLS certificate and private key must be readable files");
    }
}

/// 释放已编译公共状态，不在基类析构期间调用后端虚函数。
/// 派生类须先执行 shutdown；若遗漏回收仍可 join 的线程，std::thread
/// 析构将终止进程。
Server::~Server() = default;

/// 统一 HTTP 与 WebSocket 的字面路径格式校验，例如 /、/files 均为有效路径。
/// @param path 只读注册路径，不保存借用、不修改输入；查询参数应由请求另行承载。
/// @return 非空、以 / 开头且每个字符均合法时为 true。
/// 本方法不检查路由重复、回调、服务状态、文件存在性或业务访问权限。
bool Server::is_valid_route_path(const std::string &path) noexcept {
    // 先排除空路径，保证后续 front() 不访问空字符串。
    if (path.empty()) return false;
    // 注册路径从根开始，例如接受 /files，拒绝相对形式 files。
    if (path.front() != '/') return false;

    // all_of 表达“每个字符均满足约束”，遇到首个非法字符即可结束检查。
    return std::all_of(path.begin(), path.end(), [](char value) {
        switch (value) {
            // 查询和片段不属于注册路径，例如 /files?id=1、/files#section。
            case '?':
            case '#':
            // 拒绝换行，避免路径中混入 HTTP 消息的行分隔符。
            case '\r':
            case '\n':
            // std::string 可以包含 NUL，但底层 C 字符串接口可能在此提前截断。
            case '\0':
                return false;
            default:
                return true;
        }
    });
}

/// 在持有配置锁时查询是否仍允许注册或启动，本方法不自行加锁。
/// @return 处于 Configuring 时为 true，其余状态返回 false，供调用方短路退出。
bool Server::is_configuring() const noexcept {
    return this->impl_->state == Impl::State::Configuring;
}

/// 校验并保存 HTTP 路由，将检查重复与插入放在同一临界区，避免并发注册穿透检查。
/// @param method 支持的 HTTP 方法；Get 包含 HEAD，独立 Head 注册返回 false。
/// @param path 复制字面路径，不允许查询、片段、换行或 NUL。
/// @param handler 复制非空处理函数，持有到公共配置销毁。
/// @return 成功为 true；包括复制失败在内的内部异常全部转换为 false。
bool Server::add_route(Method method, const std::string &path,
    const HttpHandler &handler) noexcept try {
    std::lock_guard<std::mutex> lock(this->impl_->mutex);
    if (!this->is_configuring()) return false;
    switch (method) {
        case Method::Get:
        case Method::Post:
        case Method::Put:
        case Method::Delete:
        case Method::Patch:
        case Method::Options:
            break;
        case Method::Head:
            // HEAD 复用 GET；拒绝保存后端无法独立分派的处理函数。
            return false;
        default:
            throw std::invalid_argument("invalid HTTP method");
    }
    if (!Server::is_valid_route_path(path) || !handler) {
        throw std::invalid_argument("invalid HTTP route");
    }
    auto &index = this->impl_->route_indexes->http;
    const auto inserted = index.insert(Impl::RouteKey{method, path});
    if (!inserted.second) return false;
    try {
        this->impl_->configuration.http_routes.push_back(
            {method, path, handler});
    } catch (...) {
        // 回调复制或 vector 扩容失败时撤销本次占位，允许相同路由随后重试。
        index.erase(inserted.first);
        return false;
    }
    return true;
} catch (...) {
    return false;
}

/// 在配置阶段保存 WebSocket 路由；其路径唯一性独立于普通 HTTP 路由表。
/// @param path 复制握手字面路径，采用与 HTTP 路由一致的路径约束。
/// @param handler 复制回调集合；message 必须非空，其他回调可省略。
/// @return 成功为 true，非法、重复、配置冻结或内部异常为 false。
bool Server::add_websocket(
    const std::string &path, const WebSocketHandler &handler) noexcept try {
    std::lock_guard<std::mutex> lock(this->impl_->mutex);
    if (!this->is_configuring()) return false;
    if (!Server::is_valid_route_path(path) || !handler.message) {
        throw std::invalid_argument("invalid WebSocket route");
    }
    auto &index = this->impl_->route_indexes->websocket;
    const auto inserted = index.insert(path);
    if (!inserted.second) return false;
    try {
        this->impl_->configuration.websocket_routes.push_back({path, handler});
    } catch (...) {
        // 索引与列表必须同时提交，不能留下会阻止重试的虚假重复记录。
        index.erase(inserted.first);
        return false;
    }
    return true;
} catch (...) {
    return false;
}

/// 冻结配置并创建服务管理线程，调用线程随后阻塞等待一次启动结果。
/// @param endpoint 成功时写入实际监听地址，失败保持原值。
/// @return 成功为 true；重复启动、取消、线程创建或后端异常均返回 false。
bool Server::start(Endpoint &endpoint) noexcept try {
    std::future<Endpoint> ready;
    {
        std::lock_guard<std::mutex> lock(this->impl_->mutex);
        if (!this->is_configuring()) return false;
        ready = this->impl_->ready.get_future();
        // 状态与路由注册共用一把锁；此后后端可以安全借用不再变化的配置。
        this->impl_->state = Impl::State::Starting;
        // 后端只需要按注册顺序保存的列表；冻结后释放配置期的哈希索引。
        this->impl_->route_indexes.reset();
        try {
            // 钩子在新线程中执行，不在当前配置锁的调用栈中重入派生类。
            this->impl_->thread = std::thread([this] { this->run(); });
        } catch (...) {
            // 线程根本未创建，不能保持 Starting，也不允许再次获取同一个
            // future。
            this->impl_->state = Impl::State::Stopped;
            throw;
        }
    }
    // 必须在释放状态锁后等待，否则就绪通知无法取得同一把锁并完成 promise。
    endpoint = ready.get();
    return true;
} catch (...) {
    return false;
}

/// 提交启动结果：取消与成功在同一临界区裁决，避免遗漏并发停止请求。
/// @param endpoint 后端按值交付的实际监听地址，转移给启动结果。
/// 启动期间已收到停止请求时抛 runtime_error，由 run 统一清理后报告。
void Server::complete_startup(Endpoint endpoint) {
    std::lock_guard<std::mutex> lock(this->impl_->mutex);
    if (this->impl_->stop_requested) {
        // 不进入 Running，也不启动事件循环；已有监听资源由统一清理阶段回收。
        throw std::runtime_error("web server startup cancelled");
    }
    // 此处提交后到达的停止请求按正常运行期停止处理，不再追溯为启动取消。
    // promise 只交付结果，不调用后端或用户回调，可以在状态锁内完成。
    this->impl_->state = Impl::State::Running;
    this->impl_->ready.set_value(std::move(endpoint));
    this->impl_->announced = true;
}

/// 服务管理线程的统一入口：运行后端，并将异常转存为调用线程可观察的结果。
/// 固定执行配置、监听、就绪发布、事件循环和清理；运行异常不逃出线程入口。
void Server::run() {
    std::exception_ptr failure;
    try {
        this->do_configure(this->impl_->configuration);
        auto endpoint = this->do_listen();
        this->complete_startup(std::move(endpoint));
        this->do_run_event_loop();
    } catch (...) {
        failure = std::current_exception();
    }
    // 无论哪个阶段失败都先清理部分初始化资源；不持公共锁调用后端。
    this->do_cleanup();
    std::lock_guard<std::mutex> lock(this->impl_->mutex);
    this->impl_->failure = failure;
    this->impl_->state = Impl::State::Stopped;
    // 启动失败也要在清理完成后唤醒 start；运行失败只由 wait 在 join 后报告。
    if (!this->impl_->announced) this->impl_->ready.set_exception(failure);
}

/// 记录停止意图，并在后端已经就绪时至多调用一次停止钩子；允许跨线程重复调用。
/// 不等待退出；后端停止钩子必须遵守 noexcept 契约。
bool Server::request_stop() noexcept try {
    if (!this->prepare_stop()) return true;
    // 状态迁移已经完成且锁已释放，允许后端同步回调或重入本方法。
    this->do_request_stop();
    return true;
} catch (...) {
    return false;
}

/// 记录停止意图，并在锁内完成对应状态迁移；不调用后端。
/// @return 仅首次将 Running 转为 Stopping 的调用返回 true，负责提交后端停止。
bool Server::prepare_stop() {
    std::lock_guard<std::mutex> lock(this->impl_->mutex);
    this->impl_->stop_requested = true;
    switch (this->impl_->state) {
        case Impl::State::Configuring:
            // 尚未创建线程，直接取消后续启动，无需访问后端。
            this->impl_->state = Impl::State::Stopped;
            this->impl_->route_indexes.reset();
            return false;
        case Impl::State::Starting:
            // complete_startup 检查已记录的意图，再由统一清理阶段回收资源。
            return false;
        case Impl::State::Running:
            // 状态迁移本身保证唯一提交；并发或重入调用只会看到 Stopping。
            this->impl_->state = Impl::State::Stopping;
            return true;
        case Impl::State::Stopping:
        case Impl::State::Stopped:
            return false;
    }
    return false;
}

/// 由调用方管理线程等待服务管理线程退出；本方法本身不请求停止。
/// 只允许单个调用线程管理 thread；join 保证后端写入的 failure 对当前线程可见。
/// @return 回收成功且后端正常退出为 true；非法等待上下文或内部异常为 false。
bool Server::wait() noexcept try {
    if (this->impl_->thread.joinable()) {
        this->require_wait_allowed();
        this->impl_->thread.join();
    }
    if (this->impl_->failure) std::rethrow_exception(this->impl_->failure);
    return true;
} catch (...) {
    return false;
}

/// 在回收线程前检查调用上下文，拒绝会造成死锁的等待。
/// 仅在线程可 join 时调用；服务管理线程自等待或 SDK I/O 线程等待均抛
/// logic_error。
void Server::require_wait_allowed() const {
    // 自等待会直接死锁；阻塞 I/O 线程则可能使后端无法退出，导致间接死锁。
    if (this->impl_->thread.get_id() == std::this_thread::get_id() ||
        this->do_is_io_thread()) {
        throw std::logic_error("cannot wait from a web server callback");
    }
}

/// 供派生类析构执行“请求停止 → 等待退出”，须在派生类成员仍存活时调用。
/// 已记录的运行异常在析构路径被吞掉；线程仍未回收则终止进程，避免继续销毁后端状态。
void Server::shutdown() noexcept {
    this->request_stop();
    this->wait();
    // 仍有线程时不能安全销毁后端；从回调析构违反所有权契约。
    if (this->impl_->thread.joinable()) std::terminate();
}

}  // namespace web
}  // namespace mental1104
