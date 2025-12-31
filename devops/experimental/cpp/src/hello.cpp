#include "hello.hpp"

namespace exp_hello {

std::optional<std::string_view> get_world(std::string_view greeting) {
    constexpr std::string_view needle{"world"};
    const auto pos = greeting.find(needle);
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    return greeting.substr(pos, needle.size());
}

}  // namespace exp_hello
