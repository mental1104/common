#include "mental1104/redis_lock.h"
#include "mental1104/log.h"
#include "mental1104/random.h"

/// 保存连接与键，生成本锁对象的持有者标记。
/// @param redis 调用期间可用的非空共享连接，本对象保存共享所有权。
/// @param key 锁键的只读名称，本对象保存副本。
RedisLock::RedisLock(std::shared_ptr<Redis> redis, const std::string &key)
    : redis_(redis), key_(key), locked_(false) {
  // 生成一个唯一的锁值，用于后续判断当前客户端是否持有锁
  this->value_ = this->generate_unique_value();
}

/// 使用 SET NX PX 获取锁，成功时记录本地持有状态。
/// @param expire 锁租期，单位为毫秒。
/// @return 获取成功为 true；竞争失败或 Redis 错误为 false。
bool RedisLock::try_lock_impl(std::chrono::milliseconds expire) {
  try {
    // 使用 SET key value NX PX expire 来实现分布式锁
    bool result = this->redis_->set(
        this->key_, this->value_, expire,
        UpdateType::NOT_EXIST); // NX 语义：只在 key
                                // 不存在时写入，确保只有第一个调用者持锁
    if (result) {
      this->locked_ = true;
    }
    // result == true 表示 SET 成功写入（锁获取成功）；false 表示 key
    // 已存在或命令失败（未持锁）
    return result;
  } catch (const Error &err) {
    // 进入这里通常是 Redis
    // 命令执行异常（连接断开/超时、序列化错误、鉴权失败等），此时未持锁
    M1104_LOG_ERROR("try_lock error: ", err.what());
    return false;
  }
}

/// 通过 Lua 校验持有者后删除锁键，未持锁时直接返回；失败只记录诊断。
void RedisLock::unlock() {
  if (!this->locked_)
    return;
  // Lua脚本：判断当前键的值是否和value_一致，如果一致则删除该键；用 Lua
  // 是为了在 Redis 侧原子地完成校验+删除，避免先读后删的竞态
  static const std::string lua_script = R"(
            if redis.call("get", KEYS[1]) == ARGV[1] then
                return redis.call("del", KEYS[1])
            else
                return 0
            end
        )";
  try {
    // eval 参数说明：
    //   KEYS 从 {key_} 传入，对应 Lua 中 KEYS[1]（Lua 数组 1 起始）。
    //   ARGV 从 {value_} 传入，对应 Lua 中 ARGV[1]（同样 1 起始）。
    //   key_/value_ 都是 std::string，sw::redis++ 会按完整字符串传给
    //   Redis（KEYS[1] 是整个字符串，不是第一个字符）。
    auto result =
        this->redis_->eval<long long>(lua_script, {this->key_}, {this->value_});
    if (result == 0) {
      M1104_LOG_WARNING("unlock: lock not held or value mismatch");
    }
  } catch (const Error &err) {
    M1104_LOG_ERROR("unlock error: ", err.what());
  }
  this->locked_ = false;
}

/// 尽力释放持有的锁，不让异常离开析构函数。
RedisLock::~RedisLock() {
  try {
    this->unlock();
  } catch (...) {
    // 忽略析构期间的异常
  }
}

/// 为当前锁生成随机持有者标记。
/// @return 独立的随机十六进制字符串。
std::string RedisLock::generate_unique_value() {
  return mental1104::random_hex<>(2);
}

/// 根据 REDIS_HOST/REDIS_PORT/REDISCLI_AUTH 创建连接并 ping 验证。
/// @return 连接的共享所有权；缺环境或 Redis 错误时为空。端口转换异常保持传播。
std::shared_ptr<Redis> create_redis_from_env() {
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
    M1104_LOG_ERROR("Failed to connect to redis: ", err.what());
    return nullptr;
  }
}
