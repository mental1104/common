#ifndef __MENTAL1104_UTIL
#define __MENTAL1104_UTIL

#include <algorithm>
#include <chrono>
#include <map>
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

// 指数退避工具：返回下一次等待时长并步进，带上限与可重置。
class ExponentialBackoff {
public:
  ExponentialBackoff(std::chrono::milliseconds initial =
                         std::chrono::milliseconds(10),
                     std::chrono::milliseconds max_delay =
                         std::chrono::milliseconds(200),
                     int factor = 2) // 默认构造等价于 ExponentialBackoff(10ms, 200ms, 2)
      : initial_(initial), max_(max_delay), factor_(factor),
        current_(initial) {}

  template <typename Rep1, typename Rep2>
  ExponentialBackoff(Rep1 initial_ms, Rep2 max_ms, int factor = 2)
      : ExponentialBackoff(std::chrono::milliseconds(initial_ms),
                           std::chrono::milliseconds(max_ms), factor) {}

  std::chrono::milliseconds next() {
    auto delay = current_;
    auto next_delay =
        std::chrono::milliseconds(current_.count() * factor_);
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
