#ifndef __MENTAL1104_UTIL
#define __MENTAL1104_UTIL

#include "mental1104/meta/compiler_support.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <map>
#include <random>
#include <stdexcept>
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

/// 按当前 C locale 转换字符，返回独立的小写字符串。
/// @param text 只读输入，不保存视图。
/// @return 小写副本。
std::string to_lower_copy(string_view text);

/// 维护带上限的指数退避状态；随机引擎与泛型时长适配保留在调用方。
class ExponentialBackoff {
public:
  /// 建立带上限的指数退避序列。
  /// @param initial 初始等待时间。
  /// @param max_delay 后续等待上限。
  /// @param factor 每次推进的倍率。
  ExponentialBackoff(
      std::chrono::milliseconds initial = std::chrono::milliseconds(10),
      std::chrono::milliseconds max_delay = std::chrono::milliseconds(200),
      int factor = 2);

  template <typename Rep1, typename Rep2>
  ExponentialBackoff(Rep1 initial_ms, Rep2 max_ms, int factor = 2)
      : ExponentialBackoff(std::chrono::milliseconds(initial_ms),
                           std::chrono::milliseconds(max_ms), factor) {}

  /// 返回本次等待并将内部状态推进到受上限限制的下一次等待。
  /// @return 推进前的等待时长。
  std::chrono::milliseconds next();

  template <typename UniformRandomBitGenerator>
  std::chrono::milliseconds next(double jitter_ratio,
                                 UniformRandomBitGenerator &rng) {
    return with_symmetric_jitter(next(), jitter_ratio, rng);
  }

  template <typename UniformRandomBitGenerator>
  static std::chrono::milliseconds
  with_symmetric_jitter(std::chrono::milliseconds delay, double jitter_ratio,
                        UniformRandomBitGenerator &rng) {
    if (jitter_ratio < 0.0 || jitter_ratio > 1.0) {
      throw std::invalid_argument("jitter_ratio must be in [0, 1]");
    }
    if (jitter_ratio == 0.0 || delay.count() == 0) {
      return delay;
    }

    std::uniform_real_distribution<double> multiplier(1.0 - jitter_ratio,
                                                      1.0 + jitter_ratio);
    auto jittered =
        static_cast<long long>(std::llround(delay.count() * multiplier(rng)));
    return std::chrono::milliseconds(std::max<long long>(0, jittered));
  }

  /// 将下一次等待恢复到构造时的初始值。
  void reset();

private:
  std::chrono::milliseconds initial_;
  std::chrono::milliseconds max_;
  int factor_;
  std::chrono::milliseconds current_;
};

} // namespace mental1104

#endif
