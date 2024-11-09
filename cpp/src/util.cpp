#include "mental1104/util.h"
#include <iostream>

namespace mental1104 {
    void Timed::operator()() const{
        std::cout << "Entering " << name << "\n";
        func();
        std::cout << "Exiting " << name << "\n";
    }
}