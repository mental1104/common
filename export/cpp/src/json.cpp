#include "export/json.hpp"

namespace mental1104_export_layer {

JsonParseResult parse_json(std::string_view input, mental1104::JsonParser parser) {
    JsonParseResult result{};
    auto parsed = mental1104::parse_json(input, parser);
    result.ok = parsed.ok;
    result.error = parsed.error;
    result.offset = parsed.offset;
    return result;
}

}  // namespace mental1104_export_layer
