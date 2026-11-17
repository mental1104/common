#pragma once
// walkthrough: done

#include <boost/multiprecision/mpfr.hpp>
#include <string>

/// 保留公开的 Boost/MPFR 数值类型，库与消费方必须采用兼容配置。
typedef boost::multiprecision::number<
    boost::multiprecision::mpfr_float_backend<0>, boost::multiprecision::et_off>
    high_precision;

/// 高精度计算器基类；保存精度，具体算法与格式化由已编译实现提供。
class InfiniteDecimalCalculator {
protected:
  int decimal_precision; // 用户输入的十进制精度。
  int binary_precision;  // 沿用既有换算得到的 MPFR 精度参数。

public:
  /// 保存精度，并沿用既有的 MPFR 默认精度设置。
  /// @param precision 十进制精度参数；构造会更新 high_precision 的默认精度。
  explicit InfiniteDecimalCalculator(int precision);

  /// 按具体计算器的算法生成数值。
  /// @return 按当前精度计算的高精度值。
  virtual high_precision operator()() const = 0;

  /// 计算并按配置精度生成固定小数格式。
  /// @return 独立的十进制字符串。
  std::string to_string() const;

  /// 计算结果并截取小数区间，沿用 std::string::substr 的异常行为。
  /// @param start 相对小数点的起始偏移。
  /// @param length 要截取的字符数。
  /// @return 独立子串；没有小数点时为空。
  std::string getDecimalSubstring(int start, int length) const;

  /// 通过基类指针销毁具体计算器，无额外资源清理。
  virtual ~InfiniteDecimalCalculator() = default; // GCOVR_EXCL_LINE
};

/// 通过阶乘级数计算自然常数 e。
class ECalculator : public InfiniteDecimalCalculator {
public:
  using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

  /// 执行与配置精度对应次数的级数迭代。
  /// @return 计算得到的高精度 e 值。
  high_precision operator()() const override;
};

/// 从 1 开始计算 cos 不动点。
class FixedPointCalculator : public InfiniteDecimalCalculator {
public:
  using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

  /// 按配置精度对应的次数迭代 cos。
  /// @return 迭代得到的高精度不动点值。
  high_precision operator()() const override;
};

/// 使用高斯-莱格朗日算法计算圆周率。
class PiCalculator : public InfiniteDecimalCalculator {
public:
  using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

  /// 按配置精度对应的次数执行圆周率迭代。
  /// @return 计算得到的高精度 pi 值。
  high_precision operator()() const override;
};
