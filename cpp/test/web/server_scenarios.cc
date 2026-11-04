#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <future>
#include <stdexcept>
#include <thread>

#include "backend.h"
#include "wire_client.h"

namespace web_test {
namespace {
using namespace mental1104::web;
using namespace std::chrono_literals;

/// 每个场景独占服务和临时文件；CTest 让每个场景运行在独立进程中。
class ServerScenarios : public testing::Test {
protected:
    /// 创建服务但不启动，场景先按业务需求注册路由。
    void SetUp() override {
        options_.io_threads = 2;
        options_.max_body_bytes = 1024;
        options_.max_websocket_message_bytes = 128;
        ASSERT_TRUE(backend().create(options_, server_)) << backend().name;
        const auto prefix =
            "web-scenario-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        for (unsigned i = 0; i < 100; ++i) {
            const auto candidate = std::filesystem::temp_directory_path() /
                                   (prefix + "-" + std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                directory_ = candidate;
                break;
            }
        }
        ASSERT_FALSE(directory_.empty());
        file_ = directory_ / "payload.txt";
        std::ofstream output(file_, std::ios::binary);
        output << "0123456789";
        ASSERT_TRUE(output.good());
    }

    /// 先回收服务，确保回调不再访问文件，再清理场景资源。
    void TearDown() override {
        if (server_) {
            EXPECT_TRUE(server_->request_stop());
            EXPECT_TRUE(server_->wait());
            server_.reset();
        }
        if (!directory_.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory_, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    /// 启动真实监听器，失败作为场景失败而不是继续使用未初始化端口。
    void start() {
        if (!server_->start(endpoint_) || endpoint_.port == 0)
            throw std::runtime_error("scenario server failed to start");
    }

    /// 请求当前场景服务，不依赖被测 SDK 的客户端。
    /// @param path 请求目标。
    /// @param method 公共 HTTP 方法。
    /// @param body 请求体。
    /// @param headers 请求头。
    /// @return 真实网络响应，网络失败由测试框架报告。
    WireResponse ask(const std::string &path, Method method = Method::Get,
        const std::string &body = {}, const std::vector<Header> &headers = {}) {
        return request(endpoint_, method, path, body, headers, tls_);
    }

    /// 为文件下载场景提供同一份业务处理函数。
    /// @return 捕获文件路径副本的 handler。
    HttpHandler download() const {
        return [path = file_](Request, Respond respond) {
            Response response;
            response.body = FileBody{path, "download.txt"};
            respond(std::move(response));
        };
    }

    ServerOptions options_;
    std::unique_ptr<Server> server_;
    Endpoint endpoint_{};
    std::filesystem::path directory_;
    std::filesystem::path file_;
    bool tls_{false};
};

/// 用户发送请求体、查询参数和头字段，业务处理后完整返回。
TEST_F(ServerScenarios, HttpRequestReachesBusinessHandler) {
    ASSERT_TRUE(server_->add_route(
        Method::Post, "/echo", [](Request request, Respond respond) {
            Response response;
            response.body = request.body + "|" + request.query;
            response.headers = {{"Content-Type", "application/octet-stream"},
                {"X-Observed", request.header("X-Test").value_or("missing")}};
            respond(std::move(response));
        }));
    start();
    const auto result =
        ask("/echo?a=1&a=2", Method::Post, "payload", {{"X-Test", "present"}});
    EXPECT_EQ(200U, result.status);
    EXPECT_EQ("payload|a=1&a=2", result.body);
    EXPECT_EQ("present", result.header("x-observed"));
    EXPECT_EQ("application/octet-stream", result.header("content-type"));
}

/// 字面路径区分大小写；GET 默认提供无正文的 HEAD，其他未注册方法不能成功。
TEST_F(ServerScenarios, RoutesMatchLiteralPathAndExplicitMethod) {
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/literal.{id}", [](Request, Respond respond) {
            Response response;
            response.body = "literal";
            response.headers = {
                {"Content-Type", "text/plain"}, {"ETag", "\"v1\""}};
            respond(std::move(response));
        }));
    EXPECT_FALSE(server_->add_route(Method::Head, "/literal.{id}",
        [](Request, Respond respond) { respond(Response{}); }));
    start();
    EXPECT_EQ("literal", ask("/literal.{id}").body);
    for (const auto *path :
        {"/LITERAL.{id}", "/literal.42", "/missing", "/README.md"})
        EXPECT_EQ(404U, ask(path).status) << path;
    const auto head = ask("/literal.{id}", Method::Head);
    EXPECT_EQ(200U, head.status);
    EXPECT_TRUE(head.body.empty());
    EXPECT_EQ("7", head.header("content-length"));
    EXPECT_EQ("\"v1\"", head.header("etag"));
    EXPECT_EQ("text/plain", head.header("content-type"));
    // 公共契约尚未统一未知方法的 404/405 策略，但不能进入 GET handler。
    const auto post = ask("/literal.{id}", Method::Post);
    EXPECT_TRUE(post.status == 404U || post.status == 405U);
}

