#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#include "mental1104/meta/compiler_support.h"
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

#include "mental1104/random.h"
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
class RedisLock {
public:
  /**
   * @brief 构造函数，需要传入redis连接和锁对应的key
   * @param redis 共享的redis连接对象
   * @param key   锁的键值
   */
  RedisLock(std::shared_ptr<Redis> redis, const std::string &key)
      : redis_(redis), key_(key), locked_(false) {
    // 生成一个唯一的锁值，用于后续判断当前客户端是否持有锁
    value_ = generate_unique_value();
  }

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

  template <typename T>
  typename std::enable_if<std::is_integral<T>::value, bool>::type
  try_lock(T expire_ms) {
    if (expire_ms < 0)
      expire_ms = 0;
    return try_lock_impl(
        std::chrono::milliseconds(static_cast<long long>(expire_ms)));
  }

private:
  bool try_lock_impl(std::chrono::milliseconds expire) {
    try {
      // 使用 SET key value NX PX expire 来实现分布式锁
      bool result = redis_->set(
          key_, value_, expire,
          UpdateType::NOT_EXIST); // NX 语义：只在 key
                                  // 不存在时写入，确保只有第一个调用者持锁
      if (result) {
        locked_ = true;
      }
      // result == true 表示 SET 成功写入（锁获取成功）；false 表示 key
      // 已存在或命令失败（未持锁）
      return result;
    } catch (const Error &err) {
      // 进入这里通常是 Redis
      // 命令执行异常（连接断开/超时、序列化错误、鉴权失败等），此时未持锁
      std::cerr << "try_lock error: " << err.what() << std::endl;
      return false;
    }
  }

public:
  /**
   * @brief 释放锁，通过Lua脚本保证只有锁的拥有者才能释放锁
   */
  void unlock() {
    if (!locked_)
      return;
    // Lua脚本：判断当前键的值是否和value_一致，如果一致则删除该键；用 Lua
    // 是为了在 Redis 侧原子地完成校验+删除，避免先读后删的竞态
    static const std::string lua_script = R"(
            if redis.call("get", KEYS[1]) == ARGV[1] then
                return redis.call("del", KEYS[1]) -- DEL 返回删除的键数量：这里要么 1（删除成功），要么 0（键不存在）
            else
                return 0 -- 不一致说明锁被别人持有或已被清理，当前调用者无权删除
            end
        )";
    try {
      // eval 参数说明：
      //   KEYS 从 {key_} 传入，对应 Lua 中 KEYS[1]（Lua 数组 1 起始）。
      //   ARGV 从 {value_} 传入，对应 Lua 中 ARGV[1]（同样 1 起始）。
      //   key_/value_ 都是 std::string，sw::redis++ 会按完整字符串传给
      //   Redis（KEYS[1] 是整个字符串，不是第一个字符）。
      auto result = redis_->eval<long long>(lua_script, {key_}, {value_});
      if (result == 0) {
        std::cerr << "unlock: lock not held or value mismatch" << std::endl;
      }
    } catch (const Error &err) {
      std::cerr << "unlock error: " << err.what() << std::endl;
    }
    locked_ = false;
  }

  // 析构时尝试释放锁，避免锁泄露
  ~RedisLock() {
    try {
      unlock();
    } catch (...) {
      // 忽略析构期间的异常
    }
  }

private:
  // 生成唯一标识字符串：用随机设备 + 64bit 引擎拼接两段随机数（约
  // 128bit），避免 std::rand 撞值和非线程安全 解释：thread_local
  // 保证每线程独立的 mt19937_64；种子用时间戳 ^
  // random_device()，异或混合降低单一源质量不高时的模式偏差。
  //       static thread_local（C++11 引入）让每个线程持有自己的静态 rng
  //       实例，避免锁竞争；首次触达时构造，线程退出时销毁，适用于
  //       std::thread/pthread。
  std::string generate_unique_value() { return mental1104::random_hex<>(2); }

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
inline std::shared_ptr<Redis> create_redis_from_env() {
  // std::getenv 从进程环境取变量，未设置时返回 nullptr/NULL；C 和 C++
  // 共用这个接口。缺失必需的 host/port 就直接返回 nullptr（未设置默认值）。
  // 若需默认值，可在取出后判空填入默认，如 host_env ? host_env :
  // "127.0.0.1"。纯 C 使用时直接调用 getenv（无需 std::），头文件 <stdlib.h>。
  const char *host_env = std::getenv("REDIS_HOST");
  const char *port_env = std::getenv("REDIS_PORT");
  const char *auth_env = std::getenv("REDISCLI_AUTH");

  if (!host_env || !port_env) {
    return nullptr;
  }
  std::string host =
      host_env; // 与 std::string host(host_env); 等价，都是用 const char* 构造
                // string，对象只构造一次；不同优化级别 O0/O1/O2/O3 也无差异
  int port =
      std::stoi(port_env); // 若 REDIS_PORT 非数字，std::stoi 会抛出
                           // std::invalid_argument/OutOfRange，未捕获会让
                           // create_redis_from_env 传播异常

  ConnectionOptions connection_options;
  connection_options.host = host;
  connection_options.port = port;
  if (auth_env && std::string(auth_env).size() > 0) {
    connection_options.password = auth_env;
  }
  // Redis++ 行为（源码 connection::_auth）：构造连接后立即发送 AUTH。
  // - Redis 未设密码却提供 password => AUTH 直接报错，连接阶段失败。
  // - Redis 需要密码却未提供 => 不会发 AUTH，连接表面成功，但首条命令会收到
  // NOAUTH。 可根据需要调整超时设置
  connection_options.socket_timeout = std::chrono::milliseconds(
      200); // redisSetTimeout
            // 设置此连接上所有命令的读写超时（非连接超时），作用于会话内每条命令

  try {
    auto redis = std::make_shared<Redis>(
        connection_options); // 用 make_shared
                             // 一次分配控制块+对象；需共享所有锁实例使用同一连接，故选
                             // shared_ptr。改用直接 shared_ptr<Redis>(new ...)
                             // 语义相同；改用 unique_ptr 则不能在多个 RedisLock
                             // 间共享。
    redis->ping();
    return redis;
  } catch (const Error &err) {
    std::cerr << "Failed to connect to redis: " << err.what() << std::endl;
    return nullptr;
  }
}