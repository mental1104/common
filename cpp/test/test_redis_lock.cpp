// redis_lock_test.cpp
// #include <sw/redis++/redis++.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#include "mental1104/concurrency/thread/thread_util.h"
#include "mental1104/log.h"
#include "mental1104/random.h"
#include "mental1104/redis_lock.h"
#include "mental1104/util.h"
using namespace sw::redis;

// 测试基类：统一初始化/跳过逻辑
class RedisLockTest : public ::testing::Test {
protected:
  // SetUpTestSuite 是套件级静态钩子（必须 static），在首个用例执行前运行一次
  static void SetUpTestSuite() {
    redis_ = create_redis_from_env(); // 套件启动时建一次连接，供所有用例复用
  }

  // TearDownTestSuite 在套件内最后一个用例结束后调用一次，用于清理套件级资源
  static void TearDownTestSuite() { redis_.reset(); }

  // SetUp 是每个用例前的实例方法，可访问 this/成员
  void SetUp() override {
    if (!redis_) {
      GTEST_SKIP() << "Environment variables REDIS_HOST and REDIS_PORT are not "
                      "set or connection failed.";
    }
  }

  static std::shared_ptr<Redis> redis_; // 套件级共享 redis 连接
};

std::shared_ptr<Redis> RedisLockTest::redis_ = nullptr;

// 测试阅读指南：
// - MultiThreadMutexTest：100 线程抢同一锁，仅一个进入临界区，验证互斥。
// - ReentrantLockTest：同一 RedisLock 对象重复加锁返回 false，说明非可重入。
// - MultiThreadStringIncrementTest：持锁下读取/累加 Redis
// 中的字符串值，最终值等于并发数×循环次数。
// - LockExpiresAndReacquirable：等待锁自然过期后可再次获取。
// - 测试依赖环境变量 REDIS_HOST/REDIS_PORT/REDISCLI_AUTH
// 创建连接，连接失败会跳过。

// ======================================================================
// 单元测试：多线程争抢同一把分布式锁，验证互斥性
// ======================================================================
TEST_F(RedisLockTest,
       MultiThreadMutexTest) { // TEST_F 按测试夹具类分组，自动复用
                               // SetUp/成员；TEST 按手动命名的套件分组
  // 定义测试中使用的锁键，并确保测试前删除已有的key
  std::string lock_key = mental1104::key_with_random_suffix(
      "test:distributed_lock"); // gtest
                                // 默认串行执行套件；若并行跑多实例/多进程共用同一
                                // Redis，可能与其它进程同名 key
                                // 竞争，可加前缀/随机后缀避冲突
  try {
    redis_->del({lock_key}); // 返回 long long: 删除的键数量，存在为 1，不存在为
                             // 0，错误抛异常
  } catch (const Error &err) {
    std::cerr << "Error cleaning up key: " << err.what() << std::endl;
  }

  const int thread_count = 100;
  std::atomic<int> counter(
      0); // 原子读写避免数据竞争；此处只用 load/store 单步操作，不需要额外锁
  std::vector<std::thread> threads;

  // 每个线程争抢锁，成功后对共享计数器做加1操作
  auto thread_func =
      [&counter,
       &lock_key]() { // redis_ 静态共享连接；lock_key
                      // 按引用共享同一字符串（其作用域覆盖所有线程生命周期）；counter
                      // 用引用以原子共享计数
        RedisLock lock(redis_, lock_key);
        // 循环尝试获取锁（最多等待5秒）
        mental1104::ExponentialBackoff backoff(
            std::chrono::milliseconds(10), std::chrono::milliseconds(200), 2);
        while (true) {
          if (lock.try_lock(std::chrono::milliseconds(5000))) {
            auto critical = [&]() {
              int current = counter.load();
              mental1104::sleep_for(
                  std::chrono::milliseconds(50)); // 模拟工作耗时
              counter.store(current + 1);
              lock.unlock();
            };
            critical();
            break;
          } else {
            // 未获取到锁则等待一会儿重试；使用指数退避避免空转，可在此处理其他待办任务
            mental1104::sleep_for(backoff.next());
          }
        }
      };

  // 启动多个线程（算法写法）
  threads.reserve(thread_count);
  std::generate_n(std::back_inserter(threads), thread_count,
                  [&] { return std::thread(thread_func); });
  // 等价的传统写法：
  // for (int i = 0; i < thread_count; ++i) {
  //   threads.emplace_back(thread_func);
  // }

  // 等待所有线程完成（算法写法）
  std::for_each(threads.begin(), threads.end(), [](std::thread &t) {
    // joinable() 仅表示未 join/detach，线程跑完但未回收也为 true；按序 join
    // 阻塞主线程，其他线程仍并发。需要自动回收可用 C++20 std::jthread
    if (t.joinable())
      t.join();
  });
  // 等价的传统写法：
  // for (auto &t : threads) {
  //   if (t.joinable())
  //     t.join();
  // }

  // 最终计数应等于线程数，证明互斥执行
  EXPECT_EQ(counter.load(),
            thread_count)
      << "Final counter=" << counter.load()
      << " expected thread_count=" << thread_count << " (lock_key=" << lock_key
      << ")"; // EXPECT_EQ(expected, actual)
              // 固定两个参数，前者期望值，后者实际值；额外提示可用 <<
              // 拼接到失败消息（写入 gtest 的输出流）
}

