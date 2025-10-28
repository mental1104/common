// redis_lock_test.cpp
// #include <sw/redis++/redis++.h>
#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <sstream>
#include <random>

#include "mental1104/redis_lock.h"
using namespace sw::redis;

// ======================================================================
// 单元测试：多线程争抢同一把分布式锁，验证互斥性
// ======================================================================
TEST(RedisLockTest, MultiThreadMutexTest) {
    auto redis = create_redis_from_env();
    if (!redis) {
        GTEST_SKIP() << "Environment variables REDIS_HOST and REDIS_PORT are not set or connection failed.";
    }
    
    // 定义测试中使用的锁键，并确保测试前删除已有的key
    std::string lock_key = "test:distributed_lock";
    try {
        redis->del({lock_key});
    } catch (const Error &err) {
        std::cerr << "Error cleaning up key: " << err.what() << std::endl;
    }
    
    const int thread_count = 100;
    std::atomic<int> counter(0);
    std::vector<std::thread> threads;
    
    // 每个线程争抢锁，成功后对共享计数器做加1操作
    auto thread_func = [redis, &counter, lock_key]() {
        RedisLock lock(redis, lock_key);
        // 循环尝试获取锁（最多等待5秒）
        while (true) {
            if (lock.try_lock(std::chrono::milliseconds(5000))) {
                // 进入临界区
                int current = counter.load();
                // 模拟工作耗时
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                counter.store(current + 1);
                lock.unlock();
                break;
            } else {
                // 未获取到锁则等待一会儿重试
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };
    
    // 启动多个线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(thread_func);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }
    
    // 最终计数应等于线程数，证明互斥执行
    EXPECT_EQ(counter.load(), thread_count);
}

// ======================================================================
// 单元测试：测试同一线程重复获取锁（非可重入）
// ======================================================================
TEST(RedisLockTest, ReentrantLockTest) {
    auto redis = create_redis_from_env();
    if (!redis) {
        GTEST_SKIP() << "Environment variables REDIS_HOST and REDIS_PORT are not set or connection failed.";
    }
    
    std::string lock_key = "test:distributed_lock_reentrant";
    try {
        redis->del({lock_key});
    } catch (const Error &err) {
        std::cerr << "Error cleaning up key: " << err.what() << std::endl;
    }
    
    RedisLock lock(redis, lock_key);
    // 第一次获取锁应成功
    bool acquired = lock.try_lock(std::chrono::milliseconds(5000));
    EXPECT_TRUE(acquired);
    
    // 同一对象再次尝试获取锁，因非可重入应返回false
    bool reacquired = lock.try_lock(std::chrono::milliseconds(5000));
    EXPECT_FALSE(reacquired);
    
    lock.unlock();
}


// ======================================================================
// 单元测试：多个线程争抢同一把锁，对 Redis 中的数组元素进行累加
// ======================================================================
TEST(RedisLockTest, MultiThreadStringIncrementTest) {
    auto redis = create_redis_from_env();
    if (!redis) {
        GTEST_SKIP() << "Environment variables REDIS_HOST and REDIS_PORT are not set or connection failed.";
    }

    // 定义测试键
    std::string lock_key = "test:array_lock";
    std::string array_key = "test:redis_array";

    // 清理 Redis 中的键
    try {
        redis->del({lock_key, array_key});
    } catch (const Error &err) {
        std::cerr << "Error cleaning up keys: " << err.what() << std::endl;
    }

    // 初始化 Redis 数组元素为 "0"
    redis->set(array_key, "0");

    const int thread_count = 10;
    const int loop_count = 100;
    std::vector<std::thread> threads;

    // 线程函数：争抢锁，对 Redis 中的 `array_key` 进行累加
    auto thread_func = [redis, lock_key, array_key, loop_count](int thread_id) {
        RedisLock lock(redis, lock_key);

        for (int i = 0; i < loop_count; ++i) {
            while (true) {
                if (lock.try_lock(std::chrono::milliseconds(5000))) {
                    // 获取 Redis 存储的当前值
                    auto val_opt = redis->get(array_key);
                    if (!val_opt) {
                        lock.unlock();
                        break;
                    }
                    // 打印线程编号和当前值
                    std::cout << "Thread " << thread_id << " accessed array_key, current value: " << *val_opt << "\n";
                    // 解析整数值并累加
                    int current_value = std::stoi(*val_opt);
                    current_value += 1;

                    // 存回 Redis
                    redis->set(array_key, std::to_string(current_value));

                    lock.unlock();
                    break;
                } else {
                    std::cout << "Thread " << thread_id << " cannot get the lock: " << lock_key << "\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
    };

    // 启动多个线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(thread_func, i);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // 获取 Redis 最终存储的值
    auto final_value_opt = redis->get(array_key);
    ASSERT_TRUE(final_value_opt.has_value());

    int final_count = std::stoi(*final_value_opt);

    // 期望最终值等于 thread_count * loop_count
    EXPECT_EQ(final_count, thread_count * loop_count);
}
