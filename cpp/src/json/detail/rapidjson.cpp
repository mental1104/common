/// @file
/// RapidJSON 后端：统一管理 DOM/allocator、节点访问及 Writer 输出。
#include <limits>
#include <utility>

#include "backend.h"
#include "mental1104/json/codec.h"
#if M1104_HAS_JSON_RAPIDJSON
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#endif

namespace mental1104 {
#if M1104_HAS_JSON_RAPIDJSON
namespace {
/// 通过 Writer 输出完整 JSON，失败时不暴露部分文本。
/// @param document 只读借用 DOM 及其 allocator，不转移所有权。
/// @return 独立的紧凑 JSON 文本或错误。
DumpResult dump_rapidjson_document(const rapidjson::Document &document) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    // 由 DOM 发出类型化事件，64 位整数直接写出，不先转换成可能丢精度的 double。
    if (!document.Accept(writer)) {
        // Writer 拒绝数值或编码时，不能把已生成的前缀当成成功输出。
        return json_detail::dump_error("RapidJSON serialization failed");
    }
    DumpResult result;
    // 完整遍历成功后复制临时缓冲，返回文本不借用 Writer、DOM 或 allocator。
    result.text.assign(buffer.GetString(), buffer.GetSize());
    result.ok = true;
    return result;
}

/// RapidJSON 文档实现；Document 拥有或管理 allocator，子节点借用其存储。
/// 对象构造后原生 DOM 不再移动或修改，因此 JsonDoc 移动不改变已公开节点的地址。
class RapidJsonDocument final : public IJsonDocument {
public:
    /// 接管完整文档，后续不再移动原生 Document。
    /// @param document 转移成功解析的文档所有权。
    explicit RapidJsonDocument(rapidjson::Document &&document)
        : document_(std::move(document)) {}

private:
    /// 使用当前后端的 Writer，保持原生 DOM 及借用节点地址不变。
    /// @return 独立文本或序列化错误。
    DumpResult do_dump() const override {
        return dump_rapidjson_document(this->document_);
    }
    /// 借用地址固定的原生根节点。
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
        // 先检查节点类型和 SizeType 上界，再进入会断言或收窄长度的原生 API。
        if (!value.IsObject() ||
            key.size() > (std::numeric_limits<rapidjson::SizeType>::max)()) {
            return nullptr;
        }
        // 显式构造 Value，避免 StringRef 被选为 const char* 重载而丢失长度。
        const rapidjson::Value member_name(
            rapidjson::StringRef(key.empty() ? "" : key.data(),
                static_cast<rapidjson::SizeType>(key.size())));
        // 单次查找同时判断存在性并取得成员，找不到时不解引用 MemberEnd。
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
        // 短路保护原生 operator[]：类型与下标有效之前不做数组访问或索引收窄。
        return value.IsArray() && index < value.Size()
                   ? &value[static_cast<rapidjson::SizeType>(index)]
                   : nullptr;
    }
    /// 按显式长度借用字符串，保留内嵌 NUL 字节。
    /// @param node 非空借用句柄。
    /// @return 字符串或空结果。
    JsonOptional<string_view> do_string(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        // GetString 对错误类型会断言；预期类型错误在此转换为空结果。
        if (!value.IsString()) return {};
        return string_view(value.GetString(), value.GetStringLength());
    }
    /// 严格读取布尔。
    /// @param node 非空借用句柄。
    /// @return 布尔值或空结果。
    JsonOptional<bool> do_bool(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        // false 也是有效值；仅节点类型不符时返回空 optional。
        if (!value.IsBool()) return {};
        return value.GetBool();
    }
    /// 保留原生整数精度，对浮点表示做显式整数检查。
    /// @param node 非空借用句柄。
    /// @return 有符号整数或空结果。
    JsonOptional<long long> do_i64(Node node) const override {
        const auto &value = *static_cast<const rapidjson::Value *>(node);
        // 优先原生整数路径，避免 LLONG_MAX 等值经 double 舍入到不可表示的上界。
        if (value.IsInt64()) return static_cast<long long>(value.GetInt64());
        // 不属于 Int64 的 Uint64 必然超出有符号上界，不能直接强制转换。
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
        // 非负整数已被 Uint64 分支接收，此处剩余的 Int64 为负数。
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
    /// 唯一持有原生 DOM 和 allocator；文档释放后所有原生节点与字符串借用失效。
    rapidjson::Document document_;
};
}  // namespace

/// 常规解析有界只读输入，成功后把 DOM 与 allocator 一起移入固定地址的文档。
/// @param text 不要求 NUL 结尾的 JSON 字节，不修改输入，也不借用其存储到结果。
/// @return 独占文档或错误，错误位置为输入起点的字节偏移；分配异常仍可抛出。
ParseResult RapidJsonParser::parse(const string_view &text) const {
    // 统一拒绝空输入和原始 NUL，避免不同 SDK 对结束符的处理产生分歧。
    if (const auto bad = json_detail::invalid_input(text)) {
        return json_detail::parse_error(
            "empty JSON input or embedded NUL byte", *bad);
    }
    // 使用非原地解析，DOM 的字符串存储不依赖调用方输入缓冲继续存活。
    rapidjson::Document document;
    document.Parse(text.data(), text.size());
    // 失败时保留结构化偏移；局部 Document 自行清理已经解析出的部分内容。
    if (document.HasParseError()) {
        return json_detail::parse_error(
            rapidjson::GetParseError_En(document.GetParseError()),
            document.GetErrorOffset());
    }
    // 对外取得根视图之前完成唯一一次原生 DOM 移动；构造失败仍由局部对象清理。
    return ParseResult(JsonDoc(std::unique_ptr<IJsonDocument>(
                                   new RapidJsonDocument(std::move(document))),
        JsonParser::RapidJSON));
}

#else
/// 当前构建缺少 RapidJSON，接收但不读取传入的只读视图，也不保存其存储。
/// @return 后端不可用错误，不隐式改用 cJSON 或其他实现。
ParseResult RapidJsonParser::parse(const string_view &) const {
    return json_detail::parse_error("RapidJSON backend is not built");
}
#endif
}  // namespace mental1104