/// 用户下载完整文件，或者只查询元信息，文件内容与声明长度一致。
TEST_F(ServerScenarios, FileDownloadAndHeadPreserveMetadata) {
    ASSERT_TRUE(server_->add_route(Method::Get, "/file", download()));
    start();
    const auto file = ask("/file");
    EXPECT_EQ(200U, file.status);
    EXPECT_EQ("0123456789", file.body);
    EXPECT_NE(std::string::npos,
        file.header("content-disposition").find("download.txt"));
    const auto head = ask("/file", Method::Head);
    EXPECT_EQ(200U, head.status);
    EXPECT_TRUE(head.body.empty());
    EXPECT_EQ("10", head.header("content-length"));
}

/// 断点下载涵盖闭区间、后缀、开放结尾、非法范围和完整响应回退。
TEST_F(ServerScenarios, FileRangesSupportResumeAndFallback) {
    ASSERT_TRUE(server_->add_route(Method::Get, "/file", download()));
    start();
    for (const auto &entry : std::vector<std::pair<std::string, std::string>>{
             {"bytes=2-5", "2345"}, {"bytes=-3", "789"}, {"bytes=8-", "89"}}) {
        const auto result =
            ask("/file", Method::Get, "", {{"Range", entry.first}});
        EXPECT_EQ(206U, result.status);
        EXPECT_EQ(entry.second, result.body);
    }
    const auto unsatisfied =
        ask("/file", Method::Get, "", {{"Range", "bytes=20-30"}});
    EXPECT_EQ(416U, unsatisfied.status);
    EXPECT_EQ("bytes */10", unsatisfied.header("content-range"));
    for (const auto &headers : std::vector<std::vector<Header>>{
             {{"Range", "bytes=2-5"}, {"If-Range", "old"}},
             {{"Range", "bytes=0-1,4-5"}}, {{"Range", "items=0-1"}}}) {
        const auto result = ask("/file", Method::Get, "", headers);
        EXPECT_EQ(200U, result.status);
        EXPECT_EQ("0123456789", result.body);
    }
}

/// 不存在的文件返回 404；不安全的下载名称在框架内转换为 500。
TEST_F(ServerScenarios, FileErrorsDoNotEscapeToBusinessThread) {
    ASSERT_TRUE(server_->add_route(Method::Get, "/missing",
        [path = file_.string() + ".missing"](Request, Respond respond) {
            Response response;
            response.body = FileBody{path, ""};
            respond(std::move(response));
        }));
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/unsafe", [path = file_](Request, Respond respond) {
            Response response;
            response.body = FileBody{path, "bad\r\ninjected: true"};
            respond(std::move(response));
        }));
    start();
    EXPECT_EQ(404U, ask("/missing").status);
    EXPECT_EQ(500U, ask("/unsafe").status);
}

/// handler 抛异常或忘记响应时，请求得到 500，服务随后仍能接受请求。
TEST_F(ServerScenarios, HandlerFailuresAreContainedAndServiceContinues) {
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/throw", [](Request, Respond) { throw 42; }));
    ASSERT_TRUE(
        server_->add_route(Method::Get, "/forgot", [](Request, Respond) {}));
    ASSERT_TRUE(server_->add_route(Method::Get, "/ok",
        [](Request, Respond respond) { respond(Response{}); }));
    start();
    for (const auto *path : {"/throw", "/forgot"}) {
        const auto result = ask(path);
        EXPECT_EQ(500U, result.status);
        EXPECT_TRUE(result.body.empty());
    }
    EXPECT_EQ(200U, ask("/ok").status);
}

