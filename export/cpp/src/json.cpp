#include "export/json.hpp"

namespace mental1104_export_layer {

/// 调用公共 JSON 加载入口，只向导出层传递状态与诊断。
/// @param input 不保存引用的只读 JSON 文本。
/// @param parser 选择内置后端，不改变返回协议。
/// @return 成功标识、错误说明和错误字节偏移。
JsonParseResult parse_json(
    std::string_view input, mental1104::JsonParser parser) {
    JsonParseResult result{};
    // 导出协议仅传递状态与诊断；原生文档留在局部结果中，在作用域结束时释放。
    auto parsed = mental1104::load_json(input, parser);
    // 复制诊断文本而不返回指向局部结果的借用，保持既有跨语言返回协议。
    result.ok = parsed.ok;
    result.error = parsed.error;
    result.offset = parsed.offset;
    return result;
}

}  // namespace mental1104_export_layer
