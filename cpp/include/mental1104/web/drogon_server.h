#pragma once

#include "mental1104/web/server.h"

namespace mental1104 {
namespace web {

/// 创建 Drogon 适配器；只有应用装配代码需要包含此头，不暴露任何 Drogon 类型。
/// @param options 服务配置，复制保存，不修改调用者对象。
/// @param server 仅成功时写入独占服务；输入必须为空，失败保持原值。
/// @return 创建成功为 true；非法配置、非空输出或内部异常为
/// false，不向外抛异常。 Drogon 运行时为进程单例：每进程仅允许一个实例成功进入
/// start，不支持重启， 不能与直接操作 drogon::app() 的代码混用；绑定失败等致命
/// SDK 错误遵循 Drogon 行为。
/// OPTIONS /* 是 SDK 保留请求；注册该路由或为 /* 启用 CORS 后，start 返回
/// false，且不占用进程运行机会。普通 GET /* 不受此限制。
bool make_drogon_server(
    const ServerOptions &options, std::unique_ptr<Server> &server) noexcept;

}  // namespace web
}  // namespace mental1104