/// 重复发送同一请求响应只接受第一次，通过 bool 告知第二次失败。
TEST_F(ServerScenarios, ResponseCanOnlyBeSubmittedOnce) {
    const auto repeated = std::make_shared<std::promise<bool>>();
    auto observed = repeated->get_future();
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/once", [repeated](Request, Respond respond) {
            Response response;
            response.body = "first";
            respond(std::move(response));
            repeated->set_value(respond(Response{}));
        }));
    start();
    EXPECT_EQ("first", ask("/once").body);
    ASSERT_EQ(std::future_status::ready, observed.wait_for(3s));
    EXPECT_FALSE(observed.get());
}

/// 非法状态、重复或保留头、响应拆分输入都映射为 500，不泄漏异常文本。
TEST_F(ServerScenarios, InvalidResponsesBecomeInternalServerError) {
    std::vector<Response> invalid(5);
    invalid[0].status = static_cast<Status>(199);
    invalid[1].headers = {{"x-test", "bad\r\nvalue"}};
    invalid[2].headers = {{"X-Test", "one"}, {"x-test", "two"}};
    invalid[3].headers = {{"Content-Length", "999"}};
    invalid[4].headers = {{"bad name", "value"}};
    for (std::size_t i = 0; i < invalid.size(); ++i) {
        ASSERT_TRUE(
            server_->add_route(Method::Get, "/invalid/" + std::to_string(i),
                [response = invalid[i]](
                    Request, Respond respond) { respond(response); }));
    }
    start();
    for (std::size_t i = 0; i < invalid.size(); ++i) {
        const auto result = ask("/invalid/" + std::to_string(i));
        EXPECT_EQ(500U, result.status);
        EXPECT_TRUE(result.body.empty());
    }
}

/// 业务执行器接管请求和响应句柄，稍后从其他线程完成处理，无需 SDK 类型。
TEST_F(ServerScenarios, BusinessThreadCanCompleteDeferredResponse) {
    const auto pending =
        std::make_shared<std::promise<std::pair<Request, Respond>>>();
    auto work = pending->get_future();
    ASSERT_TRUE(server_->add_route(
        Method::Post, "/deferred", [pending](Request request, Respond respond) {
            pending->set_value({std::move(request), std::move(respond)});
        }));
    start();
    auto network = std::async(std::launch::async,
        [&] { return ask("/deferred?q=1", Method::Post, "saved"); });
    ASSERT_EQ(std::future_status::ready, work.wait_for(3s));
    auto task = work.get();
    EXPECT_EQ("saved", task.first.body);
    Response response;
    response.body = task.first.body + "|" + task.first.query;
    EXPECT_TRUE(task.second(std::move(response)));
    EXPECT_EQ("saved|q=1", network.get().body);
}

/// 停止后仍被业务保存的响应句柄返回 false，不能再访问已回收的网络资源。
TEST_F(ServerScenarios, DeferredResponseIsRejectedAfterStop) {
    const auto pending = std::make_shared<std::promise<Respond>>();
    auto work = pending->get_future();
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/pending", [pending](Request, Respond respond) {
            pending->set_value(std::move(respond));
        }));
    start();
    auto network = std::async(std::launch::async, [&] {
        try {
            ask("/pending");
        } catch (const std::exception &) {
        }
    });
    ASSERT_EQ(std::future_status::ready, work.wait_for(3s));
    auto respond = work.get();
    ASSERT_TRUE(server_->request_stop());
    ASSERT_TRUE(server_->wait());
    server_.reset();
    EXPECT_FALSE(respond(Response{}));
    network.get();
}

/// 过大的请求体在进入业务 handler 前被拒绝。
TEST_F(ServerScenarios, BodyLimitProtectsBusinessHandler) {
    const auto called = std::make_shared<std::atomic<bool>>(false);
    ASSERT_TRUE(server_->add_route(
        Method::Post, "/limited", [called](Request, Respond respond) {
            called->store(true);
            respond(Response{});
        }));
    start();
    EXPECT_EQ(
        413U, ask("/limited", Method::Post, std::string(2048, 'x')).status);
    EXPECT_FALSE(called->load());
}

