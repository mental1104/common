#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

#include "mental1104/json/value.h"

namespace mental1104 {
/// 布尔读取只接受 JSON 布尔，不转换数值 0/1。
template <>
struct JsonValueReader<bool> {
    /// 读取布尔节点。
    /// @param value 只读借用视图。
    /// @return 布尔值或空结果。
    static JsonOptional<bool> read(const JsonValueView &value) {
        return value.read_bool();
    }
};

/// 有符号整数读取保留目标类型，收窄前检查范围。
template <typename T>
struct JsonValueReader<T,
    typename std::enable_if<std::is_integral<T>::value &&
                            std::is_signed<T>::value>::type> {
    /// 读取整数，拒绝小数及目标类型越界。
    /// @param value 只读借用视图。
    /// @return 转换后的整数或空结果。
    static JsonOptional<T> read(const JsonValueView &value) {
        const auto number = value.read_i64();
        if (!number || *number < (std::numeric_limits<T>::min)() ||
            *number > (std::numeric_limits<T>::max)()) {
            return {};
        }
        return static_cast<T>(*number);
    }
};

/// 无符号整数读取拒绝负数，并检查目标类型上界。
template <typename T>
struct JsonValueReader<T,
    typename std::enable_if<std::is_integral<T>::value &&
                            std::is_unsigned<T>::value &&
                            !std::is_same<T, bool>::value>::type> {
    /// 读取非负整数。
    /// @param value 只读借用视图。
    /// @return 转换后的整数或空结果。
    static JsonOptional<T> read(const JsonValueView &value) {
        const auto number = value.read_u64();
        if (!number || *number > (std::numeric_limits<T>::max)()) {
            return {};
        }
        return static_cast<T>(*number);
    }
};

/// 浮点读取允许普通浮点舍入，拒绝非有限值及目标类型溢出。
template <typename T>
struct JsonValueReader<T,
    typename std::enable_if<std::is_floating_point<T>::value>::type> {
    /// 从后端 double 表示转换到目标类型。
    /// @param value 只读借用视图。
    /// @return 有限浮点值或空结果，不承诺整数转浮点无精度损失。
    static JsonOptional<T> read(const JsonValueView &value) {
        const auto number = value.read_f64();
        if (!number || !std::isfinite(*number) ||
            static_cast<long double>(*number) >
                (std::numeric_limits<T>::max)() ||
            static_cast<long double>(*number) <
                -(std::numeric_limits<T>::max)()) {
            return {};
        }
        return static_cast<T>(*number);
    }
};

/// 字符串读取返回独立副本，结果可以比文档活得更久。
template <>
struct JsonValueReader<std::string> {
    /// 复制字符串节点。
    /// @param value 只读借用视图。
    /// @return 副本或空结果；分配失败仍抛异常。
    static JsonOptional<std::string> read(const JsonValueView &value) {
        const auto text = value.read_string();
        if (!text) {
            return {};
        }
        return std::string(text->data(), text->size());
    }
};
#if M1104_HAS_CXX17
/// 标准 string_view 借用文档存储，不延长文档生命周期。
template <>
struct JsonValueReader<std::string_view> {
    /// 借用字符串节点。
    /// @param value 只读借用视图。
    /// @return 字符串视图或空结果。
    static JsonOptional<std::string_view> read(const JsonValueView &value) {
        return value.read_string();
    }
};
#endif

template <typename T>
JsonOptional<T> JsonValueView::as_optional() const {
    return JsonValueReader<T>::read(*this);
}
template <typename T>
JsonOptional<T> JsonValueView::get_optional(const string_view &key) const {
    return this->get(key).template as_optional<T>();
}
template <typename T>
JsonOptional<T> JsonValueView::get_optional(
    std::initializer_list<string_view> path) const {
    return this->get(path).template as_optional<T>();
}
}  // namespace mental1104
