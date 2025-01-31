/*
 * @Date: 2025-01-31 13:34:35
 * @Author: mental1104 mental1104@gmail.com
 * @LastEditors: mental1104 mental1104@gmail.com
 * @LastEditTime: 2025-01-31 22:18:06
 */
#include <gtest/gtest.h>
#include "mental1104/cache.h"  // 引入你的LRUCache类定义
#include "mental1104/container_printer.h"

// 一个简单的计算函数，用来作为缓存的测试函数
int compute(int x) {
    return x * 2;
}

// 测试LRU缓存
TEST(LRUCacheTest, BasicCacheBehavior) {
    LRUCache<int, int> cache(3, compute);

    // 第一次访问，缓存未命中，需要计算
    EXPECT_EQ(cache(1), 2);
    EXPECT_EQ(cache(2), 4);
    EXPECT_EQ(cache(3), 6);
    
    // 缓存已满，应该开始移除最少使用的元素
    EXPECT_EQ(cache(1), 2);  // 1 应该仍然在缓存中
    EXPECT_EQ(cache(4), 8);  // 4 是新的元素，且容量限制已满，所以会移除最旧的元素

    // 检查缓存是否按LRU策略移除
    EXPECT_EQ(cache(2), 4);  // 2 应该被移除，因为它最少被访问
    EXPECT_EQ(cache(3), 6);  // 3 应该仍然在缓存中
    EXPECT_EQ(cache(4), 8);  // 4 应该在缓存中
}

// 测试无限容量缓存
TEST(LRUCacheTest, UnlimitedCacheBehavior) {
    auto cache = make_cache<int, int>(compute);  // 使用 make_cache 创建无限容量缓存

    // 测试无限容量缓存，验证缓存大小没有限制
    EXPECT_EQ(cache(1), 2);
    EXPECT_EQ(cache(2), 4);
    EXPECT_EQ(cache(3), 6);
    EXPECT_EQ(cache(4), 8);  // 即使容量超出了正常缓存，也应该不移除任何元素

    // 即使不移除，缓存仍然应该返回正确的值
    EXPECT_EQ(cache(1), 2);
    EXPECT_EQ(cache(2), 4);
    EXPECT_EQ(cache(3), 6);
    EXPECT_EQ(cache(4), 8);
}

// 测试缓存命中与结果
TEST(LRUCacheTest, CacheHitBehavior) {
    LRUCache<int, int> cache(3, compute);

    // 访问缓存并计算
    EXPECT_EQ(cache(1), 2);  // 未缓存，计算并存入
    EXPECT_EQ(cache(2), 4);  // 未缓存，计算并存入

    // 再次访问，应该命中缓存
    EXPECT_EQ(cache(1), 2);  // 从缓存中命中
    EXPECT_EQ(cache(2), 4);  // 从缓存中命中

    // 继续填充缓存并测试LRU机制
    EXPECT_EQ(cache(3), 6);  // 缓存满时，3会加入缓存
    EXPECT_EQ(cache(4), 8);  // 由于容量限制，1会被移除，4加入

    // 检查缓存命中情况
    EXPECT_EQ(cache(1), 2);  // 1 被移除，所以这里会重新计算
    EXPECT_EQ(cache(4), 8);  // 4 被缓存，应该命中
}

TEST(LRUCacheTest, ComputeFibonacci50) {
    std::unique_ptr<LRUCache<int, unsigned long long>> cache_ptr;

    std::function<unsigned long long(int)> fibonacci = [&](int n) -> unsigned long long {
        if (n == 0) return 0;
        if (n == 1) return 1;

        // 打印调试信息
        std::cout << "Calculating Fibonacci(" << n << ")" << std::endl;

        return (*cache_ptr)(n - 1) + (*cache_ptr)(n - 2);
    };

    // 绑定 LRUCache（容量 50，使用 Fibonacci 计算）
    cache_ptr = std::make_unique<LRUCache<int, unsigned long long>>(40, fibonacci);

    // 计算 Fibonacci(50)
    unsigned long long fib40 = (*cache_ptr)(40);
    
    mental1104::ContainerPrinter::print(std::move(*cache_ptr));
    // 打印计算结果
    std::cout << "Computed Fibonacci(40): " << fib40 << std::endl;

    // 预期 Fibonacci(50) 的值
    unsigned long long expected_fib40 = 102334155ULL;

    // 断言
    EXPECT_EQ(fib40, expected_fib40);
}