/// 注册去重与冻结对所有后端一致，同路径不同方法或协议可以分别注册。
TEST_F(ServerScenarios, RegistrationAndStartupHaveStableBooleanContract) {
    const HttpHandler handler = [](Request, Respond respond) {
        respond(Response{});
    };
    EXPECT_FALSE(server_->add_route(Method::Get, "relative", handler));
    EXPECT_FALSE(server_->add_route(Method::Get, "/empty", {}));
    ASSERT_TRUE(server_->add_route(Method::Get, "/same", handler));
    EXPECT_FALSE(server_->add_route(Method::Get, "/same", handler));
    ASSERT_TRUE(server_->add_route(Method::Post, "/same", handler));
    WebSocketHandler socket;
    socket.message = [](std::shared_ptr<WebSocketSession>, Message) {};
    ASSERT_TRUE(server_->add_websocket("/same", socket));
    EXPECT_FALSE(server_->add_websocket("/same", socket));
    start();
    Endpoint unchanged{"unchanged", 99};
    EXPECT_FALSE(server_->start(unchanged));
    EXPECT_EQ(99, unchanged.port);
    EXPECT_FALSE(server_->add_route(Method::Get, "/late", handler));
    EXPECT_FALSE(server_->add_websocket("/late", socket));
    EXPECT_TRUE(server_->request_stop());
    EXPECT_TRUE(server_->request_stop());
    EXPECT_TRUE(server_->wait());
}

/// 工厂拒绝非法配置或覆盖已有所有者；启动前取消使后续启动失败。
TEST_F(
    ServerScenarios, InvalidConfigurationAndPreStartCancellationReturnFalse) {
    std::unique_ptr<Server> invalid;
    ServerOptions options;
    options.io_threads = 0;
    EXPECT_FALSE(backend().create(options, invalid));
    EXPECT_FALSE(invalid);
    const auto *owner = server_.get();
    EXPECT_FALSE(backend().create(options_, server_));
    EXPECT_EQ(owner, server_.get());
    EXPECT_TRUE(server_->request_stop());
    Endpoint unchanged{"unchanged", 99};
    EXPECT_FALSE(server_->start(unchanged));
    EXPECT_EQ(99, unchanged.port);
}

/// WebSocket 在升级前鉴权，升级后保持文本与二进制消息类型。
TEST_F(ServerScenarios, WebSocketAuthorizationAndEcho) {
    const auto closed = std::make_shared<std::promise<void>>();
    auto disconnected = closed->get_future();
    WebSocketHandler handler;
    handler.closed = [closed](std::shared_ptr<WebSocketSession>) {
        closed->set_value();
    };
    handler.handshake = [](const Request &request) -> std::optional<Response> {
        if (request.header("authorization") ==
            std::optional<std::string>("Bearer test"))
            return std::nullopt;
        Response response;
        response.status = Status::Forbidden;
        return response;
    };
    handler.message = [](std::shared_ptr<WebSocketSession> session,
                          Message message) { session->send(message); };
    ASSERT_TRUE(server_->add_websocket("/socket", handler));
    start();
    const auto denial = ask("/socket", Method::Get, "",
        {{"Connection", "Upgrade"}, {"Upgrade", "websocket"},
            {"Sec-WebSocket-Version", "13"},
            {"Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ=="}});
    EXPECT_EQ(403U, denial.status);
    WireSocket client;
    client.connect(endpoint_, "/socket", {{"Authorization", "Bearer test"}});
    for (const auto &message :
        std::vector<Message>{{MessageType::Text, "hello"},
            {MessageType::Binary, std::string("a\0b", 3)}}) {
        client.send(message);
        const auto echoed = client.receive();
        ASSERT_TRUE(echoed);
        EXPECT_EQ(message.type, echoed->type);
        EXPECT_EQ(message.data, echoed->data);
    }
    // 模拟客户端连接丢失，验证业务断连通知；服务端正常关闭在下一场景验证。
    client.disconnect();
    ASSERT_EQ(std::future_status::ready, disconnected.wait_for(3s));
}

