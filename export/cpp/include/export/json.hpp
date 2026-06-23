#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "mental1104/common/c_api_compat.h"
#include "mental1104/json.h"

namespace mental1104_export_layer {

struct JsonParseResult {
    bool ok{false};
    std::string error;
    std::size_t offset{0};
};

// Wrap the project-provided JSON parser.
JsonParseResult parse_json(std::string_view input,
                           mental1104::JsonParser parser = mental1104::JsonParser::CJSON);

COMMON_EXTERN_C_BEGIN
struct export_json_result {
    int ok;          // 1 on success, 0 on failure
    const char* error;
    std::size_t offset;
};

export_json_result export_parse_json(const char* input);
COMMON_EXTERN_C_END

}  // namespace mental1104_export_layer
