#pragma once

/// @file
/// 消费端私有模型与读取器特化，证明开放模板无需库内预先实例化或原生 SDK 类型。

#include "mental1104/json/access.h"

namespace json_consumer {
/// 仅由调用方定义的模型，库无法为它预先实例化模板。
struct Settings {
    /// 调用方独占的标识副本，不借用 JSON 文档中的字符串存储。
    std::string uuid;
};
/// 在独立翻译单元调用文档和类型化访问，亦用于安装后消费验证。
/// @return 所有已启用后端、自定义模型、文档移动及编解码往返均通过时为 true。
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
        // 路径失败时不构造不完整模型；成功的空字符串仍是实际读取结果。
        if (!uuid) return {};
        return json_consumer::Settings{*uuid};
    }
};
}  // namespace mental1104
