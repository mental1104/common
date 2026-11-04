#pragma once

#include "mental1104/json/value.h"

namespace json_consumer {
/// 仅由调用方定义的模型，库无法为它预先实例化模板。
struct Settings {
    std::string uuid;
};
/// 在独立翻译单元调用文档和类型化访问，亦用于安装后消费验证。
/// @return 两个已启用后端、自定义模型及文档移动均符合约定时为 true。
bool verify();
}  // namespace json_consumer

namespace mental1104 {
/// 调用方扩展点：仅组合公共视图 API，不接触任何后端类型。
template <>
struct JsonValueReader<json_consumer::Settings> {
    /// 读取嵌套的配置标识。
    /// @param value 只读根节点视图。
    /// @return 完整模型；路径或类型错误时为空。
    static JsonOptional<json_consumer::Settings> read(
        const JsonValueView &value) {
        const auto uuid =
            value.get_optional<std::string>({"status", "settings", "uuid"});
        if (!uuid) return {};
        return json_consumer::Settings{*uuid};
    }
};
}  // namespace mental1104
