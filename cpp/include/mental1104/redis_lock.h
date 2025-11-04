// redis_lock_test.cpp
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <sstream>
#include <sw/redis++/redis++.h>
#include <thread>
#include <vector>

using namespace sw::redis;

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
    try {
      // 使用 SET key value NX PX expire 来实现分布式锁
      bool result = redis_->set(key_, value_, expire, UpdateType::NOT_EXIST);
      if (result) {
        locked_ = true;
      }
      return result;
    } catch (const Error &err) {
      std::cerr << "try_lock error: " << err.what() << std::endl;
      return false;
    }
  }

  /**
   * @brief 释放锁，通过Lua脚本保证只有锁的拥有者才能释放锁
   */
  void unlock() {
    if (!locked_)
      return;
    // Lua脚本：判断当前键的值是否和value_一致，如果一致则删除该键
    static const std::string lua_script = R"(
            if redis.call("get", KEYS[1]) == ARGV[1] then
                return redis.call("del", KEYS[1])
            else
                return 0
            end
        )";
    try {
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
  // 生成唯一标识字符串：使用当前时间和随机数（C++11中没有标准UUID生成器）
  std::string generate_unique_value() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::stringstream ss;
    ss << now << "_" << std::rand();
    return ss.str();
  }

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
std::shared_ptr<Redis> create_redis_from_env() {
  const char *host_env = std::getenv("REDIS_HOST");
  const char *port_env = std::getenv("REDIS_PORT");
  const char *auth_env = std::getenv("REDISCLI_AUTH");

  if (!host_env || !port_env) {
    return nullptr;
  }
  std::string host = host_env;
  int port = std::stoi(port_env);

  ConnectionOptions connection_options;
  connection_options.host = host;
  connection_options.port = port;
  if (auth_env && std::string(auth_env).size() > 0) {
    connection_options.password = auth_env;
  }
  // 可根据需要调整超时设置
  connection_options.socket_timeout = std::chrono::milliseconds(200);

  try {
    auto redis = std::make_shared<Redis>(connection_options);
    return redis;
  } catch (const Error &err) {
    std::cerr << "Failed to connect to redis: " << err.what() << std::endl;
    return nullptr;
  }
}
