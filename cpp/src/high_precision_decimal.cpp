#include "mental1104/high_precision_decimal.h"

#include <cmath>
#include <sstream>

/// 设置计算器精度，并沿用既有的 MPFR 默认精度设置。
/// @param precision 十进制精度参数；构造会更新 high_precision 的默认精度。
InfiniteDecimalCalculator::InfiniteDecimalCalculator(int precision)
    : decimal_precision(precision),
      binary_precision(static_cast<int>(std::ceil(precision * std::log2(10)))) {
  high_precision::default_precision(this->binary_precision);
}

/// 计算并按配置精度生成固定小数格式。
/// @return 独立的十进制字符串。
std::string InfiniteDecimalCalculator::to_string() const { // 原：非 const
  std::ostringstream oss;
  oss.precision(this->decimal_precision);
  oss << std::fixed << (*this)();
  return oss.str();
}

/// 计算结果并截取小数点后的指定区间，沿用 std::string::substr 的异常行为。
/// @param start 相对小数点的起始偏移。
/// @param length 要截取的字符数。
/// @return 独立子串；没有小数点时为空。
std::string InfiniteDecimalCalculator::getDecimalSubstring(
    int start, int length) const { // 原：非 const & 多行分支
  const std::string s = this->to_string();
  const size_t dot = s.find('.');
  return (dot == std::string::npos) ? std::string()
                                    : s.substr(dot + start, length);
}

/// 使用阶乘级数计算 e。
/// @return 按现有精度和迭代次数得到的高精度值。
high_precision ECalculator::operator()() const { // 原：非 const
  high_precision e = 0;
  high_precision fact = 1;
  for (int i = 0; i < this->decimal_precision; ++i) {
    e += high_precision(1) / fact;
    fact *= (i + 1);
  }
  return e;
}

/// 从 1 开始迭代 cos 不动点。
/// @return 按现有精度和迭代次数得到的高精度值。
high_precision FixedPointCalculator::operator()() const { // 原：非 const
  high_precision x = 1.0;                                 // 初始值设为 1
  for (int i = 0; i < this->decimal_precision; ++i) {
    x = cos(x); // 不动点迭代公式 x = cos(x)
  }
  return x;
}

/// 使用高斯-莱格朗日迭代计算 pi。
/// @return 按现有精度和迭代次数得到的高精度值。
high_precision PiCalculator::operator()() const { // 原：非 const
  high_precision a = 1;
  high_precision b = 1 / sqrt(high_precision(2));
  high_precision t = 0.25;
  high_precision p = 1;
  high_precision pi;

  for (int i = 0; i < this->decimal_precision; ++i) {
    high_precision a_next = (a + b) / 2;
    high_precision b_next = sqrt(a * b);
    high_precision t_next = t - p * (a - a_next) * (a - a_next);
    a = a_next;
    b = b_next;
    t = t_next;
    p *= 2;
  }
  pi = (a + b) * (a + b) / (4 * t);
  return pi;
}
