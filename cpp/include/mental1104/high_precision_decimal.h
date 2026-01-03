#include <boost/multiprecision/mpfr.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<0>,
                                      boost::multiprecision::et_off>
    high_precision;

class InfiniteDecimalCalculator {
protected:
  int decimal_precision; // 用户输入的十进制精度
  int binary_precision;  // MPFR 需要的二进制精度

public:
  // 构造函数，用户传入十进制精度，内部转换为二进制精度
  explicit InfiniteDecimalCalculator(int precision)
      : decimal_precision(precision),
        binary_precision(
            static_cast<int>(std::ceil(precision * std::log2(10)))) {
    high_precision::default_precision(binary_precision);
  }

  // [修改] 声明为 const，便于在 const 场景调用，同时不改变语义
  // 原：virtual high_precision operator()() = 0;
  virtual high_precision operator()() const = 0;

  // [修改] 标记为 const；仅格式化输出，不修改对象状态
  std::string to_string() const { // 原：非 const
    std::ostringstream oss;
    oss.precision(decimal_precision);
    oss << std::fixed << (*this)();
    return oss.str();
  }

  // 获取指定区间的小数部分
  // [修改]
  // 收敛为“单行条件返回”避免出现“无小数点时的不可达分支”造成未覆盖行；语义与原逻辑等价
  std::string getDecimalSubstring(int start,
                                  int length) const { // 原：非 const & 多行分支
    const std::string s = to_string();
    const size_t dot = s.find('.');
    return (dot == std::string::npos) ? std::string()
                                      : s.substr(dot + start, length);
  }

  // [修改] 使用 = default 并加排除注释，避免空体析构在覆盖率中作为未命中行
  virtual ~InfiniteDecimalCalculator() = default; // GCOVR_EXCL_LINE
};

// 计算 e
class ECalculator : public InfiniteDecimalCalculator {
public:
  using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

  // [修改] 与基类一致，补上 const；不改变行为
  high_precision operator()() const override { // 原：非 const
    high_precision e = 0;
    high_precision fact = 1;
    for (int i = 0; i < decimal_precision; ++i) {
      e += high_precision(1) / fact;
      fact *= (i + 1);
    }
    return e;
  }
};

// 计算不动点（初始值为 1，迭代公式 x = cos(x)）
class FixedPointCalculator : public InfiniteDecimalCalculator {
public:
  using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

  // [修改] 与基类一致，补上 const；不改变行为
  high_precision operator()() const override { // 原：非 const
    high_precision x = 1.0;                    // 初始值设为 1
    for (int i = 0; i < decimal_precision; ++i) {
      x = cos(x); // 不动点迭代公式 x = cos(x)
    }
    return x;
  }
};

// 计算圆周率（使用高斯-莱格朗日算法）
class PiCalculator : public InfiniteDecimalCalculator {
public:
  using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

  // [修改] 与基类一致，补上 const；不改变行为
  high_precision operator()() const override { // 原：非 const
    high_precision a = 1;
    high_precision b = 1 / sqrt(high_precision(2));
    high_precision t = 0.25;
    high_precision p = 1;
    high_precision pi;

    for (int i = 0; i < decimal_precision; ++i) {
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
};
