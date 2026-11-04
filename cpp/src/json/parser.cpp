#include <cmath>
#include <cstring>
#include <limits>

#include "mental1104/json.h"
#include "support.h"

namespace mental1104 {
namespace json_detail {
ParseResult parse_error(const char *message, std::size_t offset) {
    ParseResult result;
    result.error = message;
    result.offset = offset;
    return result;
}
JsonOptional<std::size_t> invalid_input(const string_view &text) {
    if (text.empty()) {
        return std::size_t{0};
    }
    const auto *nul =
        static_cast<const char *>(std::memchr(text.data(), '\0', text.size()));
    return nul ? JsonOptional<std::size_t>(
                     static_cast<std::size_t>(nul - text.data()))
               : JsonOptional<std::size_t>{};
}
JsonOptional<long long> signed_integer(double value) {
    const double bound =
        std::ldexp(1.0, std::numeric_limits<long long>::digits);
    // 正上界不能用 double(LLONG_MAX)：它可能舍入到不可转换的 2^63。
    if (!std::isfinite(value) || std::trunc(value) != value || value < -bound ||
        value >= bound) {
        return {};
    }
    return static_cast<long long>(value);
}
JsonOptional<unsigned long long> unsigned_integer(double value) {
    const double bound =
        std::ldexp(1.0, std::numeric_limits<unsigned long long>::digits);
    if (!std::isfinite(value) || std::trunc(value) != value || value < 0 ||
        value >= bound) {
        return {};
    }
    return static_cast<unsigned long long>(value);
}
}  // namespace json_detail

IJsonParser::~IJsonParser() = default;
ParseResult parse_json(const string_view &text, JsonParser parser) {
    switch (parser) {
        case JsonParser::CJSON:
            return CJsonParser{}.parse(text);
        case JsonParser::RapidJSON:
            return RapidJsonParser{}.parse(text);
        case JsonParser::Custom:
            break;
    }
    return json_detail::parse_error(
        "use an IJsonParser instance for a custom backend");
}
ParseResult parse_json(const char *data, std::size_t len, JsonParser parser) {
    if (!data) {
        return json_detail::parse_error("null JSON input");
    }
    return parse_json(string_view(data, len), parser);
}
#if M1104_HAS_CXX17
ParseResult parse_json(const std::string &text, JsonParser parser) {
    return parse_json(string_view(text), parser);
}
#endif
}  // namespace mental1104
