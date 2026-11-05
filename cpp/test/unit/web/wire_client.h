#pragma once

#include "mental1104/web/server.h"

namespace web_test {
/// 从真实网络读取的响应，独立于所有被测服务端 SDK。
struct WireResponse {
    unsigned status;
    std::string body;
    std::vector<mental1104::web::Header> headers;
    /// 查询响应头，不区分 ASCII 大小写。
    /// @param name 要读取的字段名。
    /// @return 首个字段值，缺失时为空字符串。
    std::string header(const std::string &name) const;
};

/// 通过独立客户端发起一次 HTTP/HTTPS 请求，每个网络操作最多等待三秒。
/// @param endpoint 服务实际监听地址。
/// @param method 请求方法。
/// @param path 含可选查询字符串的请求目标。
/// @param body 请求体副本。
/// @param headers 只读请求头。
/// @param tls 是否对本地测试证书使用 TLS；仅测试传输，不验证公共 CA。
/// @return 完整响应；网络或超时失败抛异常，由 GoogleTest 报告失败。
WireResponse request(const mental1104::web::Endpoint &endpoint,
    mental1104::web::Method method, const std::string &path,
    const std::string &body = {},
    const std::vector<mental1104::web::Header> &headers = {}, bool tls = false);

/// 测试拥有的 WebSocket 客户端；所有网络操作有截止时间，禁止跨线程并发调用。
class WireSocket {
public:
    /// 创建独立事件循环与连接状态，不建立网络连接。
    WireSocket();
    /// 释放连接，不执行可能阻塞的关闭握手。
    ~WireSocket();
    WireSocket(const WireSocket &) = delete;
    WireSocket &operator=(const WireSocket &) = delete;
    /// 建立 TCP 连接并完成 WebSocket 握手。
    /// @param endpoint 服务监听地址。
    /// @param path 握手路径。
    /// @param headers 附加鉴权等请求头。
    void connect(const mental1104::web::Endpoint &endpoint,
        const std::string &path,
        const std::vector<mental1104::web::Header> &headers = {});
    /// 发送一条完整消息，保留文本/二进制类型。
    /// @param message 只读消息，调用期间有效。
    void send(const mental1104::web::Message &message);
    /// 读取一条完整消息；超时或网络异常直接使测试失败。
    /// @return 收到的消息；正常关闭返回空。
    std::optional<mental1104::web::Message> receive();
    /// 直接断开 TCP，模拟客户端退出或网络丢失，不执行 WebSocket 关闭握手。
    void disconnect();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace web_test