/// 服务端共享会话可被业务线程使用，超限明确失败，关闭后不会被句柄复活。
TEST_F(ServerScenarios, WebSocketSessionSupportsBusinessThreadAndClose) {
    const auto opened =
        std::make_shared<std::promise<std::shared_ptr<WebSocketSession>>>();
    const auto closed = std::make_shared<std::promise<void>>();
    auto connection = opened->get_future();
    auto disconnected = closed->get_future();
    WebSocketHandler handler;
    handler.opened = [opened](const Request &,
                         std::shared_ptr<WebSocketSession> session) {
        opened->set_value(session);
    };
    handler.message = [](std::shared_ptr<WebSocketSession>, Message) {};
    handler.closed = [closed](std::shared_ptr<WebSocketSession>) {
        closed->set_value();
    };
    ASSERT_TRUE(server_->add_websocket("/socket", handler));
    start();
    WireSocket client;
    client.connect(endpoint_, "/socket");
    ASSERT_EQ(std::future_status::ready, connection.wait_for(3s));
    const auto session = connection.get();
    EXPECT_EQ(SendResult::TooLarge,
        session->send({MessageType::Text, std::string(129, 'x')}));
    EXPECT_EQ(SendResult::Accepted, session->send({MessageType::Text, "push"}));
    const auto message = client.receive();
    ASSERT_TRUE(message);
    EXPECT_EQ("push", message->data);
    session->close();
    session->close();
    EXPECT_FALSE(client.receive());
    ASSERT_EQ(std::future_status::ready, disconnected.wait_for(3s));
    EXPECT_EQ(SendResult::Closed, session->send({MessageType::Text, "late"}));
}

/// HTTPS 也通过相同公共接口配置；不支持 TLS 的后端显式报告跳过。
TEST_F(ServerScenarios, HttpsUsesTheSamePublicRoutes) {
    if (!backend().tls) GTEST_SKIP() << "Backend does not support TLS";
    const auto *certificate_directory = std::getenv("M1104_TEST_TLS_DIR");
    ASSERT_NE(nullptr, certificate_directory);
    server_.reset();
    options_.listener.tls =
        TlsOptions{std::filesystem::path(certificate_directory) / "cert.pem",
            std::filesystem::path(certificate_directory) / "key.pem"};
    ASSERT_TRUE(backend().create(options_, server_));
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/secure", [](Request, Respond respond) {
            Response response;
            response.body = "secure";
            respond(std::move(response));
        }));
    tls_ = true;
    start();
    const auto result = ask("/secure");
    EXPECT_EQ(200U, result.status);
    EXPECT_EQ("secure", result.body);
}
/// 指定路由的跨域许可覆盖文件/HEAD；未装饰路由保持原状。
TEST_F(ServerScenarios, CorsIsScopedToRouteAndPreservesMetadata) {
    const CorsPolicy policy({"https://front.example"}, {Method::Get}, {},
        {"Content-Disposition", "ETag"}, true);
    ASSERT_TRUE(server_->add_route(Method::Get, "/cors", download(), policy));
    ASSERT_TRUE(server_->add_route(Method::Get, "/plain", download()));
    start();
    const std::vector<Header> headers{{"Origin", "https://front.example"}};
    const auto file = ask("/cors", Method::Get, "", headers);
    EXPECT_EQ("0123456789", file.body);
    EXPECT_EQ(
        "https://front.example", file.header("access-control-allow-origin"));
    EXPECT_EQ("true", file.header("access-control-allow-credentials"));
    EXPECT_EQ("Content-Disposition, ETag",
        file.header("access-control-expose-headers"));
    const auto head = ask("/cors", Method::Head, "", headers);
    EXPECT_TRUE(head.body.empty());
    EXPECT_EQ("10", head.header("content-length"));
    EXPECT_EQ(file.header("access-control-allow-origin"),
        head.header("access-control-allow-origin"));
    EXPECT_TRUE(ask("/plain", Method::Get, "", headers)
                    .header("access-control-allow-origin")
                    .empty());
    const auto local = ask("/cors");
    EXPECT_EQ(200U, local.status);
    EXPECT_TRUE(local.header("access-control-allow-origin").empty());
    EXPECT_EQ("Origin", local.header("vary"));
    const auto denied = ask(
        "/cors", Method::Get, "", {{"Origin", "https://front.example.evil"}});
    EXPECT_EQ(403U, denied.status);
    EXPECT_TRUE(denied.header("access-control-allow-origin").empty());
}

