#ifndef MENTAL1104_UTIL
#define MENTAL1104_UTIL

#include <iostream>
#include <functional>
#include <string>


namespace mental1104 {
    class Timed {
    public:
        Timed(const std::function<void()> &func, std::string name) : func{func}, name{std::move(name)} {}
        void operator()() const;
    private:
        std::function<void()> func;
        std::string name;
    };
}

#endif


