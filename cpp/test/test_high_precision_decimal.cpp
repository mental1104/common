#include <gtest/gtest.h>
#include <boost/multiprecision/mpfr.hpp>
#include <cmath>
#include "mental1104/high_precision_decimal.h"  // 引入你的类实现

typedef boost::multiprecision::mpfr_float high_precision;

// 测试 ECalculator 类
TEST(ECalculatorTest, Calculate) {
    int precision = 20;  // 设置精度为20位
    ECalculator e_calculator(precision);

    // 计算 e 的值
    high_precision result = e_calculator();
    std::string result_str = e_calculator.to_string();
    
    // 检查计算结果的前几个字符
    ASSERT_TRUE(result_str.substr(0, 4) == "2.71");  // 期望 e ≈ 2.71828
}

TEST(ECalculatorTest, GetDecimalSubstring) {
    int precision = 20;
    ECalculator e_calculator(precision);

    // 获取小数部分第5到第10位
    std::string decimal_substr = e_calculator.getDecimalSubstring(1, 6);
    ASSERT_EQ(decimal_substr, "718281");  // 期望结果为 "718281"
}

// 测试 FixedPointCalculator 类
TEST(FixedPointCalculatorTest, Calculate) {
    int precision = 200;  // 设置精度为20位
    FixedPointCalculator fp_calculator(precision);

    // 计算不动点的值
    high_precision result = fp_calculator();
    std::string result_str = fp_calculator.to_string();

    // 检查结果是否接近 0.739085（不动点）
    std::cout << result_str.substr(0, 8) << std::endl;
    ASSERT_TRUE(result_str.substr(0, 8) == "0.739085");
}

TEST(FixedPointCalculatorTest, GetDecimalSubstring) {
    int precision = 200;
    FixedPointCalculator fp_calculator(precision);

    // 获取小数部分第5到第15位
    std::string decimal_substr = fp_calculator.getDecimalSubstring(1, 10);
    ASSERT_EQ(decimal_substr, "7390851332");  // 期望结果
}

// 测试 PiCalculator 类
TEST(PiCalculatorTest, Calculate) {
    int precision = 20;  // 设置精度为20位
    PiCalculator pi_calculator(precision);

    // 计算圆周率的值
    high_precision result = pi_calculator();
    std::string result_str = pi_calculator.to_string();

    // 检查结果是否接近圆周率 3.14159
    ASSERT_TRUE(result_str.substr(0, 6) == "3.1415");
}

TEST(PiCalculatorTest, GetDecimalSubstring) {
    int precision = 200;
    PiCalculator pi_calculator(precision);

    // 获取小数部分第5到第15位
    std::string decimal_substr = pi_calculator.getDecimalSubstring(1, 10);
    ASSERT_EQ(decimal_substr, "1415926535");  // 期望结果
}

