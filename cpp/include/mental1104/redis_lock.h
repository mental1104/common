#pragma once
// redis_lock_test.cpp
#include "mental1104/meta/compiler_support.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#if defined(M1104_REDISPP_CXX_STANDARD)
#if M1104_REDISPP_CXX_STANDARD < 17
#include <sw/redis++/cxx11/sw/redis++/cxx_utils.h>
#else
#include <sw/redis++/cxx17/sw/redis++/cxx_utils.h>
#endif
#elif M1104_HAS_CXX17
#include <sw/redis++/cxx17/sw/redis++/cxx_utils.h>
#else
#include <sw/redis++/cxx11/sw/redis++/cxx_utils.h>
#endif
#include <sw/redis++/redis++.h>
#include <thread>
#include <type_traits>
#include <vector>

using namespace sw::redis;

// 读图索引：
// - 这个类用 Redis 的 SET NX PX 做分布式互斥锁，value_ 是本客户端的唯一标识。
// - try_lock(expire) => SET key value NX PX expire 成功则持锁；默认 30s 过期。
// - unlock() => Lua 校验 value 相等才 DEL，析构也会尝试释放，防止泄露。
// - create_redis_from_env() 从 REDIS_HOST/REDIS_PORT/REDISCLI_AUTH
// 构建共享连接。 注意点：
// - value_ 使用随机设备+64bit 引擎生成约 128bit 随机串；生产可替换成
// UUID/更强随机源。
// - 没有自动续期，长任务需自行延长或实现看门狗；锁是非可重入的。

// ======================================================================
// Redis分布式锁实现
// ======================================================================
/// 借用共享 Redis 连接实现带租期的非可重入锁；析构尽力释放，未提供自动续期。
class RedisLock {
public:
  /**
   * @brief 构造函数，需要传入redis连接和锁对应的key
   * @param redis 共享的redis连接对象
   * @param key   锁的键值
   */
  /// 保存连接与键，生成本锁对象的持有者标记。
  /// @param redis 调用期间可用的非空共享连接，本对象保存共享所有权。
  /// @param key 锁键的只读名称，本对象保存副本。
  RedisLock(std::shared_ptr<Redis> redis, const std::string &key);

  /**
   * @brief 尝试获取锁，返回true表示加锁成功
   * @param expire 锁的过期时间，默认为30000毫秒
   */
  bool try_lock(
      std::chrono::milliseconds expire = std::chrono::milliseconds(30000)) {
    return try_lock_impl(expire);
  }

  template <typename Rep, typename Period>
  bool try_lock(const std::chrono::duration<Rep, Period> &expire) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(expire);
    if (ms.count() < 0)
      ms = std::chrono::milliseconds(0);
    return try_lock_impl(ms);
  }

  template <typename T,
            typename = typename std::enable_if<std::is_integral<T>::value>::
                type> // SFINAE 约束：仅限整数类型重载（避免与 duration 冲突）
  bool try_lock(T expire_ms) {
    if (expire_ms < 0)
      expire_ms = 0;
    return try_lock_impl(
        std::chrono::milliseconds(static_cast<long long>(expire_ms)));
  }

private:
  /// 使用 SET NX PX 获取锁，成功时记录本地持有状态。
  /// @param expire 锁租期，单位为毫秒。
  /// @return 获取成功为 true；竞争失败或 Redis 错误为 false。
  bool try_lock_impl(std::chrono::milliseconds expire);

public:
  /**
   * @brief 释放锁，通过Lua脚本保证只有锁的拥有者才能释放锁
   */
  /// 通过 Lua 校验持有者后删除锁键，未持锁时直接返回；失败只记录诊断。
  void unlock();

  // 析构时尝试释放锁，避免锁泄露
  /// 尽力释放持有的锁，不让异常离开析构函数。
  ~RedisLock();

private:
  // 生成唯一标识字符串：用随机设备 + 64bit 引擎拼接两段随机数（约
  // 128bit），避免 std::rand 撞值和非线程安全 解释：thread_local
  // 保证每线程独立的 mt19937_64；种子用时间戳 ^
  // random_device()，异或混合降低单一源质量不高时的模式偏差。
  //       static thread_local（C++11 引入）让每个线程持有自己的静态 rng
  //       实例，避免锁竞争；首次触达时构造，线程退出时销毁，适用于
  //       std::thread/pthread。
  /// 为当前锁生成随机持有者标记。
  /// @return 独立的随机十六进制字符串。
  std::string generate_unique_value();

  std::shared_ptr<Redis> redis_;
  std::string key_;
  std::string value_;
  bool locked_;
};

// ======================================================================
// 辅助函数：根据环境变量创建redis连接
// 环境变量格式示例：
//   REDISCLI_AUTH=''
//   REDIS_HOST=192.168.31.239
//   REDIS_PORT=6379
// ======================================================================
/// 根据 REDIS_HOST/REDIS_PORT/REDISCLI_AUTH 创建连接并 ping 验证。
/// @return 连接的共享所有权；缺环境或 Redis 错误时为空。端口转换异常保持传播。
std::shared_ptr<Redis> create_redis_from_env();
