#include "mental1104/json/rapidjson_parser.h"

#include "support.h"

#if M1104_HAS_JSON_RAPIDJSON
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <limits>
#include <utility>

namespace mental1104 {
namespace {
/// RapidJSON 文档实现；Document 及其 allocator 与所有节点共享生命周期。
class RapidJsonDocument final : public IJsonDocument {
public:
    /// 接管完整文档，后续不再移动原生 Document。
    /// @param document 转移成功解析的文档所有权。
    explicit RapidJsonDocument(rapidjson::Document &&document)
        : document_(std::move(document)) {}

private:
    /// @return 地址固定的根节点借用句柄。
    Node do_root() const noexcept override {
        return static_cast<const rapidjson::Value *>(&this->document_);
    }
    /// 查询原生节点类型。
    /// @param node 非空借用句柄。
    /// @return 逻辑类型。
    JsonType do_type(Node node) const noexcept override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (value.IsNull()) return JsonType::Null;
        if (value.IsBool()) return JsonType::Bool;
        if (value.IsNumber()) return JsonType::Number;
        if (value.IsString()) return JsonType::String;
        if (value.IsArray()) return JsonType::Array;
        return JsonType::Object;
    }
    /// 单次查询对象成员，调用原生 API 前先检查类型和长度。
    /// @param node 非空借用句柄。
    /// @param key 区分大小写的只读键，不保存引用。
    /// @return 成员句柄，非对象或缺失为空。
    Node do_get(Node node, const string_view &key) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (!value.IsObject() ||
            key.size() > (std::numeric_limits<rapidjson::SizeType>::max)()) {
            return nullptr;
        }
        // 显式构造 Value，避免 StringRef 被选为 const char* 重载而丢失长度。
        const rapidjson::Value member_name(
            rapidjson::StringRef(key.empty() ? "" : key.data(),
                static_cast<rapidjson::SizeType>(key.size())));
        const auto found = value.FindMember(member_name);
        return found == value.MemberEnd() ? nullptr : &found->value;
    }
    /// 查询数组长度。
    /// @param node 非空借用句柄。
    /// @return 数组长度，非数组返回 0。
    std::size_t do_size(Node node) const noexcept override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        return value.IsArray() ? value.Size() : 0;
    }
    /// 检查类型和索引后访问数组，避免原生断言。
    /// @param node 非空借用句柄。
    /// @param index 从 0 开始的下标。
    /// @return 元素句柄，非数组或越界为空。
    Node do_at(Node node, std::size_t index) const noexcept override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        return value.IsArray() && index < value.Size()
                   ? &value[static_cast<rapidjson::SizeType>(index)]
                   : nullptr;
    }
    /// 按显式长度借用字符串，保留内嵌 NUL 字节。
    /// @param node 非空借用句柄。
    /// @return 字符串或空结果。
    JsonOptional<string_view> do_string(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (!value.IsString()) return {};
        return string_view(value.GetString(), value.GetStringLength());
    }
    /// 严格读取布尔。
    /// @param node 非空借用句柄。
    /// @return 布尔值或空结果。
    JsonOptional<bool> do_bool(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (!value.IsBool()) return {};
        return value.GetBool();
    }
    /// 保留原生整数精度，对浮点表示做显式整数检查。
    /// @param node 非空借用句柄。
    /// @return 有符号整数或空结果。
    JsonOptional<long long> do_i64(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (value.IsInt64()) return static_cast<long long>(value.GetInt64());
        if (value.IsUint64()) return {};
        return value.IsDouble() ? json_detail::signed_integer(value.GetDouble())
                                : JsonOptional<long long>{};
    }
    /// 保留原生无符号整数精度，拒绝负数及浮点越界。
    /// @param node 非空借用句柄。
    /// @return 无符号整数或空结果。
    JsonOptional<unsigned long long> do_u64(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (value.IsUint64())
            return static_cast<unsigned long long>(value.GetUint64());
        if (value.IsInt64()) return {};
        return value.IsDouble()
                   ? json_detail::unsigned_integer(value.GetDouble())
                   : JsonOptional<unsigned long long>{};
    }
    /// 读取数值的 double 表示。
    /// @param node 非空借用句柄。
    /// @return 数值或空结果。
    JsonOptional<double> do_f64(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        if (!value.IsNumber()) return {};
        return value.GetDouble();
    }
    rapidjson::Document document_;
};
}  // namespace

ParseResult RapidJsonParser::parse(const string_view &text) const {
    if (const auto bad = json_detail::invalid_input(text)) {
        return json_detail::parse_error(
            "empty JSON input or embedded NUL byte", *bad);
    }
    rapidjson::Document document;
    document.Parse(text.data(), text.size());
    if (document.HasParseError()) {
        return json_detail::parse_error(
            rapidjson::GetParseError_En(document.GetParseError()),
            document.GetErrorOffset());
    }
    ParseResult result;
    result.doc = JsonDoc(std::unique_ptr<IJsonDocument>(
                             new RapidJsonDocument(std::move(document))),
        JsonParser::RapidJSON);
    result.ok = true;
    return result;
}
}  // namespace mental1104
#else
namespace mental1104 {
ParseResult RapidJsonParser::parse(const string_view &) const {
    return json_detail::parse_error("RapidJSON backend is not built");
}
}  // namespace mental1104
#endif
