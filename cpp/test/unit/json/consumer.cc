/// @file
/// 按库实际标准编译的消费端实现，组合调用方自定义模型、文档移动和编解码往返。
#include "consumer.h"

#include <utility>

#include "mental1104/json/codec.h"

namespace json_consumer {
namespace {
#if M1104_HAS_JSON_CJSON || M1104_HAS_JSON_RAPIDJSON || M1104_HAS_JSON_NLOHMANN
/// 用借用解析器产生独立文档，检查调用方模型、移动与编解码边界。
/// @param parser 借用的后端无关解析入口。
/// @return 调用方模板、加载/序列化往返及移动后视图均有效时为 true。
bool verify_parser(const mental1104::IJsonParser &parser) {
    // 独立翻译单元也必须链接公共错误路径，最低标准不能只覆盖正常分派。
    const auto empty = parser.parse(mental1104::string_view{});
    const char nul_input[] = {'{', '}', '\0', 'x'};
    const auto nul =
        parser.parse(mental1104::string_view(nul_input, sizeof(nul_input)));
    if (empty || empty.error != "empty JSON input or embedded NUL byte" ||
        empty.offset != 0 || nul || nul.error != empty.error ||
        nul.offset != 2) {
        return false;
    }
    auto parsed = parser.parse(mental1104::string_view(
        "{\"status\":{\"settings\":{\"uuid\":\"consumer\"}},"
        "\"count\":42,\"ratio\":1.5,\"flag\":false}"));
    // 解析失败时没有可供后续访问的文档，立即报告消费端验证失败。
    if (!parsed) return false;
    const auto root = parsed.get();
    const auto settings = root.get_optional<Settings>();
    // 保存根视图后转移拥有者，后续仍用旧视图读取，以检验后端地址保持稳定。
    auto document = std::move(parsed);
    const auto dumped = mental1104::dump_json(document);
    // 序列化失败时不能把部分输出送回解析器，消费端将整次往返判为失败。
    if (!dumped) return false;
    const auto restored = mental1104::load_json(
        mental1104::string_view(dumped.text), document.backend());
    // 通过公共加载入口重建独立文档，同时验证两个运行时入口均能正确链接。
    if (!restored) return false;
    const auto restored_settings = restored.get().get_optional<Settings>();

    // 在实际库标准和独立翻译单元中验证数组范围、节点模板及结果序列化。
    const auto array = parser.parse(mental1104::string_view("[1,2,3]"));
    if (!array) return false;
    int sum = 0;
    for (const auto item : array.get()) {
        const auto number = item.get_optional<int>();
        if (!number) return false;
        sum += *number;
    }
    return restored_settings && restored_settings->uuid == "consumer" &&
           sum == 6 && settings && settings->uuid == "consumer" &&
           // 移出后的读取按公开契约返回 Missing，属于本消费测试的验证目标。
           // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
           !parsed.get().is_valid() &&
           root.get_optional<int>("count").value_or(0) == 42 &&
           root.get_optional<unsigned>("count").value_or(0) == 42u &&
           root.get_optional<float>("ratio").value_or(0) == 1.5f &&
           !root.get_optional<bool>("flag").value_or(true) &&
           static_cast<bool>(document) &&
           root.get_optional<std::string>({"status", "settings", "uuid"})
                   .value_or("") == "consumer";
}
#endif
}  // namespace

/// 对每个已构建后端运行同一组消费端场景，不依赖 GoogleTest 的编译标准。
/// @return 默认后端契约及所有已启用后端均通过时为 true。
bool verify() {
    // 按库实际标准验证默认参数，关闭 cJSON 时也不能静默切换到另一后端。
    const auto loaded = mental1104::load_json("{}");
#if M1104_HAS_JSON_CJSON
    bool ok = loaded && loaded.backend() == mental1104::JsonParser::CJSON &&
              loaded.get().is_object();
    ok = ok && verify_parser(mental1104::CJsonParser{});
#else
    bool ok = !loaded && !loaded.error.empty();
#endif
#if M1104_HAS_JSON_RAPIDJSON
    ok = ok && verify_parser(mental1104::RapidJsonParser{});
#endif
#if M1104_HAS_JSON_NLOHMANN
    ok = ok && verify_parser(mental1104::NlohmannJsonParser{});
#endif
    return ok;
}
}  // namespace json_consumer
