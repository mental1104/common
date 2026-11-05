#pragma once

/// @file
/// JSON 对象访问：逻辑类型、文档所有权、节点视图及调用方类型转换。
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

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

class IJsonDocument;
class JsonDoc;
struct DumpResult;
/// 调用方可特化的转换入口；自定义类型组合公共读取接口，不依赖原生 JSON。
/// @tparam T 调用方需要的具体值类型，成功结果保留该静态类型。
/// @tparam Enable 内置 traits 特化的选择条件，普通调用方沿用默认 void。
/// 预期的类型错误返回空结果；自定义转换及内存分配异常不在统一层吞掉。
template <typename T, typename Enable = void>
struct JsonValueReader;

/// 不拥有资源的只读节点视图；所属文档销毁后不得再使用视图或借用字符串。
/// 复制和遍历视图不会复制 JSON 树，节点句柄仅由所属文档解释。
class JsonValueView {
public:
    /// 数组范围遍历的借用游标，仅供 range-for；不延长所属文档生命周期。
    class ArrayIterator;
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

    /// 建立数组起始游标；非数组视为空范围，不遍历对象成员。
    /// @return 借用当前数组的起始游标，须在所属文档存活时使用。
    ArrayIterator begin() const noexcept;
    /// 建立数组结束游标，不能解引用。
    /// @return 数组末尾；非数组时与 begin() 相等。
    ArrayIterator end() const noexcept;

    /// 严格读取当前节点，不在字符串、布尔与数值之间隐式转换。
    /// @tparam T 由 JsonValueReader 支持的类型。
    /// @return 转换值；缺失、null、类型不符或越界时为空。
    template <typename T>
    JsonOptional<T> get_optional() const;
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
    /// @return 转换值或默认值；严格读取使用 get_optional。
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

    /// 只读借用的所属文档；该指针不参与引用计数或延长 DOM 生命周期。
    const IJsonDocument *owner_{nullptr};
    /// 只允许 owner_ 解释的原生句柄；nullptr 表示 Missing，不能跨文档混用。
    const void *node_{nullptr};
    friend class IJsonDocument;
    template <typename T, typename Enable>
    friend struct JsonValueReader;
};

/// 按索引借用数组元素的轻量范围游标，不复制数组或缓存元素。
/// 每步读取使用后端 at()，复杂度取决于后端；cJSON 的索引访问需要遍历链表。
class JsonValueView::ArrayIterator {
public:
    /// 取得当前元素的借用视图；调用方不得解引用结束游标。
    /// @return 当前元素，不延长文档生命周期。
    JsonValueView operator*() const noexcept;
    /// 前进到下一个数组元素；调用方不得递增结束游标。
    /// @return 当前游标引用。
    ArrayIterator &operator++() noexcept;
    /// 比较所属数组和位置，不访问原生 DOM。
    /// @param other 待比较的借用游标。
    /// @return 同一数组同一位置时为 true。
    bool operator==(const ArrayIterator &other) const noexcept;
    /// 判断游标是否仍未到达指定位置。
    /// @param other 待比较的借用游标，range-for 中为结束位置。
    /// @return 所属数组或位置不同时为 true。
    bool operator!=(const ArrayIterator &other) const noexcept;

private:
    /// 把数组视图与索引绑定，只有所属视图可以建立游标。
    /// @param array 借用数组；非数组只允许建立空范围。
    /// @param index 当前索引，数组长度表示结束位置。
    ArrayIterator(JsonValueView array, std::size_t index) noexcept;
    /// 借用所属数组，游标复制不会取得文档所有权。
    JsonValueView array_;
    /// 当前元素索引，必须在解引用前保证未到达末尾。
    std::size_t index_;
    friend class JsonValueView;
};

/// 后端扩展边界：拥有只读 DOM，解释节点句柄并实现基础读取。
/// 派生文档地址固定；存在借用视图时不得修改或销毁 DOM。
/// 统一视图负责拦截空句柄；do_* 接收的非空节点必须属于当前文档。
/// 后端仍需检查节点逻辑类型与索引范围，不能直接触发原生 SDK 断言。
class IJsonDocument {
public:
    /// 通过基类释放具体后端资源。
    virtual ~IJsonDocument();
    /// 禁止复制源文档的基类部分；文档地址和句柄所属关系必须保持唯一。
    /// 参数为只读源文档，本声明在编译期禁止调用。
    IJsonDocument(const IJsonDocument &) = delete;
    /// 禁止从只读源文档复制赋值，避免替换已被视图绑定的文档身份。
    /// 此操作不可调用，不产生赋值结果。
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
    /// 将当前只读文档交给编解码层序列化；默认实现返回不支持错误。
    /// @return 紧凑 JSON 文本或错误，不修改文档；完整结果定义在 codec.h。
    virtual DumpResult do_dump() const;
    friend class JsonValueView;
    friend DumpResult dump_json(const JsonDoc &document);
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
    /// 禁止复制只读源拥有者，资源转移必须在调用点通过移动显式表达。
    JsonDoc(const JsonDoc &) = delete;
    /// 禁止从只读源拥有者复制赋值；本声明不可调用，不产生赋值结果。
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
    /// 唯一资源拥有者；释放或替换该指针会使旧文档的所有借用视图失效。
    std::unique_ptr<IJsonDocument> document_;
    /// 独立保存的诊断标识，不能用于判断 document_ 是否有效或强制转换其类型。
    JsonParser backend_{JsonParser::Custom};
    friend DumpResult dump_json(const JsonDoc &document);
};

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
        // 收窄发生在范围检查之后，小数和原生数值越界已由后端严格读取拒绝。
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
        // 后端先检查负数与小数，这里继续检查具体无符号目标类型的上界。
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
        // 先拒绝非有限值与目标类型溢出；其余转换允许普通浮点舍入。
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
        // 空 optional 才是失败；即使字符串长度为 0，也必须返回成功副本。
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

/// 将当前节点交给具体类型的读取器，保持自定义模型的转换逻辑在调用方可见。
/// @tparam T 结果值类型，由对应 JsonValueReader<T> 决定转换规则与所有权。
/// @return 类型化的可选结果；读取器异常直接传播，不改写为字段缺失。
template <typename T>
JsonOptional<T> JsonValueView::get_optional() const {
    return JsonValueReader<T>::read(*this);
}
/// 先查询成员，再对最终节点执行一次目标类型转换。
/// @tparam T 读取器支持的结果类型，不要求原生 JSON 库认识该类型。
/// @param key 只读成员键，本次查询之后不保存键的存储。
/// @return 成员转换结果；缺失与类型错误为空，合法的 false/0/空字符串仍包含值。
template <typename T>
JsonOptional<T> JsonValueView::get_optional(const string_view &key) const {
    return this->get(key).template get_optional<T>();
}
/// 路径遍历只传递借用视图，避免为中间节点复制整棵 JSON 子树。
/// @tparam T 最终节点的结果类型，中间节点不提前转换为 T。
/// @param path 从外到内的对象键序列；空路径直接转换当前节点。
/// @return 最终转换结果；路径中断或类型不符时为空，异常仍按读取器契约传播。
template <typename T>
JsonOptional<T> JsonValueView::get_optional(
    std::initializer_list<string_view> path) const {
    return this->get(path).template get_optional<T>();
}
}  // namespace mental1104
