#pragma once

#include "mental1104/meta/compiler_support.h"
#if M1104_HAS_CXX17
#include <optional>
#else
#include <boost/optional.hpp>
#endif

namespace mental1104 {
/// 内置后端标识；Custom 用于应用自行实现的解析器，不参与全局注册。
enum class JsonParser { CJSON, RapidJSON, Custom };
/// 节点逻辑类型；Missing 表示查找失败，与有效的 JSON null 区分。
enum class JsonType { Missing, Null, Bool, Number, String, Array, Object };
/// 可选读取结果；C++17 起使用标准 optional，旧标准使用 Boost.Optional。
/// 空结果表示缺失或转换失败，合法的 false、0、空字符串仍包含值。
#if M1104_HAS_CXX17
template <typename T>
using JsonOptional = std::optional<T>;
#else
template <typename T>
using JsonOptional = boost::optional<T>;
#endif
}  // namespace mental1104
