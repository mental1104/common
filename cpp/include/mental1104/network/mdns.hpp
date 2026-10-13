#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mental1104::network {

/// mDNS 服务发布和发现使用的服务描述。
///
/// 该结构只描述 DNS-SD 层面的信息，不包含具体网络连接逻辑。
/// identity_key 会被编码到 TXT record 中，用于在多个同类型服务中定位目标设备。
struct MdnsServiceInfo {
    /// DNS-SD service type，例如 "_file-manager._tcp"。
    std::string service_type;

    /// 服务实例名称，用于区分同一网络中的多个实例。
    std::string instance_name;

    /// 业务唯一标识，例如设备 SN 或机器人 ID。
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
/// 当前接口定义统一抽象，具体平台通过 Bonjour 或 Avahi backend 实现。
/// 调用方不需要感知底层 mDNS 实现差异。
class MdnsClient {
public:
    MdnsClient() = default;
    ~MdnsClient() = default;

    /// 发布一个局域网 DNS-SD 服务。
    ///
    /// @param service 服务描述，identity_key 必须唯一。
    /// @return 发布成功返回 true。
    bool Publish(const MdnsServiceInfo& service);

    /// 根据 service type 和唯一 key 查找服务。
    ///
    /// @param service_type DNS-SD 服务类型。
    /// @param identity_key TXT 中定义的唯一身份。
    /// @return 找到时返回 endpoint，否则返回空。
    std::optional<MdnsEndpoint> FindByKey(
        const std::string& service_type,
        const std::string& identity_key);

    /// 停止当前发布的服务。
    void Stop();
};

}  // namespace mental1104::network
