#include <iostream>

#include "hello.hpp"

int main() {
    auto result = exp_hello::get_world("hello world");
    if (!result.has_value()) {
        return 1;
    }
    std::cout << *result << "\n";
    return 0;
}
