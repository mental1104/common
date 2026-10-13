#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace mental1104::network {

/// mDNS 服务发布和发现使用的服务描述。
struct MdnsServiceInfo {
    /// DNS-SD service type，例如 "_file-manager._tcp"。
    std::string service_type;

    /// 服务实例名称。
    std::string instance_name;

    /// 业务唯一标识，例如设备 SN。
    std::string identity_key;

    /// 服务监听端口。
    uint16_t port = 0;

    /// 额外 TXT 元数据。
    std::map<std::string, std::string> txt_records;
};

/// mDNS 服务访问端点。
struct MdnsEndpoint {
    /// 解析后的 IP 地址。
    std::string address;

    /// 服务端口。
    uint16_t port = 0;

    /// 服务 TXT 元数据。
    std::map<std::string, std::string> txt_records;
};

/// 跨平台 mDNS 服务管理接口。
///
/// 底层实现通过私有 backend 隔离平台差异，避免调用方依赖 Avahi 或 Bonjour。
class MdnsClient {
public:
    MdnsClient();
    ~MdnsClient();

    MdnsClient(MdnsClient&&) noexcept;
    MdnsClient& operator=(MdnsClient&&) noexcept;

    MdnsClient(const MdnsClient&) = delete;
    MdnsClient& operator=(const MdnsClient&) = delete;

    /// 发布一个局域网 DNS-SD 服务。
    bool Publish(const MdnsServiceInfo& service);

    /// 根据 service type 和 identity key 查找服务。
    std::optional<MdnsEndpoint> FindByKey(
        const std::string& service_type,
        const std::string& identity_key);

    /// 停止当前发布的服务。
    void Stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mental1104::network
