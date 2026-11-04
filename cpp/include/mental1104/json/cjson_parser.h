#pragma once
#include "mental1104/json/parser.h"
namespace mental1104 {
/// 无状态 cJSON 适配器，原生节点和释放规则仅存在于实现文件。
class CJsonParser final : public IJsonParser {
public:
    /// 用 cJSON 解析完整输入，文档独立拥有原生树。
    /// @param text 不要求 NUL 结尾的只读字节。
    /// @return 独占文档或带偏移的错误；后端禁用时返回错误。
    ParseResult parse(const string_view &text) const override;
};
}  // namespace mental1104
