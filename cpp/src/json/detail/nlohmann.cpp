/// @file
/// nlohmann JSON 后端：独占 DOM，适配只读节点访问和紧凑文本输出。
/// 原生异常在编解码边界转成统一错误，调用方不需要依赖第三方类型。
#include <algorithm>
#include <limits>
#include <utility>

#include "backend.h"
#include "mental1104/json/codec.h"
#if M1104_HAS_JSON_NLOHMANN
#include <nlohmann/json.hpp>
#endif

namespace mental1104 {
#if M1104_HAS_JSON_NLOHMANN
namespace {
/// 仅在后端内部使用的 DOM 类型；只读后保持节点地址稳定并保留对象成员顺序。
using NativeJson = nlohmann::ordered_json;

/// 固定地址的原生 DOM 拥有者，所有节点和字符串视图均借用其存储。
/// 不提供原生节点注入或修改入口，解析后不会插入元素或重新分配节点。
class NlohmannJsonDocument final : public IJsonDocument {
public:
    /// 接管已解析的原生 DOM，后续 JsonDoc 移动只转移外层拥有指针。
    /// @param document 转入的完整文档，不保留调用方输入的引用。
    explicit NlohmannJsonDocument(NativeJson &&document)
        : document_(std::move(document)) {}

private:
    /// 输出独立的紧凑文本，保留整数类型且不修改当前文档。
    /// @return 完整文本或序列化错误；内存分配失败仍向外传播。
    DumpResult do_dump() const override {
        try {
            DumpResult result;
            result.text = this->document_.dump();
            result.ok = true;
            return result;
        } catch (const NativeJson::exception &) {
            // 原生编码等错误统一返回诊断，不能把部分输出当作成功结果。
            return json_detail::dump_error(
                "nlohmann JSON serialization failed");
        }
    }
    /// 借用文档自身作为根节点，合法 null 也具有实际地址。
    /// @return 非空根节点句柄，不延长所属文档生命周期。
    Node do_root() const noexcept override { return &this->document_; }
    /// 将 JSON 文本可产生的节点类型映射到统一逻辑类型。
    /// @param node 属于当前文档的非空借用句柄。
    /// @return 逻辑类型；文本解析不会产生 binary 或 discarded 节点。
    JsonType do_type(Node node) const noexcept override {
        const auto &value = *static_cast<const NativeJson *>(node);
        if (value.is_null()) return JsonType::Null;
        if (value.is_boolean()) return JsonType::Bool;
        if (value.is_number()) return JsonType::Number;
        if (value.is_string()) return JsonType::String;
        if (value.is_array()) return JsonType::Array;
        if (value.is_object()) return JsonType::Object;
        return JsonType::Missing;
    }
    /// 在对象中单次查找完整键，不把缺失成员插入只读 DOM。
    /// @param node 当前文档的非空借用句柄。
    /// @param key 区分大小写的完整字节序列，允许空键和内嵌 NUL，不保存引用。
    /// @return 成员借用句柄；非对象或成员不存在时为空。
    Node do_get(Node node, const string_view &key) const override {
        const auto &value = *static_cast<const NativeJson *>(node);
        if (!value.is_object()) return nullptr;
        const auto found = value.find(key);
        return found == value.end() ? nullptr : &*found;
    }
    /// 仅查询数组长度，不沿用原生对象和标量的 size 语义。
    /// @param node 当前文档的非空借用句柄。
    /// @return 数组长度；非数组为 0。
    std::size_t do_size(Node node) const noexcept override {
        const auto &value = *static_cast<const NativeJson *>(node);
        const auto *array = value.get_ptr<const NativeJson::array_t *>();
        return array ? array->size() : 0;
    }
    /// 类型和边界检查通过后才读取数组，避免抛出原生越界异常。
    /// @param node 当前文档的非空借用句柄。
    /// @param index 从 0 开始的索引，不收窄 size_t。
    /// @return 元素借用句柄；非数组或越界时为空。
    Node do_at(Node node, std::size_t index) const noexcept override {
        const auto &value = *static_cast<const NativeJson *>(node);
        const auto *array = value.get_ptr<const NativeJson::array_t *>();
        return array && index < array->size() ? &(*array)[index] : nullptr;
    }
    /// 按长度读取原生字符串，不截断转义得到的 NUL 字节。
    /// @param node 当前文档的非空借用句柄。
    /// @return 字符串或空结果；C++17 起借用文档，旧标准复制字符串。
    JsonOptional<string_view> do_string(Node node) const override {
        const auto &value = *static_cast<const NativeJson *>(node);
        const auto *text = value.get_ptr<const NativeJson::string_t *>();
        if (!text) return {};
        return string_view(text->data(), text->size());
    }
    /// 严格读取布尔节点，不隐式转换数值或字符串。
    /// @param node 当前文档的非空借用句柄。
    /// @return true/false；类型不符时为空。
    JsonOptional<bool> do_bool(Node node) const override {
        const auto &value = *static_cast<const NativeJson *>(node);
        const auto *flag = value.get_ptr<const NativeJson::boolean_t *>();
        return flag ? JsonOptional<bool>(*flag) : JsonOptional<bool>{};
    }
    /// 优先读取原生整数，避免完整 64 位值经 double 舍入。
    /// @param node 当前文档的非空借用句柄。
    /// @return 有符号整数；类型不符、小数或越界时为空。
    JsonOptional<long long> do_i64(Node node) const override {
        const auto &value = *static_cast<const NativeJson *>(node);
        if (const auto *number =
                value.get_ptr<const NativeJson::number_integer_t *>()) {
            return static_cast<long long>(*number);
        }
        if (const auto *number =
                value.get_ptr<const NativeJson::number_unsigned_t *>()) {
            // 无符号整数只在有符号范围内才转换，不依赖原生 get 的隐式收窄。
            if (*number > static_cast<unsigned long long>(
                              (std::numeric_limits<long long>::max)()))
                return {};
            return static_cast<long long>(*number);
        }
        const auto *number =
            value.get_ptr<const NativeJson::number_float_t *>();
        return number ? json_detail::signed_integer(*number)
                      : JsonOptional<long long>{};
    }
    /// 保留原生非负整数精度，浮点转换复用公共符号和范围检查。
    /// @param node 当前文档的非空借用句柄。
    /// @return 无符号整数；负数、小数、越界或类型不符时为空。
    JsonOptional<unsigned long long> do_u64(Node node) const override {
        const auto &value = *static_cast<const NativeJson *>(node);
        if (const auto *number =
                value.get_ptr<const NativeJson::number_unsigned_t *>()) {
            return static_cast<unsigned long long>(*number);
        }
        if (const auto *number =
                value.get_ptr<const NativeJson::number_integer_t *>()) {
            if (*number < 0) return {};
            return static_cast<unsigned long long>(*number);
        }
        const auto *number =
            value.get_ptr<const NativeJson::number_float_t *>();
        return number ? json_detail::unsigned_integer(*number)
                      : JsonOptional<unsigned long long>{};
    }
    /// 读取数值的 double 表示，整数转换允许正常浮点舍入。
    /// @param node 当前文档的非空借用句柄。
    /// @return 数值或空结果，不把字符串或布尔解释成数字。
    JsonOptional<double> do_f64(Node node) const override {
        const auto &value = *static_cast<const NativeJson *>(node);
        if (const auto *number =
                value.get_ptr<const NativeJson::number_float_t *>()) {
            return *number;
        }
        if (const auto *number =
                value.get_ptr<const NativeJson::number_integer_t *>()) {
            return static_cast<double>(*number);
        }
        const auto *number =
            value.get_ptr<const NativeJson::number_unsigned_t *>();
        return number ? JsonOptional<double>(static_cast<double>(*number))
                      : JsonOptional<double>{};
    }
    /// 唯一拥有的 DOM；析构时递归释放节点，所有借用视图随之失效。
    NativeJson document_;
};
}  // namespace

/// 解析有界输入并转交固定地址的文档，统一原生异常与错误偏移。
/// @param text 已通过公共字节校验的只读输入；解析后不保留输入缓冲。
/// @return 独占文档或 ASCII 诊断；分配失败仍抛出。
ParseResult NlohmannJsonParser::do_parse(const string_view &text) const {
    try {
        auto document =
            NativeJson::parse(text.data(), text.data() + text.size());
        return ParseResult(
            JsonDoc(std::unique_ptr<IJsonDocument>(
                        new NlohmannJsonDocument(std::move(document))),
                JsonParser::NlohmannJSON));
    } catch (const NativeJson::parse_error &error) {
        // 原生位置从 1 开始，EOF 为 size+1；统一为从 0 开始的有界字节偏移。
        const auto offset =
            error.byte ? (std::min)(error.byte - 1, text.size()) : 0;
        return json_detail::parse_error("nlohmann JSON parse error", offset);
    } catch (const NativeJson::out_of_range &) {
        // 数值溢出异常不提供字节位置；使用 0，而不暴露原生异常或伪造定位。
        return json_detail::parse_error("nlohmann JSON number out of range");
    }
}
#else
/// 当前构建未启用 nlohmann JSON，不读取或保存输入存储。
/// @param text 已通过公共字节校验的只读输入，在本分支中忽略。
/// @return 明确的后端不可用错误，不回退到其他实现。
ParseResult NlohmannJsonParser::do_parse(const string_view &text) const {
    (void)text;
    return json_detail::parse_error("nlohmann JSON backend is not built");
}
#endif
}  // namespace mental1104
