#pragma once

#include "mental1104/json/document.h"

namespace mental1104 {
/// 单次解析结果；成功时 ok=true 且独占 doc，失败时 doc 为空。
/// error 用于诊断，offset 是从输入起点计算的错误字节偏移。
struct ParseResult {
    bool ok{false};
    std::string error;
    std::size_t offset{0};
    JsonDoc doc;
    /// 判断解析是否成功。
    /// @return 成功时为 true。
    explicit operator bool() const noexcept { return this->ok; }
};
/// 可替换的解析入口；每次产生独立文档，文档不依赖解析器存活。
class IJsonParser {
public:
    /// 通过基类释放解析器。
    virtual ~IJsonParser();
    /// 解析完整输入，不借用输入、不改变之前返回的文档。
    /// @param text 只读 JSON 字节，允许没有 NUL 结束符。
    /// @return 独占文档或解析错误；分配失败仍可抛异常。
    virtual ParseResult parse(const string_view &text) const = 0;
};
/// 保留枚举选择入口，实际解析由内置派生类实现。
/// @param text 只读输入，不保存引用。
/// @param parser 内置后端；Custom 应直接使用自定义解析器。
/// @return 文档或错误；未构建的后端返回明确错误。
ParseResult parse_json(const string_view &text, JsonParser parser);
/// 按显式字节长度解析。
/// @param data 指向至少 len 字节的只读输入，len=0 时允许 nullptr。
/// @param len 输入字节数。
/// @param parser 内置后端。
/// @return 文档或错误。
ParseResult parse_json(const char *data, std::size_t len, JsonParser parser);
#if M1104_HAS_CXX17
/// 解析字符串，结果不借用输入存储。
/// @param text 只读 JSON 字符串。
/// @param parser 内置后端。
/// @return 文档或错误。
ParseResult parse_json(const std::string &text, JsonParser parser);
#endif
}  // namespace mental1104
