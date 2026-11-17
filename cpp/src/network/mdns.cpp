#include "mental1104/network/mdns.hpp"

namespace mental1104::network {

class MdnsClient::Impl {
public:
    /// 当前实现预留给平台 backend。
    ///
    /// 后续根据编译平台注入 Avahi 或 Bonjour 实现，公共 API 不感知具体库。
};

MdnsClient::MdnsClient() : impl_(std::make_unique<Impl>()) {}

MdnsClient::~MdnsClient() = default;

MdnsClient::MdnsClient(MdnsClient&&) noexcept = default;

MdnsClient& MdnsClient::operator=(MdnsClient&&) noexcept = default;

bool MdnsClient::Publish(const MdnsServiceInfo& service) {
    // 当前阶段只完成跨平台生命周期封装。
    // 具体 mDNS 注册逻辑由 backend 实现后接入。
    return !service.service_type.empty() &&
           !service.identity_key.empty() &&
           service.port != 0;
}

std::optional<MdnsEndpoint> MdnsClient::FindByKey(
    const std::string& service_type,
    const std::string& identity_key) {
    // 当前阶段保留统一查询接口，等待 backend 接入。
    if (service_type.empty() || identity_key.empty()) {
        return std::nullopt;
    }

    return std::nullopt;
}

void MdnsClient::Stop() {}

}  // namespace mental1104::network
