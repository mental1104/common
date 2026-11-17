#include "wire_client.h"

#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <stdexcept>

namespace web_test {
namespace {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
using tcp = asio::ip::tcp;
using namespace mental1104::web;

/// 运行一个异步网络操作，截止时间到达时关闭底层套接字，回调全部完成后才返回。
/// @tparam Start 接受完成回调的异步操作发起函数。
/// @param io 客户端拥有的事件循环。
/// @param socket 要取消的底层套接字。
/// @param start 发起当前操作，不保存其借用。
/// @return 操作结果；超时作为明确错误返回，不依赖 CTest 杀进程。
template <class Start>
beast::error_code run(asio::io_context &io, tcp::socket &socket, Start start) {
    io.restart();
    asio::steady_timer timer(io, std::chrono::seconds(3));
    beast::error_code result = asio::error::would_block;
    bool timed_out = false;
    timer.async_wait([&](beast::error_code error) {
        if (error) return;
        timed_out = true;
        beast::error_code ignored;
        socket.close(ignored);
    });
    try {
        start([&](beast::error_code error, auto &&...) {
            result = error;
            timer.cancel();
        });
    } catch (...) {
        timer.cancel();
        io.run();
        throw;
    }
    io.run();
    return timed_out ? beast::error_code(asio::error::timed_out) : result;
}

/// 将网络错误转成带操作上下文的测试异常。
/// @param error 当前操作结果。
/// @param operation 操作名称，仅用于失败诊断。
void checked(beast::error_code error, const char *operation) {
    if (error)
        throw std::runtime_error(
            std::string(operation) + ": " + error.message());
}

/// 将公共请求方法映射到独立客户端的协议类型。
/// @param method 公共方法。
/// @return 客户端方法；非法枚举使测试失败。
http::verb verb(Method method) {
    switch (method) {
        case Method::Get:
            return http::verb::get;
        case Method::Head:
            return http::verb::head;
        case Method::Post:
            return http::verb::post;
        case Method::Put:
            return http::verb::put;
        case Method::Delete:
            return http::verb::delete_;
        case Method::Patch:
            return http::verb::patch;
        case Method::Options:
            return http::verb::options;
    }
    throw std::invalid_argument("invalid client method");
}

/// 使用同一 HTTP 编解码过程测试明文流或 TLS 流。
/// @tparam Stream 独立客户端的网络流类型。
/// @param io 客户端事件循环。
/// @param stream 已连接且完成可选 TLS 握手的流。
/// @param message 要发送的请求。
/// @return 从网络完整解析的响应。
template <class Stream>
WireResponse exchange(asio::io_context &io, Stream &stream,
    http::request<http::string_body> &message) {
    auto &socket = beast::get_lowest_layer(stream).socket();
    checked(run(io, socket,
                [&](auto done) { http::async_write(stream, message, done); }),
        "HTTP write");
    beast::flat_buffer buffer;
    http::response_parser<http::string_body> parser;
    parser.body_limit(32 * 1024 * 1024);
    parser.skip(message.method() == http::verb::head);
    checked(
        run(io, socket,
            [&](auto done) { http::async_read(stream, buffer, parser, done); }),
        "HTTP read");
    auto received = parser.release();
    WireResponse result{received.result_int(), std::move(received.body()), {}};
    for (const auto &field : received.base())
        result.headers.push_back(
            {std::string(field.name_string()), std::string(field.value())});
    return result;
}
}  // namespace

std::string WireResponse::header(const std::string &name) const {
    for (const auto &field : this->headers) {
        if (beast::iequals(field.name, name)) return field.value;
    }
    return {};
}

WireResponse request(const Endpoint &endpoint, Method method,
    const std::string &path, const std::string &body,
    const std::vector<Header> &headers, bool tls) {
    asio::io_context io;
    const tcp::endpoint address(
        asio::ip::make_address(endpoint.address), endpoint.port);
    http::request<http::string_body> message(verb(method), path, 11);
    message.set(http::field::host,
        endpoint.address + ":" + std::to_string(endpoint.port));
    message.keep_alive(false);
    message.body() = body;
    message.prepare_payload();
    for (const auto &field : headers) message.set(field.name, field.value);
    if (tls) {
        asio::ssl::context context(asio::ssl::context::tls_client);
        context.set_verify_mode(asio::ssl::verify_none);
        beast::ssl_stream<beast::tcp_stream> stream(io, context);
        auto &socket = beast::get_lowest_layer(stream).socket();
        checked(run(io, socket,
                    [&](auto done) { socket.async_connect(address, done); }),
            "TCP connect");
        checked(run(io, socket,
                    [&](auto done) {
                        stream.async_handshake(
                            asio::ssl::stream_base::client, done);
                    }),
            "TLS handshake");
        return exchange(io, stream, message);
    }
    beast::tcp_stream stream(io);
    checked(
        run(io, stream.socket(),
            [&](auto done) { stream.socket().async_connect(address, done); }),
        "TCP connect");
    return exchange(io, stream, message);
}

/// 隐藏 Beast/Asio 类型，场景源码只看到公共消息和端口。
class WireSocket::Impl {
public:
    asio::io_context io;
    ws::stream<beast::tcp_stream> stream{io};
};
WireSocket::WireSocket() : impl_(std::make_unique<Impl>()) {}
WireSocket::~WireSocket() = default;

void WireSocket::connect(const Endpoint &endpoint, const std::string &path,
    const std::vector<Header> &headers) {
    auto &stream = this->impl_->stream;
    auto &socket = beast::get_lowest_layer(stream).socket();
    const tcp::endpoint address(
        asio::ip::make_address(endpoint.address), endpoint.port);
    checked(run(this->impl_->io, socket,
                [&](auto done) { socket.async_connect(address, done); }),
        "WebSocket connect");
    stream.set_option(
        ws::stream_base::decorator([headers](ws::request_type &message) {
            for (const auto &field : headers)
                message.set(field.name, field.value);
        }));
    checked(run(this->impl_->io, socket,
                [&](auto done) {
                    stream.async_handshake(
                        endpoint.address + ":" + std::to_string(endpoint.port),
                        path, done);
                }),
        "WebSocket handshake");
}

void WireSocket::send(const Message &message) {
    auto &stream = this->impl_->stream;
    stream.text(message.type == MessageType::Text);
    checked(run(this->impl_->io, beast::get_lowest_layer(stream).socket(),
                [&](auto done) {
                    stream.async_write(asio::buffer(message.data), done);
                }),
        "WebSocket write");
}

std::optional<Message> WireSocket::receive() {
    auto &stream = this->impl_->stream;
    beast::flat_buffer buffer;
    const auto error =
        run(this->impl_->io, beast::get_lowest_layer(stream).socket(),
            [&](auto done) { stream.async_read(buffer, done); });
    if (error == ws::error::closed) return std::nullopt;
    checked(error, "WebSocket read");
    return Message{stream.got_text() ? MessageType::Text : MessageType::Binary,
        beast::buffers_to_string(buffer.data())};
}

void WireSocket::disconnect() {
    beast::error_code error;
    beast::get_lowest_layer(this->impl_->stream).socket().close(error);
    checked(error, "TCP disconnect");
}
}  // namespace web_test
