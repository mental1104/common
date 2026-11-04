#pragma once

#include <memory>

#include "mental1104/json/value.h"

namespace mental1104 {
/// 后端扩展边界：拥有只读 DOM，解释节点句柄并实现基础读取。
/// 派生文档地址固定；存在借用视图时不得修改或销毁 DOM。
class IJsonDocument {
public:
    /// 通过基类释放具体后端资源。
    virtual ~IJsonDocument();
    IJsonDocument(const IJsonDocument &) = delete;
    IJsonDocument &operator=(const IJsonDocument &) = delete;
    /// 建立绑定当前文档的根视图。
    /// @return 不延长文档生命周期的根节点视图。
    JsonValueView root() const noexcept;

protected:
    /// 句柄只在所属派生文档解释，nullptr 始终表示 Missing。
    using Node = const void *;
    /// 初始化扩展基类，不创建原生 DOM。
    IJsonDocument() = default;
    /// 返回文档拥有的根节点。
    /// @return 借用句柄，空文档返回 nullptr。
    virtual Node do_root() const noexcept = 0;
    /// 查询节点逻辑类型。
    /// @param node 当前文档的非空借用句柄。
    /// @return 后端无关的类型。
    virtual JsonType do_type(Node node) const noexcept = 0;
    /// 查询对象成员，非对象返回 nullptr。
    /// @param node 当前文档的非空句柄。
    /// @param key 区分大小写的只读键，不保存引用。
    /// @return 借用句柄；缺失为空。
    virtual Node do_get(Node node, const string_view &key) const = 0;
    /// 查询数组长度。
    /// @param node 当前文档的非空句柄。
    /// @return 数组长度，非数组返回 0。
    virtual std::size_t do_size(Node node) const noexcept = 0;
    /// 查询数组元素。
    /// @param node 当前文档的非空句柄。
    /// @param index 从 0 开始的下标。
    /// @return 借用句柄；非数组或越界为空。
    virtual Node do_at(Node node, std::size_t index) const noexcept = 0;
    /// 读取字符串，旧标准的 string_view 可能分配内存。
    /// @param node 当前文档的非空句柄。
    /// @return 字符串或空结果。
    virtual JsonOptional<string_view> do_string(Node node) const = 0;
    /// 严格读取布尔值。
    /// @param node 当前文档的非空句柄。
    /// @return 布尔值或空结果。
    virtual JsonOptional<bool> do_bool(Node node) const = 0;
    /// 严格读取整数，拒绝小数和越界值。
    /// @param node 当前文档的非空句柄。
    /// @return 有符号整数或空结果。
    virtual JsonOptional<long long> do_i64(Node node) const = 0;
    /// 严格读取非负整数，拒绝小数和越界值。
    /// @param node 当前文档的非空句柄。
    /// @return 无符号整数或空结果。
    virtual JsonOptional<unsigned long long> do_u64(Node node) const = 0;
    /// 读取数值的 double 表示，允许整数到浮点的舍入。
    /// @param node 当前文档的非空句柄。
    /// @return 数值或空结果。
    virtual JsonOptional<double> do_f64(Node node) const = 0;
    friend class JsonValueView;
};

/// 可移动的独占文档；移动拥有指针不会改变后端地址，已有视图仍有效。
class JsonDoc {
public:
    /// 创建空文档。
    JsonDoc() noexcept;
    /// 接管后端文档，支持调用方提供派生实现。
    /// @param document 转入独占所有权，允许 nullptr。
    /// @param backend 诊断标识，自定义实现默认为 Custom。
    explicit JsonDoc(std::unique_ptr<IJsonDocument> document,
        JsonParser backend = JsonParser::Custom) noexcept;
    /// 释放 DOM；当前文档的所有借用视图失效。
    ~JsonDoc();
    /// 接管源文档，源对象变空。
    /// @param other 转移所有权的源文档。
    JsonDoc(JsonDoc &&other) noexcept;
    /// 释放旧文档再接管源文档，旧视图失效。
    /// @param other 转移所有权的源文档。
    /// @return 当前文档引用。
    JsonDoc &operator=(JsonDoc &&other) noexcept;
    JsonDoc(const JsonDoc &) = delete;
    JsonDoc &operator=(const JsonDoc &) = delete;
    /// 判断是否持有后端文档。
    /// @return 非空时为 true。
    bool valid() const noexcept;
    /// 获取诊断用后端标识。
    /// @return 构造时提供的标识。
    JsonParser backend() const noexcept;
    /// 借用根节点。
    /// @return 空文档返回空视图，不延长文档生命周期。
    JsonValueView root() const noexcept;

private:
    std::unique_ptr<IJsonDocument> document_;
    JsonParser backend_{JsonParser::Custom};
};
}  // namespace mental1104