/// OPTIONS 独立校验来源、方法和请求头，预检不会执行业务鉴权或处理函数。
TEST_F(ServerScenarios, CorsPreflightChecksPolicyWithoutCallingBusiness) {
    const CorsPolicy policy({"https://front.example"},
        {Method::Get, Method::Post}, {"Authorization", "Content-Type"}, {},
        false, 120s);
    const auto called = std::make_shared<std::atomic<int>>(0);
    ASSERT_TRUE(server_->add_route(
        Method::Post, "/cors",
        [called](Request, Respond respond) {
            ++*called;
            respond(Response{});
        },
        policy));
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/cors",
        [](Request, Respond respond) { respond(Response{}); }, policy));
    start();
    const auto check = [&](const std::string &origin, const std::string &method,
                           const std::string &headers) {
        return ask("/cors", Method::Options, "",
            {{"Origin", origin}, {"Access-Control-Request-Method", method},
                {"Access-Control-Request-Headers", headers}});
    };
    const auto accepted =
        check("https://front.example", "POST", "authorization, CONTENT-TYPE");
    EXPECT_EQ(204U, accepted.status);
    EXPECT_TRUE(accepted.body.empty());
    EXPECT_EQ(
        "GET, HEAD, POST", accepted.header("access-control-allow-methods"));
    EXPECT_EQ("120", accepted.header("access-control-max-age"));
    EXPECT_EQ("Authorization, Content-Type",
        accepted.header("access-control-allow-headers"));
    EXPECT_EQ(
        204U, check("https://front.example", "HEAD", "Authorization").status);
    EXPECT_EQ(
        403U, check("https://other.example", "POST", "Authorization").status);
    EXPECT_EQ(
        403U, check("https://front.example", "DELETE", "Authorization").status);
    EXPECT_EQ(403U, check("https://front.example", "POST", "X-Secret").status);
    EXPECT_EQ(400U, ask("/cors", Method::Options).status);
    EXPECT_EQ(
        403U, ask("/cors", Method::Post, "", {{"Origin", "null"}}).status);
    EXPECT_EQ(0, called->load());
    EXPECT_EQ(200U,
        ask("/cors", Method::Post, "", {{"Origin", "https://front.example"}})
            .status);
    EXPECT_EQ(1, called->load());
}

/// 异步业务返回的错误也携带许可头，合并 Vary 且不能覆盖策略白名单。
TEST_F(ServerScenarios, CorsDecoratesDeferredErrorsAndMergesVary) {
    const CorsPolicy policy({"https://front.example"});
    const auto pending = std::make_shared<std::promise<Respond>>();
    auto work = pending->get_future();
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/cors",
        [pending](Request, Respond respond) {
            pending->set_value(std::move(respond));
        },
        policy));
    ASSERT_TRUE(server_->add_route(
        Method::Get, "/throw", [](Request, Respond) { throw 42; }, policy));
    start();
    auto network = std::async(std::launch::async, [&] {
        return ask(
            "/cors", Method::Get, "", {{"Origin", "https://front.example"}});
    });
    ASSERT_EQ(std::future_status::ready, work.wait_for(3s));
    auto respond = work.get();
    Response error;
    error.status = Status::Forbidden;
    error.headers = {{"Vary", "Accept-Encoding, origin"},
        {"Access-Control-Allow-Origin", "*"},
        {"access-control-allow-origin", "https://evil.example"},
        {"Access-Control-Allow-Credentials", "true"}};
    ASSERT_TRUE(respond(std::move(error)));
    EXPECT_FALSE(respond(Response{}));
    const auto result = network.get();
    EXPECT_EQ(403U, result.status);
    EXPECT_EQ(
        "https://front.example", result.header("access-control-allow-origin"));
    EXPECT_TRUE(result.header("access-control-allow-credentials").empty());
    EXPECT_EQ("Accept-Encoding, origin", result.header("vary"));
    const auto failed =
        ask("/throw", Method::Get, "", {{"Origin", "https://front.example"}});
    EXPECT_EQ(500U, failed.status);
    EXPECT_EQ(
        "https://front.example", failed.header("access-control-allow-origin"));
}

