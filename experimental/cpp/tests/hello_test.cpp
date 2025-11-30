#include <cassert>

#include "hello.hpp"

int main() {
    auto result = exp_hello::get_world("hello world");
    assert(result.has_value());
    assert(*result == "world");

    auto missing = exp_hello::get_world("no match here");
    assert(!missing.has_value());
    return 0;
}
