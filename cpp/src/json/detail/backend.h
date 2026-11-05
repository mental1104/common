#pragma once

/// @file
/// 后端共用的内部契约；只依赖统一接口，不引入 cJSON/RapidJSON 原生类型。
#include "mental1104/json/access.h"

namespace mental1104 {
/// 共享契约只声明结果类型，访问层无需包含完整的编解码定义。
struct ParseResult;

namespace json_detail {
/// 构造失败解析结果，文档为空。
/// @param message 复制保存的 ASCII 诊断文本。
/// @param offset 相对于输入起点的错误字节偏移。
/// @return 解析失败结果。
ParseResult parse_error(const char *message, std::size_t offset = 0);
/// 构造失败序列化结果，不保留部分输出。
/// @param message 复制保存的 ASCII 错误说明。
/// @return text 为空的失败结果。
DumpResult dump_error(const char *message);
/// 检查空输入和原始 NUL，避免后端提前终止读取。
/// @param text 只读输入，不保存引用。
/// @return 非法位置；通过检查时为空。
JsonOptional<std::size_t> invalid_input(const string_view &text);
/// 检查有限数值能否无截断地表示为有符号整数。
/// @param value 已解析数值，不恢复后端丢失的原始精度。
/// @return 范围内的整数或空结果。
JsonOptional<long long> signed_integer(double value);
/// 检查有限数值的非负性、整数性和无符号范围。
/// @param value 已解析数值，不恢复后端丢失的原始精度。
/// @return 范围内的非负整数或空结果。
JsonOptional<unsigned long long> unsigned_integer(double value);
}  // namespace json_detail
}  // namespace mental1104
