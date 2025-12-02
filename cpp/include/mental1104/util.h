#ifndef __MENTAL1104_UTIL
#define __MENTAL1104_UTIL

#include "mental1104/meta/compiler_support.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>
#include <string>
#include <unordered_map>

#if __cplusplus >= 202302L // 检查是否支持 C++23 的 ranges::contains
#include <ranges>
#endif

namespace mental1104 {

template <typename K, typename V, typename T>
bool contains(const std::unordered_map<K, V> &m, const T &value) {
  return m.find(value) != m.end();
}

template <typename K, typename V, typename T>
bool contains(const std::map<K, V> &m, const T &value) {
  return m.find(value) != m.end();
}

template <typename Container, typename T>
bool contains(const Container &c, const T &value) {
  return std::find(c.begin(), c.end(), value) != c.end();
}

// to_lower_copy：把 string_view 按字符快速转换为全小写 std::string，复用 log
// 等场景的需求。 预先用 '\0' 填充只是为了分配足够空间，后续 transform
// 会写满；即便省略填充值（用 string(size_t) 构造）也会默认填
// char(0)，行为等价，这里显式写出便于理解。
inline std::string to_lower_copy(string_view text) {
  auto lower = [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  };
  std::string out(text.size(), '\0');
  std::transform(text.begin(), text.end(), out.begin(), lower);
  return out;
}

// 指数退避工具：返回下一次等待时长并步进，带上限与可重置。
class ExponentialBackoff {
public:
  ExponentialBackoff(
      std::chrono::milliseconds initial = std::chrono::milliseconds(10),
      std::chrono::milliseconds max_delay = std::chrono::milliseconds(200),
      int factor = 2) // 默认构造等价于 ExponentialBackoff(10ms, 200ms, 2)
      : initial_(initial), max_(max_delay), factor_(factor), current_(initial) {
  }

  template <typename Rep1, typename Rep2>
  ExponentialBackoff(Rep1 initial_ms, Rep2 max_ms, int factor = 2)
      : ExponentialBackoff(std::chrono::milliseconds(initial_ms),
                           std::chrono::milliseconds(max_ms), factor) {}

  std::chrono::milliseconds next() {
    auto delay = current_;
    auto next_delay = std::chrono::milliseconds(current_.count() * factor_);
    current_ = std::min(next_delay, max_);
    return delay;
  }

  void reset() { current_ = initial_; }

private:
  std::chrono::milliseconds initial_;
  std::chrono::milliseconds max_;
  int factor_;
  std::chrono::milliseconds current_;
};

} // namespace mental1104

#endif
