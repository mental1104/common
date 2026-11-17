#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>

#include "mental1104/web/server.h"
using namespace mental1104::web;

namespace {
using namespace std::chrono_literals;
/// 模拟后端的失败位置，便于验证公共启动和等待的异常契约。
enum class RunMode {
    Normal,
    FailConfigure,
    FailListen,
    FailEventLoop,
    FailUnknown
};
/// 观察后端阶段的调用顺序，保证基类固定流程。
enum class Stage { Configure, Listen, EventLoop, Cleanup };
/// 测试拥有的后端同步控制，不借用 Server 生命周期。
struct BackendControl {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered{false};
    bool release_start{true};
    bool stopped{false};
    bool finished{false};
    std::atomic<unsigned> stop_calls{0};
    std::atomic<bool> self_wait_rejected{false};
    std::size_t http_count{0};
    std::size_t websocket_count{0};
    std::vector<Stage> stages;
};
/// 模拟 SDK 的 I/O 线程标识，仅作用于当前测试线程。
thread_local bool backend_io_thread = false;

/// 模拟业务回调在 std::function 复制时抛异常，验证复制发生在捕获边界内。
struct CopyFailureHandler {
    std::shared_ptr<bool> fail;
    /// 保存控制开关，初始构造不模拟失败。
    /// @param flag 测试拥有的失败开关。
    explicit CopyFailureHandler(std::shared_ptr<bool> flag)
        : fail(std::move(flag)) {}
    /// 根据开关模拟非标准复制异常。
    /// @param other 要复制的回调。
    CopyFailureHandler(const CopyFailureHandler &other) : fail(other.fail) {
        if (*this->fail) throw 42;
    }
    /// 移动只转移控制开关，不触发复制失败。
    CopyFailureHandler(CopyFailureHandler &&) noexcept = default;
    /// 提供可注册的空回调；该测试不启动服务。
    /// @param request 不使用的请求快照。
    /// @param respond 不使用的响应入口。
    void operator()(Request request, Respond respond) const {
        (void)request;
        (void)respond;
    }
};

/// 只实现受保护钩子的替代后端，验证公共约束无需复制到派生类。
class TestServer final : public Server {
public:
    /// 保存测试控制与失败模式；公共配置交由基类校验。
    /// @param control 共享同步控制，测试结束前持续有效。
    /// @param mode 后端模拟的失败位置。
    /// @param options 只读公共配置。
    explicit TestServer(std::shared_ptr<BackendControl> control,
        RunMode mode = RunMode::Normal,
        const ServerOptions &options = ServerOptions{})
        : Server(options), control_(std::move(control)), mode_(mode) {}
    /// 派生状态仍有效时停止并等待，验证基类无需析构虚分派。
    ~TestServer() override { this->shutdown(); }

protected:
    /// 记录基类传入的冻结配置；可在部分配置后模拟失败。
    /// @param configuration 基类提供的只读注册结果。
    void do_configure(const Configuration &configuration) override {
        std::lock_guard<std::mutex> lock(this->control_->mutex);
        this->control_->stages.push_back(Stage::Configure);
        this->control_->http_count = configuration.http_routes.size();
        this->control_->websocket_count = configuration.websocket_routes.size();
        if (this->mode_ == RunMode::FailConfigure)
            throw std::runtime_error("configuration failure");
    }

    /// 模拟监听期间的阻塞与失败，返回后由基类完成启动通知。
    /// @return 测试使用的固定监听地址。
    Endpoint do_listen() override {
        {
            std::unique_lock<std::mutex> lock(this->control_->mutex);
            this->control_->stages.push_back(Stage::Listen);
            this->control_->entered = true;
            this->control_->changed.notify_all();
            this->control_->changed.wait(
                lock, [&] { return this->control_->release_start; });
        }
        if (this->mode_ == RunMode::FailListen)
            throw std::runtime_error("listening failure");
        // 非 std::exception 同样必须在公共边界收口。
        if (this->mode_ == RunMode::FailUnknown) throw 42;
        return Endpoint{"127.0.0.1", 12345};
    }

    /// 模拟事件循环，验证自等待保护后阻塞到停止。
    void do_run_event_loop() override {
        {
            std::lock_guard<std::mutex> lock(this->control_->mutex);
            this->control_->stages.push_back(Stage::EventLoop);
        }
        // 管理线程自身即使不是 SDK I/O 线程，也不能等待自己。
        this->control_->self_wait_rejected.store(!this->wait());
        if (this->mode_ == RunMode::FailEventLoop)
            throw std::runtime_error("runtime failure");
        std::unique_lock<std::mutex> lock(this->control_->mutex);
        this->control_->changed.wait(
            lock, [&] { return this->control_->stopped; });
        this->control_->finished = true;
    }

    /// 记录统一清理阶段，允许在配置或监听尚未完成时调用。
    void do_cleanup() noexcept override {
        std::lock_guard<std::mutex> lock(this->control_->mutex);
        this->control_->stages.push_back(Stage::Cleanup);
    }

