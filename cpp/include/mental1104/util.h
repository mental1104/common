#ifndef __MENTAL1104_UTIL
#define __MENTAL1104_UTIL

#include <algorithm>
#include <map>
#include <unordered_map>

#if __cplusplus >= 202302L  // 检查是否支持 C++23 的 ranges::contains
#include <ranges>
#endif

namespace mental1104 {

template <typename K, typename V, typename T>
bool contains(const std::unordered_map<K, V>& m, const T& value) {
    return m.find(value) != m.end();
}

template <typename K, typename V, typename T>
bool contains(const std::map<K, V>& m, const T& value) {
    return m.find(value) != m.end();
}

template <typename Container, typename T>
bool contains(const Container& c, const T& value) {
    return std::find(c.begin(), c.end(), value) != c.end();
}

}  // namespace mental1104

#endif
