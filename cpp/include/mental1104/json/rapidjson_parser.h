#pragma once
#include "mental1104/json/parser.h"
namespace mental1104 {
/// 无状态 RapidJSON 适配器，文档实现管理 Document 及 allocator。
class RapidJsonParser final : public IJsonParser {
public:
    /// 解析完整输入，不使用借用输入存储的原地解析。
    /// @param text 不要求 NUL 结尾的只读字节。
    /// @return 独占文档或带偏移的错误；后端禁用时返回错误。
    ParseResult parse(const string_view &text) const override;
};
}  // namespace mental1104
