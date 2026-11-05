/// @file
/// 统一编解码入口：适配输入范围、选择内置解析器、构造错误并转发序列化。
/// 具体 DOM 与原生 SDK 生命周期由 detail 后端维护，公共入口不强转原生类型。
#include "mental1104/json/codec.h"

#include <cstring>
#include <utility>

#include "detail/backend.h"

namespace mental1104 {
namespace json_detail {
/// 构造不包含部分输出文本的序列化错误。
/// @param message 复制保存的 ASCII 错误说明。
/// @return ok=false、text 为空的结果。
DumpResult dump_error(const char *message) {
    DumpResult result;
    result.error = message;
    return result;
}

/// 构造不含文档的解析错误。
/// @param message 复制保存的 ASCII 诊断文本。
/// @param offset 相对于输入起点的错误字节偏移。
/// @return 失败结果，文档为空。
ParseResult parse_error(const char *message, std::size_t offset) {
    ParseResult result;
    result.error = message;
    result.offset = offset;
    return result;
}
/// 检查空输入和原始 NUL，避免后端提前终止解析。
/// @param text 只读输入，不保存引用。
/// @return 非法位置，通过检查时为空。
JsonOptional<std::size_t> invalid_input(const string_view &text) {
    // 零字节输入不是 JSON；避免对可能为空的 data() 调用 memchr。
    if (text.empty()) {
        return std::size_t{0};
    }
    // 原始 NUL 不能出现在 JSON 文本中；提前统一两种后端的结束符处理。
    const auto *nul =
        static_cast<const char *>(std::memchr(text.data(), '\0', text.size()));
    return nul ? JsonOptional<std::size_t>(
                     static_cast<std::size_t>(nul - text.data()))
               : JsonOptional<std::size_t>{};
}

}  // namespace json_detail

/// 将已经解析的文档转入结果，所有节点仍由原有的固定地址后端解释。
/// @param document 独占文档；空拥有者保持失败状态，不发布成功结果。
ParseResult::ParseResult(JsonDoc document) noexcept
    : ok(document.valid()), document_(std::move(document)) {}

/// 从内部拥有者取得只读根视图，视图不会延长结果的生命周期。
/// @return 根节点；空结果或移出后的结果返回 Missing。
JsonValueView ParseResult::get() const noexcept {
    return this->document_.root();
}

/// 转发后端的诊断标识，不参与原生节点的类型强制转换。
/// @return 所持文档的后端标识；是否可读应另用布尔判断。
JsonParser ParseResult::backend() const noexcept {
    return this->document_.backend();
}

/// 同时检查解析状态与当前所有权，避免移出后的历史 ok 标识误报成功。
/// @return 解析成功且仍拥有文档时为 true。
ParseResult::operator bool() const noexcept {
    return this->ok && this->document_.valid();
}

/// 对加载结果使用同一份文档序列化逻辑，不复制 DOM。
/// @param result 借用的加载结果，调用期间必须保持存活。
/// @return 独立输出文本或错误，不使现有借用视图失效。
DumpResult dump_json(const ParseResult &result) {
    if (!result) {
        // 失败或已移出的结果不能被误序列化为合法的 JSON null。
        return json_detail::dump_error("cannot dump an empty JSON result");
    }
    return dump_json(result.document_);
}

/// 为只实现读取的自定义后端提供明确失败结果，避免伪造输出或猜测其节点布局。
/// @return 不支持序列化的错误，文本为空；支持写出的后端应覆写此钩子。
DumpResult IJsonDocument::do_dump() const {
    return json_detail::dump_error(
        "JSON backend does not support serialization");
}

/// 校验拥有者与实际根节点后，把序列化交给文档自身的虚函数。
/// @param document 只读借用的拥有者，不转移所有权，也不使现有视图失效。
/// @return 独立的完整 JSON 文本或错误；空文档失败，合法 null 文档可以成功。
/// 内存分配或自定义后端异常仍向调用方传播。
DumpResult dump_json(const JsonDoc &document) {
    // 缺少文档或实际节点是 Missing，不能与应输出 null 的合法根混同。
    if (!document.valid() || !document.root().is_valid()) {
        return json_detail::dump_error("cannot dump an empty JSON document");
    }
    // 使用对象自己的后端分派，自定义文档也不需要枚举注册或原生类型强转。
    return document.document_->do_dump();
}

/// 允许经基类指针销毁解析器；已返回的独立文档不依赖解析器继续存活。
IJsonParser::~IJsonParser() = default;
/// 仅选择内置解析器，原生树创建与释放规则由各自 detail 后端负责。
/// @param text 只读 JSON 字节，不要求 NUL 结尾，解析完成后不保存其引用。
/// @param parser 内置后端标识，调用方省略时使用 cJSON；自定义实现应直接调用
/// parse。
/// @return 独占文档或错误；禁用的后端由其适配器报告失败，不自动切换后端。
ParseResult load_json(const string_view &text, JsonParser parser) {
    switch (parser) {
        case JsonParser::CJSON:
            return CJsonParser{}.parse(text);
        case JsonParser::RapidJSON:
            return RapidJsonParser{}.parse(text);
        case JsonParser::Custom:
            break;
    }
    // 枚举入口不能构造应用自有解析器，显式失败而不回退到某个内置库。
    return json_detail::parse_error(
        "use an IJsonParser instance for a custom backend");
}
/// 将 NUL 结尾字符串适配为明确长度，解决字面量重载选择并保留空指针错误。
/// @param text 借用的 NUL 结尾文本，nullptr 返回错误。
/// @param parser 选择内置后端，调用方省略时使用 cJSON。
/// @return 独占文档或错误，不保存输入指针。
ParseResult load_json(const char *text, JsonParser parser) {
    // 必须先检查空指针，不能把 nullptr 交给 strlen。
    if (!text) {
        return json_detail::parse_error("null JSON input");
    }
    return load_json(text, std::strlen(text), parser);
}
/// 把明确的字节区间适配到统一文本入口，禁止通过结束符推测输入边界。
/// @param data 指向至少 len 个可读字节的借用指针；nullptr 统一返回输入错误。
/// @param len 输入字节数，不包含任何额外结束符。
/// @param parser 选择内置解析后端，调用方省略时使用 cJSON。
/// @return 独占文档或错误，不保存调用方缓冲。
ParseResult load_json(const char *data, std::size_t len, JsonParser parser) {
    // 构造视图前拒绝空指针；len=0 仍按无 JSON 内容的错误输入处理。
    if (!data) {
        return json_detail::parse_error("null JSON input");
    }
    return load_json(string_view(data, len), parser);
}
#if M1104_HAS_CXX17
/// 将拥有型字符串转换成只读输入视图，复用统一的后端选择与错误契约。
/// @param text 只读 JSON 字符串，不转移所有权，不借用其存储到返回文档。
/// @param parser 选择内置解析后端，调用方省略时使用 cJSON。
/// @return 独占文档或错误；此重载仅用于 string_view 为标准视图的配置。
ParseResult load_json(const std::string &text, JsonParser parser) {
    return load_json(string_view(text), parser);
}
#endif
}  // namespace mental1104
