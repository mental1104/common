#ifndef MENTAL1104_UTIL
#define MENTAL1104_UTIL

#include <algorithm>

#if __cplusplus >= 202302L // 检查是否支持 C++23 的 ranges::contains
    #include <ranges>
#endif

namespace mental1104 {

    // 定义一个模板函数
    template <typename Container, typename T>
    inline bool contains(const Container& container, const T& value) {
    #if __cplusplus >= 202302L && defined(__cpp_lib_ranges) && __cpp_lib_ranges > 202110L
            return std::ranges::contains(container, value);
    #else
            return std::find(container.begin(), container.end(), value) != container.end();
    #endif
        }
    
}

#endif
