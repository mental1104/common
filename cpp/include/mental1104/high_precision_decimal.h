#include <boost/multiprecision/mpfr.hpp>
#include <iostream>
#include <cmath>
#include <fstream>
#include <sstream>

typedef boost::multiprecision::mpfr_float high_precision;

class InfiniteDecimalCalculator {
protected:
    int decimal_precision;  // 用户输入的十进制精度
    int binary_precision;   // MPFR 需要的二进制精度

public:
    // 构造函数，用户传入十进制精度，内部转换为二进制精度
    explicit InfiniteDecimalCalculator(int precision)
        : decimal_precision(precision),
          binary_precision(static_cast<int>(std::ceil(precision * std::log2(10)))) {
        boost::multiprecision::mpfr_float::default_precision(binary_precision);
    }

    virtual high_precision operator()() = 0;  // 计算操作符

    std::string to_string() {
        std::ostringstream oss;
        oss.precision(decimal_precision);
        oss << std::fixed << (*this)();
        return oss.str();
    }

    // 获取指定区间的小数部分
    std::string getDecimalSubstring(int start, int length) {
        std::string result = to_string();
        size_t decimal_pos = result.find('.');
        if (decimal_pos != std::string::npos) {
            // 提取小数部分并截取
            return result.substr(decimal_pos + start, length);
        }
        return "";  // 如果没有小数点
    }

    virtual ~InfiniteDecimalCalculator() {}
};

// 计算 e
class ECalculator : public InfiniteDecimalCalculator {
public:
    using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

    high_precision operator()() override {
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

    high_precision operator()() override {
        high_precision x = 1.0;  // 初始值设为 1
        for (int i = 0; i < decimal_precision; ++i) {
            x = cos(x);  // 不动点迭代公式 x = cos(x)
        }
        return x;
    }
};

// 计算圆周率（使用高斯-莱格朗日算法）
class PiCalculator : public InfiniteDecimalCalculator {
public:
    using InfiniteDecimalCalculator::InfiniteDecimalCalculator;

    high_precision operator()() override {
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

