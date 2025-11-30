#include <string>

#include "export/json.hpp"

extern "C" {

export_layer::export_json_result export_parse_json(const char* input) {
    static thread_local std::string error_buf;
    export_layer::export_json_result r{0, nullptr, 0};
    if (input == nullptr) {
        error_buf = "input is null";
        r.error = error_buf.c_str();
        return r;
    }

    auto parsed = export_layer::parse_json(input, mental1104::JsonParser::CJSON);
    r.ok = parsed.ok ? 1 : 0;
    r.offset = parsed.offset;
    if (!parsed.ok) {
        error_buf = parsed.error;
        r.error = error_buf.c_str();
    }
    return r;
}

}
