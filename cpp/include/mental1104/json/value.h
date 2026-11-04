#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>

#include "mental1104/json/types.h"

namespace mental1104 {
class IJsonDocument;
/// 调用方可特化的转换入口；自定义类型组合公共读取接口，不依赖原生 JSON。
template <typename T, typename Enable = void>
struct JsonValueReader;

/// 不拥有资源的只读节点视图；所属文档销毁后不得再使用视图或借用字符串。
/// 复制和遍历视图不会复制 JSON 树，节点句柄仅由所属文档解释。
class JsonValueView {
public:
    /// 构造表示 Missing 的空视图。
    JsonValueView() noexcept = default;
    /// 查询逻辑类型。
    /// @return 缺失返回 Missing，JSON null 返回 Null。
    JsonType type() const noexcept;
    /// 判断节点是否存在，JSON null 也有效。
    /// @return 存在时为 true。
    bool is_valid() const noexcept;
    /// 判断是否为 JSON null。
    /// @return 类型匹配时为 true。
    bool is_null() const noexcept;
    /// 判断是否为对象。
    /// @return 类型匹配时为 true。
    bool is_object() const noexcept;
    /// 判断是否为数组。
    /// @return 类型匹配时为 true。
    bool is_array() const noexcept;
    /// 判断是否为字符串。
    /// @return 类型匹配时为 true。
    bool is_string() const noexcept;
    /// 判断是否为布尔值。
    /// @return 类型匹配时为 true。
    bool is_bool() const noexcept;
    /// 判断是否为数值。
    /// @return 类型匹配时为 true。
    bool is_number() const noexcept;

    /// 按区分大小写的键查询成员。
    /// @param key 只读键，不保存其引用。
    /// @return 借用视图；非对象或缺失返回空视图。
    JsonValueView get(const string_view &key) const;
    /// 沿对象键路径定位节点，中途缺失或类型不符即停止。
    /// @param path 有序只读键；空路径表示当前节点。
    /// @return 最终借用视图；失败为空。
    JsonValueView get(std::initializer_list<string_view> path) const;
    /// 判断成员是否存在，成员值为 null 也返回 true。
    /// @param key 只读键，不保存其引用。
    /// @return 成员存在时为 true。
    bool has(const string_view &key) const;
    /// 查询数组元素数量，保留原接口的数组专用语义。
    /// @return 数组长度；非数组返回 0。
    std::size_t size() const noexcept;
    /// 查询数组元素。
    /// @param index 从 0 开始的下标。
    /// @return 借用视图；非数组或越界为空。
    JsonValueView at(std::size_t index) const noexcept;

    /// 严格读取当前节点，不在字符串、布尔与数值之间隐式转换。
    /// @tparam T 由 JsonValueReader 支持的类型。
    /// @return 转换值；缺失、null、类型不符或越界时为空。
    template <typename T>
    JsonOptional<T> as_optional() const;
    /// 查询一个成员并转换。
    /// @tparam T 目标类型。
    /// @param key 只读成员键。
    /// @return 转换结果；查询或转换失败时为空。
    template <typename T>
    JsonOptional<T> get_optional(const string_view &key) const;
    /// 沿路径遍历视图，最终节点只转换一次，不复制 JSON 树。
    /// @tparam T 目标类型。
    /// @param path 有序对象键；空路径读取当前节点。
    /// @return 转换结果；任一步失败时为空。
    template <typename T>
    JsonOptional<T> get_optional(std::initializer_list<string_view> path) const;

    /// 读取字符串；旧标准的 string_view 别名会复制。
    /// @param def 类型不符时的默认值，借用时存储必须保持存活。
    /// @return 节点字符串或默认值，不延长存储生命周期。
    string_view as_string(const string_view &def = {}) const;
    /// 读取独立字符串副本。
    /// @param def 类型不符时复制的默认值。
    /// @return 不依赖文档生命周期的字符串。
    std::string as_string_copy(const std::string &def = {}) const;
    /// 读取布尔值。
    /// @param def 类型不符时的默认值。
    /// @return 节点值或默认值。
    bool as_bool(bool def = false) const;
    /// 保留旧接口对小数向零截断的行为，增加范围检查。
    /// @param def 类型不符或越界时的默认值。
    /// @return 转换值或默认值；严格读取使用 as_optional。
    long long as_i64(long long def = 0) const;
    /// 读取浮点数。
    /// @param def 类型不符时的默认值。
    /// @return 数值或默认值。
    double as_f64(double def = 0.0) const;

private:
    /// 绑定文档与节点，调用方不能独立注入原生节点。
    /// @param owner 借用的所属文档，必须比视图活得更久。
    /// @param node 仅由 owner 解释的句柄，nullptr 表示缺失。
    JsonValueView(const IJsonDocument *owner, const void *node) noexcept;
    /// 读取原始字符串，旧标准可能分配内存。
    /// @return 类型不符为空。
    JsonOptional<string_view> read_string() const;
    /// 严格读取布尔值。
    /// @return 类型不符为空。
    JsonOptional<bool> read_bool() const;
    /// 严格读取有符号整数。
    /// @return 小数、类型不符或越界时为空。
    JsonOptional<long long> read_i64() const;
    /// 严格读取无符号整数。
    /// @return 负数、小数、类型不符或越界时为空。
    JsonOptional<unsigned long long> read_u64() const;
    /// 读取数值的 double 表示。
    /// @return 非数值时为空。
    JsonOptional<double> read_f64() const;

    const IJsonDocument *owner_{nullptr};
    const void *node_{nullptr};
    friend class IJsonDocument;
    template <typename T, typename Enable>
    friend struct JsonValueReader;
};
}  // namespace mental1104

#include "mental1104/json/value_reader.h"
