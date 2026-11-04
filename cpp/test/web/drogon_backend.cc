#include <drogon/HttpAppFramework.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>

#include "backend.h"
#include "mental1104/web/drogon_server.h"
#include "wire_client.h"

namespace web_test {
/// 只有此翻译单元依赖被测 SDK；公共场景不认识 Drogon。
/// @return 工厂与实际 TLS 能力。
const Backend &backend() {
    static const Backend value{"drogon", mental1104::web::make_drogon_server,
        drogon::app().supportSSL()};
    return value;
}

/// 单进程运行次数限制是 Drogon 专属行为，不强加给未来的其他实现。
TEST(DrogonOnly, SecondRuntimeIsRejected) {
    using namespace mental1104::web;
    std::unique_ptr<Server> first, second;
    ASSERT_TRUE(backend().create(ServerOptions{}, first));
    ASSERT_TRUE(backend().create(ServerOptions{}, second));
    Endpoint endpoint{};
    ASSERT_TRUE(first->start(endpoint));
    EXPECT_FALSE(second->start(endpoint));
    EXPECT_TRUE(first->request_stop());
    EXPECT_TRUE(first->wait());
}

/// 保留 OPTIONS 路由在 SDK 配置前失败，保持输出地址及后续实例的运行机会。
TEST(DrogonOnly, ReservedOptionsRouteRejectsStartupWithoutClaimingRuntime) {
    using namespace mental1104::web;
    std::unique_ptr<Server> rejected;
    ASSERT_TRUE(backend().create(ServerOptions{}, rejected));
    ASSERT_TRUE(
        rejected->add_route(Method::Options, "/*", [](Request, Respond) {
            ADD_FAILURE() << "Reserved OPTIONS handler must not run";
        }));
    // 注册只校验公共约束；后端配置失败须在启动时报告，不能交付假监听地址。
    Endpoint endpoint{"unchanged", 4321};
    ASSERT_FALSE(rejected->start(endpoint));
    EXPECT_EQ("unchanged", endpoint.address);
    EXPECT_EQ(4321, endpoint.port);
    EXPECT_FALSE(rejected->wait());

    // 被拒绝的配置不能污染 SDK；普通路径仍可正常分派显式 OPTIONS。
    std::unique_ptr<Server> valid;
    ASSERT_TRUE(backend().create(ServerOptions{}, valid));
    ASSERT_TRUE(valid->add_route(
        Method::Options, "/download", [](Request, Respond respond) {
            Response response;
            response.body = "explicit options";
            respond(std::move(response));
        }));
    ASSERT_TRUE(valid->start(endpoint));
    const auto response = request(endpoint, Method::Options, "/download");
    EXPECT_EQ(200U, response.status);
    EXPECT_EQ("explicit options", response.body);
    EXPECT_TRUE(valid->request_stop());
    EXPECT_TRUE(valid->wait());
}

/// CORS 自动生成的 OPTIONS /* 同样触发后端限制，不能绕过显式路由检查。
TEST(DrogonOnly, ReservedCorsPreflightRejectsStartup) {
    using namespace mental1104::web;
    std::unique_ptr<Server> server;
    ASSERT_TRUE(backend().create(ServerOptions{}, server));
    const CorsPolicy policy(
        {"https://front.example"}, {Method::Get}, {"Authorization"});
    ASSERT_TRUE(server->add_route(
        Method::Get, "/*",
        [](Request, Respond) {
            ADD_FAILURE() << "Rejected CORS handler must not run";
        },
        policy));
    Endpoint endpoint{"unchanged", 4321};
    ASSERT_FALSE(server->start(endpoint));
    EXPECT_EQ("unchanged", endpoint.address);
    EXPECT_EQ(4321, endpoint.port);
    EXPECT_FALSE(server->wait());
}

/// 星号属于字面路径的一部分；仅注册 GET 时无需 OPTIONS，仍可正常启动和访问。
TEST(DrogonOnly, ReservedOptionsPathStillAllowsOrdinaryGet) {
    using namespace mental1104::web;
    std::unique_ptr<Server> server;
    ASSERT_TRUE(backend().create(ServerOptions{}, server));
    ASSERT_TRUE(
        server->add_route(Method::Get, "/*", [](Request, Respond respond) {
            Response response;
            response.body = "literal star";
            respond(std::move(response));
        }));
    Endpoint endpoint{};
    ASSERT_TRUE(server->start(endpoint));
    const auto response = request(endpoint, Method::Get, "/*");
    EXPECT_EQ(200U, response.status);
    EXPECT_EQ("literal star", response.body);
    EXPECT_TRUE(server->request_stop());
    EXPECT_TRUE(server->wait());
}

/// 人为延迟 Linux 监听线程，确认 start 不会把 BeginningAdvice 当成监听就绪。
TEST(DrogonOnly, StartWaitsUntilListenerAcceptsConnections) {
#ifdef __linux__
    using namespace mental1104::web;
    using namespace std::chrono_literals;
    auto entered = std::make_shared<std::promise<void>>();
    auto blocked = entered->get_future();
    auto release = std::make_shared<std::promise<void>>();
    const auto gate = release->get_future().share();
    // 本 advice 先于适配器注册；阻塞任务排在 SDK 的 listen 任务之前。
    drogon::app().registerBeginningAdvice([entered, gate] {
        drogon::app().getIOLoop(0)->queueInLoop([entered, gate] {
            entered->set_value();
            gate.wait();
        });
    });
    std::unique_ptr<Server> server;
    ASSERT_TRUE(backend().create(ServerOptions{}, server));
    Endpoint endpoint{};
    auto started =
        std::async(std::launch::async, [&] { return server->start(endpoint); });
    EXPECT_EQ(std::future_status::ready, blocked.wait_for(3s));
    const auto premature = started.wait_for(100ms);
    // 无论断言结果如何均释放门闩，避免失败测试把后端线程永久阻塞。
    release->set_value();
    EXPECT_EQ(std::future_status::timeout, premature);
    EXPECT_TRUE(started.get());
    EXPECT_NE(0, endpoint.port);
    EXPECT_TRUE(server->request_stop());
    EXPECT_TRUE(server->wait());
#else
    GTEST_SKIP() << "Listener scheduling gate is specific to Linux";
#endif
}
}  // namespace web_test