    /// 重入公共停止以检验锁边界；公共状态机应当阻止第二次钩子调用。
    void do_request_stop() noexcept override {
        this->control_->stop_calls.fetch_add(1);
        this->request_stop();
        {
            std::lock_guard<std::mutex> lock(this->control_->mutex);
            this->control_->stopped = true;
        }
        this->control_->changed.notify_all();
    }
    /// 返回测试模拟的 SDK I/O 线程标志。
    /// @return 当前测试线程标记为 I/O 时返回 true。
    bool do_is_io_thread() const noexcept override { return backend_io_thread; }

private:
    std::shared_ptr<BackendControl> control_;
    RunMode mode_;
};

/// 不依赖 SDK 的后端同样执行公共配置校验、重复检查和启动冻结。
TEST(ServerLifecycle, RegistrationIsValidatedAndFrozen) {
    const auto control = std::make_shared<BackendControl>();
    ServerOptions invalid;
    invalid.io_threads = 0;
    EXPECT_THROW(
        TestServer(control, RunMode::Normal, invalid), std::invalid_argument);
    TestServer server(control);
    Endpoint endpoint{};
    const HttpHandler handler = [](Request, Respond) {};
    EXPECT_FALSE(server.add_route(Method::Get, "relative", handler));
    EXPECT_FALSE(server.add_route(static_cast<Method>(99), "/bad", handler));
    server.add_route(Method::Get, "/same", handler);
    server.add_route(Method::Post, "/same", handler);
    EXPECT_FALSE(server.add_route(Method::Get, "/same", handler));
    WebSocketHandler socket;
    EXPECT_FALSE(server.add_websocket("/socket", socket));
    socket.message = [](std::shared_ptr<WebSocketSession>, Message) {};
    server.add_websocket("/socket", socket);
    EXPECT_FALSE(server.add_websocket("/socket", socket));
    EXPECT_TRUE(server.start(endpoint));
    EXPECT_EQ(12345, endpoint.port);
    EXPECT_FALSE(server.add_route(Method::Get, "/late", handler));
    EXPECT_FALSE(server.start(endpoint));
    auto first_stop =
        std::async(std::launch::async, [&] { server.request_stop(); });
    auto second_stop =
        std::async(std::launch::async, [&] { server.request_stop(); });
    first_stop.get();
    second_stop.get();
    server.wait();
    EXPECT_EQ(2U, control->http_count);
    EXPECT_EQ(1U, control->websocket_count);
    EXPECT_EQ(1U, control->stop_calls.load());
    EXPECT_TRUE(control->self_wait_rejected.load());
    EXPECT_EQ((std::vector<Stage>{Stage::Configure, Stage::Listen,
                  Stage::EventLoop, Stage::Cleanup}),
        control->stages);
}

/// 回调复制失败返回 false 且不留下半条路由，同一路径随后仍可正常注册。
TEST(ServerLifecycle, HandlerCopyFailureReturnsFalseWithoutInsertingRoute) {
    const auto control = std::make_shared<BackendControl>();
    TestServer server(control);
    const auto fail = std::make_shared<bool>(false);
    const HttpHandler handler = CopyFailureHandler(fail);
    *fail = true;
    EXPECT_FALSE(server.add_route(Method::Get, "/copy", handler));
    *fail = false;
    EXPECT_TRUE(server.add_route(Method::Get, "/copy", handler));
    EXPECT_FALSE(server.add_route(Method::Get, "/copy", handler));
    WebSocketHandler socket;
    socket.handshake = [probe = CopyFailureHandler(fail)](const Request &) {
        (void)probe;
        return std::optional<Response>{};
    };
    socket.message = [](std::shared_ptr<WebSocketSession>, Message) {};
    *fail = true;
    EXPECT_FALSE(server.add_websocket("/copy", socket));
    *fail = false;
    EXPECT_TRUE(server.add_websocket("/copy", socket));
    EXPECT_FALSE(server.add_websocket("/copy", socket));
}

/// 扩容后的索引仍能识别早期与末尾路由，HTTP 与 WebSocket 使用独立唯一键。
TEST(ServerLifecycle, RouteIndexesSurviveGrowthAndConfigurationFreeze) {
    const auto control = std::make_shared<BackendControl>();
    TestServer server(control);
    const HttpHandler handler = [](Request, Respond) {};
    WebSocketHandler socket;
    socket.message = [](std::shared_ptr<WebSocketSession>, Message) {};
    for (unsigned i = 0; i < 1024; ++i) {
        const auto path = "/route/" + std::to_string(i);
        ASSERT_TRUE(server.add_route(Method::Get, path, handler));
        ASSERT_TRUE(server.add_websocket(path, socket));
    }
    for (const auto *path : {"/route/0", "/route/512", "/route/1023"}) {
        EXPECT_FALSE(server.add_route(Method::Get, path, handler));
        EXPECT_FALSE(server.add_websocket(path, socket));
        EXPECT_TRUE(server.add_route(Method::Post, path, handler));
    }
    Endpoint endpoint{};
    ASSERT_TRUE(server.start(endpoint));
    EXPECT_EQ(1027U, control->http_count);
    EXPECT_EQ(1024U, control->websocket_count);
    EXPECT_FALSE(server.add_route(Method::Get, "/new", handler));
    EXPECT_FALSE(server.add_websocket("/new", socket));
    EXPECT_TRUE(server.request_stop());
    EXPECT_TRUE(server.wait());
}

/// 同时注册同一路由只能有一次成功，重复检查与保存必须共用状态锁。
TEST(ServerLifecycle, ConcurrentDuplicateRegistrationHasOneWinner) {
    const auto control = std::make_shared<BackendControl>();
    TestServer server(control);
    const HttpHandler handler = [](Request, Respond) {};
    auto first = std::async(std::launch::async,
        [&] { return server.add_route(Method::Get, "/same", handler); });
    auto second = std::async(std::launch::async,
        [&] { return server.add_route(Method::Get, "/same", handler); });
    EXPECT_NE(first.get(), second.get());
}

/// 尚未启动的取消不调用 SDK 停止，也不允许随后启动。
TEST(ServerLifecycle, CancellationBeforeStart) {
    const auto control = std::make_shared<BackendControl>();
    TestServer server(control);
    server.request_stop();
    Endpoint endpoint{"unchanged", 99};
    EXPECT_FALSE(server.start(endpoint));
    EXPECT_EQ(99, endpoint.port);
    EXPECT_TRUE(server.wait());
    EXPECT_EQ(0U, control->stop_calls.load());
}

/// 启动中取消先清理再返回失败，不进入事件循环或调用运行期停止钩子。
TEST(ServerLifecycle, CancellationDuringStartup) {
    const auto control = std::make_shared<BackendControl>();
    control->release_start = false;
    TestServer server(control);
    Endpoint endpoint{"unchanged", 99};
    auto starting =
        std::async(std::launch::async, [&] { return server.start(endpoint); });
    {
        std::unique_lock<std::mutex> lock(control->mutex);
        const bool entered = control->changed.wait_for(
            lock, 3s, [&] { return control->entered; });
        EXPECT_TRUE(entered);
    }
    server.request_stop();
    EXPECT_EQ(0U, control->stop_calls.load());
    {
        std::lock_guard<std::mutex> lock(control->mutex);
        control->release_start = true;
    }
    control->changed.notify_all();
    EXPECT_FALSE(starting.get());
    EXPECT_EQ(99, endpoint.port);
    // start 返回 false 时保证清理已完成；无需先 wait 才能观察这一结果。
    EXPECT_EQ(
        (std::vector<Stage>{Stage::Configure, Stage::Listen, Stage::Cleanup}),
        control->stages);
    EXPECT_FALSE(server.wait());
    EXPECT_EQ(0U, control->stop_calls.load());
}

/// 配置/监听失败先清理再返回启动失败，事件循环失败由 wait 返回 false。
TEST(ServerLifecycle, FailuresTerminateStartupOrWait) {
    for (const auto mode :
        {RunMode::FailConfigure, RunMode::FailListen, RunMode::FailUnknown}) {
        const auto control = std::make_shared<BackendControl>();
        TestServer server(control, mode);
        Endpoint endpoint{};
        EXPECT_FALSE(server.start(endpoint));
        // start 报错时清理已经完成，调用方无需等待异步清理。
        EXPECT_EQ(Stage::Cleanup, control->stages.back());
        EXPECT_FALSE(server.wait());
        const auto expected =
            mode == RunMode::FailConfigure
                ? std::vector<Stage>{Stage::Configure, Stage::Cleanup}
                : std::vector<Stage>{
                      Stage::Configure, Stage::Listen, Stage::Cleanup};
        EXPECT_EQ(expected, control->stages);
    }
    const auto control = std::make_shared<BackendControl>();
    TestServer server(control, RunMode::FailEventLoop);
    Endpoint endpoint{};
    EXPECT_TRUE(server.start(endpoint));
    EXPECT_EQ(12345, endpoint.port);
    EXPECT_FALSE(server.wait());
    EXPECT_EQ((std::vector<Stage>{Stage::Configure, Stage::Listen,
                  Stage::EventLoop, Stage::Cleanup}),
        control->stages);
}

/// SDK I/O 线程不能 wait；派生类析构随后自动请求停止并等待真实退出。
TEST(ServerLifecycle, IoWaitGuardAndDestructorCleanup) {
    const auto control = std::make_shared<BackendControl>();
    {
        TestServer server(control);
        Endpoint endpoint{};
        EXPECT_TRUE(server.start(endpoint));
        backend_io_thread = true;
        EXPECT_FALSE(server.wait());
        backend_io_thread = false;
    }
    EXPECT_EQ(1U, control->stop_calls.load());
    EXPECT_TRUE(control->finished);
}
}  // namespace