// ======================================================================
// 单元测试：测试同一线程重复获取锁（非可重入）
// ======================================================================
TEST_F(RedisLockTest, ReentrantLockTest) { // 同上，使用基类共享 redis_
  std::string lock_key =
      mental1104::key_with_random_suffix("test:distributed_lock_reentrant");
  try {
    redis_->del({lock_key});
  } catch (const Error &err) {
    std::cerr << "Error cleaning up key: " << err.what() << std::endl;
  }

  RedisLock lock(redis_, lock_key);
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
TEST_F(RedisLockTest, MultiThreadStringIncrementTest) { // 同上
  // 定义测试键
  std::string lock_key = mental1104::key_with_random_suffix("test:array_lock");
  std::string array_key =
      mental1104::key_with_random_suffix("test:redis_array");

  // 清理 Redis 中的键
  try {
    redis_->del({lock_key, array_key});
  } catch (const Error &err) {
    std::cerr << "Error cleaning up keys: " << err.what() << std::endl;
  }

  // 初始化 Redis 数组元素为 "0"
  redis_->set(array_key, "0");

  const int thread_count = 10;
  const int loop_count = 100;

  // 线程函数（逐步说明）：
  // 1) 每个线程各自创建 RedisLock，竞争同一 lock_key。
  // 2) 外层循环 loop_count 次；每次都在内层 while 里反复 try_lock(5s)
  // 直到成功。 3) 持锁后读取 array_key 的值；如果键不存在，解锁并跳出本轮。 4)
  // 打印线程号和当前值，转成 int 自增，再写回 Redis。 5)
  // 解锁，结束本次循环；未取到锁则打印并 sleep 10ms 再试。
  auto thread_func = [lock_key, array_key, loop_count](int thread_id) {
    RedisLock lock(redis_, lock_key);

    for (int i = 0; i < loop_count; ++i) {
      while (true) {
        if (lock.try_lock(std::chrono::milliseconds(5000))) {
          // 获取 Redis 存储的当前值
          auto val_opt = redis_->get(array_key);
          if (!val_opt) {
            lock.unlock(); // 键不存在，解锁后跳出本轮
            break;
          }
          // 打印线程编号和当前值
          M1104_LOG_DEBUGF("Thread {} accessed array_key, current value: {}",
                           thread_id, *val_opt);
          // 解析整数值并累加
          int current_value = std::stoi(*val_opt);
          current_value += 1;

          // 存回 Redis
          redis_->set(array_key, std::to_string(current_value));

          lock.unlock();
          break;
        } else {
          M1104_LOG_DEBUGF("Thread {} cannot get the lock: {}", thread_id,
                           lock_key);
          mental1104::sleep_for(std::chrono::milliseconds(10));
        }
      }
    }
  };

  std::vector<std::thread> threads;

  // 启动多个线程（用 iota + transform 构造线程，顺序生成 id 并按序 emplace）
  std::vector<int> ids(thread_count);
  std::iota(ids.begin(), ids.end(), 0);
  threads.reserve(thread_count);
  std::transform(ids.begin(), ids.end(), std::back_inserter(threads),
                 [&](int id) {
                   // transform 顺序遍历 ids，将返回的 thread 按序 push 到
                   // threads
                   return std::thread(thread_func, id);
                 });

  // 等待所有线程完成（用 accumulate 触发 join，顺序遍历 threads 逐个
  // join，初始值 0 只是占位）
  std::accumulate(threads.begin(), threads.end(), 0,
                  [](int acc, std::thread &t) {
                    if (t.joinable())
                      t.join();
                    return acc;
                  });

  // 获取 Redis 最终存储的值
  auto final_value_opt = redis_->get(array_key);
  ASSERT_TRUE(final_value_opt.has_value());

  int final_count = std::stoi(*final_value_opt);

  // 期望最终值等于 thread_count * loop_count
  EXPECT_EQ(final_count, thread_count * loop_count);
}

// ======================================================================
// 单元测试：等待锁过期后可再次获取
// ======================================================================
TEST_F(RedisLockTest, LockExpiresAndReacquirable) {
  std::string lock_key =
      mental1104::key_with_random_suffix("test:lock_expire_reacquire");
  redis_->del({lock_key});

  RedisLock lock1(redis_, lock_key);
  ASSERT_TRUE(lock1.try_lock(std::chrono::milliseconds(200)));

  // 不主动释放，等待锁自然过期
  mental1104::sleep_for(std::chrono::milliseconds(400));

  RedisLock lock2(redis_, lock_key);
  EXPECT_TRUE(lock2.try_lock(std::chrono::milliseconds(500)))
      << "Lock should be reacquired after TTL expiry";

  lock2.unlock();
}