/// 无效策略返回空且不占用路由，之后同路径可以正常注册。
TEST_F(ServerScenarios, CorsInvalidPolicyLeavesRoutesUnchanged) {
    HttpHandler original = [](Request, Respond respond) {
        Response response;
        response.body = "unchanged";
        respond(std::move(response));
    };
    CorsPolicy policy({});
    EXPECT_FALSE(with_cors(policy, original));
    policy.origins = {"*"};
    EXPECT_FALSE(with_cors(policy, original));
    EXPECT_FALSE(
        server_->add_route(Method::Get, "/unchanged", original, policy));
    policy.origins = {"https://front.example\r\nInjected: yes"};
    EXPECT_FALSE(with_cors(policy, original));
    policy.origins = {"https://front.example"};
    policy.request_headers = {"bad name"};
    EXPECT_FALSE(with_cors(policy, original));
    policy.request_headers.clear();
    policy.max_age = -1s;
    EXPECT_FALSE(with_cors(policy, original));
    ASSERT_TRUE(server_->add_route(Method::Get, "/unchanged", original));
    const auto bundle =
        with_cors(CorsPolicy({"https://front.example"}), original);
    ASSERT_TRUE(bundle);
    EXPECT_TRUE(bundle->request);
    EXPECT_TRUE(bundle->preflight);
    start();
    EXPECT_EQ("unchanged", ask("/unchanged").body);
}

/// 手工 OPTIONS 冲突或业务路由重复时，两条注册均不留下新增项。
TEST_F(ServerScenarios, CorsRegistrationConflictsAreAtomic) {
    const CorsPolicy policy({"https://front.example"});
    const HttpHandler plain = [](Request, Respond respond) {
        respond(Response{});
    };
    ASSERT_TRUE(server_->add_route(Method::Options, "/manual", plain));
    EXPECT_FALSE(server_->add_route(Method::Get, "/manual", plain, policy));
    EXPECT_TRUE(server_->add_route(Method::Get, "/manual", plain));
    ASSERT_TRUE(server_->add_route(Method::Get, "/duplicate", plain));
    EXPECT_FALSE(server_->add_route(Method::Get, "/duplicate", plain, policy));
    EXPECT_TRUE(server_->add_route(Method::Options, "/duplicate", plain));
    EXPECT_FALSE(server_->add_route(Method::Post, "/method", plain, policy));
    EXPECT_TRUE(server_->add_route(Method::Options, "/method", plain));
    EXPECT_FALSE(server_->add_route(Method::Options, "/nested", plain, policy));
    EXPECT_TRUE(server_->add_route(Method::Get, "/nested", plain, policy));
}

/// 同路径共享等价策略，冲突拒绝后仍允许使用原策略重试新增方法。
TEST_F(ServerScenarios, CorsSharedPreflightRejectsConflictingPolicy) {
    const CorsPolicy policy({"https://front.example"},
        {Method::Get, Method::Post}, {"Authorization"});
    const CorsPolicy equivalent({"https://front.example"},
        {Method::Post, Method::Get}, {"authorization"});
    const CorsPolicy conflicting({"https://evil.example"},
        {Method::Get, Method::Post}, {"Authorization"});
    const HttpHandler plain = [](Request, Respond respond) {
        respond(Response{});
    };
    ASSERT_TRUE(server_->add_route(Method::Get, "/shared", plain, policy));
    EXPECT_FALSE(
        server_->add_route(Method::Post, "/shared", plain, conflicting));
    ASSERT_TRUE(server_->add_route(Method::Post, "/shared", plain, equivalent));
    EXPECT_FALSE(server_->add_route(Method::Options, "/shared", plain));
    start();
    EXPECT_EQ(200U,
        ask("/shared", Method::Post, "", {{"Origin", "https://front.example"}})
            .status);
    EXPECT_EQ(
        204U, ask("/shared", Method::Options, "",
                  {{"Origin", "https://front.example"},
                      {"Access-Control-Request-Method", "POST"},
                      {"Access-Control-Request-Headers", "authorization"}})
                  .status);
}
}  // namespace
}  // namespace web_test
