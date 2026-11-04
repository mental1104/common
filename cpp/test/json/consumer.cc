#include "consumer.h"

#include <utility>

#include "mental1104/json.h"

namespace json_consumer {
namespace {
#if M1104_HAS_JSON_CJSON || M1104_HAS_JSON_RAPIDJSON
/// 验证一个解析器的结果不依赖输入或解析器生命周期。
/// @param parser 借用的后端无关解析入口。
/// @return 调用方模板、字符串副本和移动后视图均有效时为 true。
bool verify_parser(const mental1104::IJsonParser &parser) {
    auto parsed = parser.parse(mental1104::string_view(
        "{\"status\":{\"settings\":{\"uuid\":\"consumer\"}},"
        "\"count\":42,\"ratio\":1.5,\"flag\":false}"));
    if (!parsed) return false;
    const auto root = parsed.doc.root();
    const auto settings = root.as_optional<Settings>();
    auto document = std::move(parsed.doc);
    return settings && settings->uuid == "consumer" && !parsed.doc.valid() &&
           root.get_optional<int>("count").value_or(0) == 42 &&
           root.get_optional<unsigned>("count").value_or(0) == 42u &&
           root.get_optional<float>("ratio").value_or(0) == 1.5f &&
           !root.get_optional<bool>("flag").value_or(true) &&
           document.valid() &&
           root.get_optional<std::string>({"status", "settings", "uuid"})
                   .value_or("") == "consumer";
}
#endif
}  // namespace

bool verify() {
    bool ok = true;
#if M1104_HAS_JSON_CJSON
    ok = ok && verify_parser(mental1104::CJsonParser{});
#endif
#if M1104_HAS_JSON_RAPIDJSON
    ok = ok && verify_parser(mental1104::RapidJsonParser{});
#endif
    return ok;
}
}  // namespace json_consumer
