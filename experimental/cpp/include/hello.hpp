#pragma once

#include <optional>
#include <string_view>

namespace exp_hello {

std::optional<std::string_view> get_world(std::string_view greeting);

}  // namespace exp_hello
