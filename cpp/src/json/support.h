#pragma once

#include "mental1104/json/parser.h"

namespace mental1104 {
namespace json_detail {
/// 构造不含文档的失败结果。
/// @param message ASCII 诊断文本，复制后不保存指针。
/// @param offset 输入起点的字节偏移。
/// @return ok=false 的解析结果。
ParseResult parse_error(const char *message, std::size_t offset = 0);
/// 检查空输入和原始 NUL，避免后端把 NUL 当成提前结束符。
/// @param text 只读输入，不保存引用。
/// @return 非法字节偏移；输入通过公共检查时为空。
JsonOptional<std::size_t> invalid_input(const string_view &text);
/// 检查有限浮点数能否无截断地转换为有符号整数。
/// @param value 后端已解析的数值，不恢复解析前丢失的精度。
/// @return 范围内的整数或空结果。
JsonOptional<long long> signed_integer(double value);
/// 检查有限浮点数能否无截断地转换为无符号整数。
/// @param value 后端已解析的数值，不恢复解析前丢失的精度。
/// @return 非负范围内的整数或空结果。
JsonOptional<unsigned long long> unsigned_integer(double value);
}  // namespace json_detail
}  // namespace mental1104
