# C++ 工具库

公共 C++ 头文件位于 `cpp/include/mental1104`。下面的包含示例假设已经通过仓库 CMake 安装，或包含路径中已有 `cpp/include`。

## 维护规则

新增公共函数、类、结构体、枚举、别名、宏、可复用方法或公共头文件时，必须更新此 README，写明类别、包含路径、用途、最小调用示例和备注。如果 API 已公开但稳定性尚不明确，请标记为 `待复核`。

## 分类

- 容器与字符串
- 缓存包装器
- JSON
- 日志
- C API 兼容
- 资源管理
- 随机键
- 语义基类型
- 计时
- 并发
- 网络与服务
- 调试
- 数值计算器

## 用法索引

| 类别 | 名称 | 类型 | 包含 | 用途 |
|---|---|---|---|---|
| 容器与字符串 | `contains` | 函数模板 | `mental1104/util.h` | 检查 STL 容器成员关系和 map 键是否存在。 |
| 容器与字符串 | `to_lower_copy` | 函数 | `mental1104/util.h` | 返回小写化后的 `std::string` 副本。 |
| 容器与字符串 | `ExponentialBackoff` | 类 | `mental1104/util.h` | 按上限推进重试延迟。 |
| 缓存包装器 | `LRUCache`, `make_lru_cache`, `make_cache` | 类 / 函数 | `mental1104/core/cache.h` | 用 tuple 键 LRU 或无界缓存包装可调用对象。 |
| 缓存包装器 | `LFUCache`, `make_lfu_cache` | 类 / 函数 | `mental1104/core/cache.h` | 用 tuple 键 LFU 缓存包装可调用对象。 |
| JSON | `JsonParser`, `parse_json`, `ParseResult`, `JsonDoc`, `JsonValueView` | 枚举 / 函数 / 类 | `mental1104/json.h` | 通过 cJSON 或 RapidJSON 解析 JSON，并通过后端无关视图读取值。 |
| 日志 | `LogLevel`, `get_log_level`, `set_log_level`, `log`, `logf`, `M1104_LOG_*` | 枚举 / 函数 / 宏 | `mental1104/log.h` | 通过 stdout/stderr 输出分级日志，可用时使用 spdlog。 |
| C API 兼容 | `COMMON_EXTERN_C_BEGIN`, `COMMON_EXTERN_C_END`, `COMMON_EXTERN_C` | 宏 | `mental1104/common/c_api_compat.h` | 包装 C 声明，使同一头文件可被 C 和 C++ 包含。 |
| 资源管理 | `unique_fd` | 类 | `mental1104/c_api_raii.h` | 持有 POSIX 文件描述符。 |
| 资源管理 | `unique_file`, `open_file` | 类 / 函数 | `mental1104/c_api_raii.h` | 持有 C `FILE*`。 |
| 资源管理 | `scope_exit`, `make_scope_exit` | 类 / 函数 | `mental1104/c_api_raii.h` | 在作用域退出时执行清理代码。 |
| 随机键 | `random_hex`, `key_with_random_suffix` | 函数模板 | `mental1104/random.h` | 生成随机十六进制字符串和带前缀的键。 |
| 语义基类型 | `NonCopyable`, `Movable`, `Immovable`, `MENTAL1104_MOVE_ONLY`, `is_move_only_v` | 结构体 / 宏 / trait | `mental1104/semantic.h` | 为持有资源的类型声明复制 / 移动语义。 |
| 计时 | `Timed`, `make_timed` | 类模板 / 函数 | `mental1104/timed.h` | 包装函数指针，并在调用前后输出简单计时信息。 |
| 并发 | `IExecutor` | 接口类 | `mental1104/concurrency/executor.h` | 通用 fire-and-forget 执行器接口。 |
| 并发 | `sleep_for`, `sleep_for_ms`, `ThreadPool` | 函数 / 类 | `mental1104/concurrency/thread/thread_util.h` | 睡眠辅助函数和返回 future 的线程池。 |
| 并发 | `ThreadPoolExecutor`, `BoostAsioExecutor` | 类 | `mental1104/concurrency/thread/*.h` | 基于本地线程池或 Boost.Asio 线程池的 `IExecutor` 适配器。 |
| 并发 | `Task`, `ICoroutineScheduler`, `BasicCoroutineScheduler`, `MnCoroutinePoolT`, `MnCoroutinePool`, `BoostMnCoroutinePool` | 类 / 别名 | `mental1104/concurrency/coroutine/*.h`, `mental1104/concurrency/mn/*.h` | 在执行器适配器上调度 C++20 coroutine。 |
| 容器 | `BasicBloomFilter`, `BloomFilter`, `CoarseLockBloomFilter`, `CoarseLockStringBloomFilter` | 类 / 别名 | `mental1104/bloom_filter.h` | 用于字符串或自定义键成员判断的 Bloom filter 变体。 |
| 网络与服务 | `RedisLock`, `create_redis_from_env` | 类 / 函数 | `mental1104/redis_lock.h` | 基于 Redis 的锁辅助工具。 |
| 网络与服务 | `EpollServer` | 类 | `mental1104/net/epoll_server.h` | 注册文件描述符并分发事件回调。 |
| 调试 | `st_options_t`, `st_init`, `st_shutdown`, `st_dump_current_thread` | C API | `mental1104/debug/stacktrace.h` | 初始化并输出原生 stacktrace。 |
| 数值计算器 | `high_precision`, `InfiniteDecimalCalculator`, `ECalculator`, `FixedPointCalculator`, `PiCalculator` | 别名 / 类 | `mental1104/high_precision_decimal.h` | 从高精度计算器生成十进制字符串。 |

## 详情

### `contains`, `to_lower_copy`, `ExponentialBackoff`

- **类别：** 容器与字符串
- **类型：** 函数和类
- **定义位置：** `cpp/include/mental1104/util.h`
- **包含：** `#include "mental1104/util.h"`
- **用途：** 提供通用成员判断、小写转换和重试延迟推进。

**基础用法：**

```cpp
#include "mental1104/util.h"
#include <chrono>
#include <iostream>
#include <vector>

int main() {
  std::vector<int> values{1, 2, 3};
  bool has_two = mental1104::contains(values, 2);
  std::string lower = mental1104::to_lower_copy("Hello");

  mental1104::ExponentialBackoff backoff(
      std::chrono::milliseconds(10),
      std::chrono::milliseconds(100));
  auto wait = backoff.next();
  backoff.reset();

  std::cout << has_two << " " << lower << " " << wait.count() << "\n";
}
```

**示例输出：**

```text
1 hello 10
```

**备注：**

- 对 map 使用时，`contains` 检查键是否存在。

### `LRUCache`、`LFUCache` 和缓存工厂

- **类别：** 缓存包装器
- **类型：** 类模板和工厂函数
- **定义位置：** `cpp/include/mental1104/core/cache.h`
- **包含：** `#include "mental1104/core/cache.h"`
- **用途：** 按参数 tuple 缓存可调用对象的结果。

**基础用法：**

```cpp
#include "mental1104/core/cache.h"
#include <iostream>

int slow_add(int a, int b) { return a + b; }

int main() {
  auto lru = mental1104::make_lru_cache<int, int, int>(32, slow_add);
  auto lfu = mental1104::make_lfu_cache<int, int, int>(32, slow_add);
  auto unlimited = mental1104::make_cache<int, int, int>(slow_add);

  std::cout << lru(1, 2) << " " << lfu(1, 2) << " " << unlimited(1, 2);
}
```

**示例输出：**

```text
3 3 3
```

**备注：**

- 模板参数顺序为返回类型，然后是参数类型。

### JSON 解析

- **类别：** JSON
- **类型：** 枚举、函数、类
- **定义位置：** `cpp/include/mental1104/json.h`
- **包含：** `#include "mental1104/json.h"`
- **用途：** 解析 JSON 并读取值，同时不暴露后端专有对象。

**基础用法：**

```cpp
#include "mental1104/json.h"
#include <iostream>

int main() {
  auto result = mental1104::parse_json(
      "{\"name\":\"common\",\"ok\":true}",
      mental1104::JsonParser::RapidJSON);
  if (!result) {
    std::cerr << result.error << "\n";
    return 1;
  }

  auto root = result.doc.root();
  std::cout << root.get("name").as_string_copy() << "\n";
}
```

**示例输出：**

```text
common
```

**备注：**

- 需要 C++ 构建中配置的 cJSON 和 RapidJSON 头文件 / 库。

### 日志

- **类别：** 日志
- **类型：** 枚举、函数、宏
- **定义位置：** `cpp/include/mental1104/log.h`
- **包含：** `#include "mental1104/log.h"`
- **用途：** 输出分级消息和格式化消息。

**基础用法：**

```cpp
#include "mental1104/log.h"

int main() {
  mental1104::set_log_level(mental1104::LogLevel::Info);
  mental1104::log(mental1104::LogLevel::Info, "ready: ", 1);
  mental1104::logf(mental1104::LogLevel::Warning, "retry=%d", 3);

  M1104_LOG_INFO("plain macro log");
  M1104_LOG_ERRORF("code=%d", 500);
}
```

**示例输出：**

未启用 spdlog 时的回退输出如下；启用 spdlog 时会由 spdlog 添加自己的时间戳 / logger 格式。

```text
[info] ready: 1
[warning] retry=3
[info] plain macro log
[error] code=500
```

**备注：**

- `MENTAL1104_LOG_LEVEL` 可设置进程级初始日志级别。

### C API 兼容宏

- **类别：** C API 兼容
- **类型：** 宏
- **定义位置：** `cpp/include/mental1104/common/c_api_compat.h`
- **包含：** `#include "mental1104/common/c_api_compat.h"`
- **用途：** 在不重复编写 `extern "C"` 保护的情况下，让 C 声明可被 C++ 调用方共享。

**基础用法：**

```c
#include "mental1104/common/c_api_compat.h"

COMMON_EXTERN_C_BEGIN

int common_add(int lhs, int rhs);
void common_reset(void);

COMMON_EXTERN_C_END
```

**示例结果：**

```text
无标准输出；在 C++ 编译单元中，common_add/common_reset 以 C linkage 声明。
```

**单个声明用法：**

```c
#include "mental1104/common/c_api_compat.h"

COMMON_EXTERN_C int common_add(int lhs, int rhs);
```

**示例结果：**

```text
无标准输出；common_add 在 C++ 下等价于 extern "C" 声明，在 C 下保持普通函数声明。
```

**备注：**

- 这些宏只用于 C 兼容声明。
- 不要用它们包装 C++ 类、模板、重载函数或仅存在于命名空间中的 API。

### C API RAII 包装器

- **类别：** 资源管理
- **类型：** 类和工厂函数
- **定义位置：** `cpp/include/mental1104/c_api_raii.h`
- **包含：** `#include "mental1104/c_api_raii.h"`
- **用途：** 持有 C 文件句柄、POSIX 描述符和作用域清理回调。

**基础用法：**

```cpp
#include "mental1104/c_api_raii.h"

int main() {
  auto file = mental1104::open_file("example.txt", "w");
  if (file) {
    std::fputs("hello\n", file.get());
  }

  auto cleanup = mental1104::make_scope_exit([] {
    // cleanup code here
  });
  cleanup.dismiss();
}
```

**示例结果：**

```text
无标准输出；如果 example.txt 可写，文件内容为：
hello
```

**备注：**

- `unique_fd` 仅在非 Windows 平台可用。

### 随机键

- **类别：** 随机键
- **类型：** 函数模板
- **定义位置：** `cpp/include/mental1104/random.h`
- **包含：** `#include "mental1104/random.h"`
- **用途：** 为临时键和锁值构建随机后缀。

**基础用法：**

```cpp
#include "mental1104/random.h"
#include <iostream>

int main() {
  std::cout << mental1104::random_hex<>(2) << "\n";
  std::cout << mental1104::key_with_random_suffix<>("job", 1) << "\n";
}
```

**示例输出：**

```text
<hex-string>
job:<hex-string>
```

### 语义基类型

- **类别：** 语义基类型
- **类型：** 结构体、宏、trait
- **定义位置：** `cpp/include/mental1104/semantic.h`
- **包含：** `#include "mental1104/semantic.h"`
- **用途：** 为持有资源的公共类型表达复制 / 移动约束。

**基础用法：**

```cpp
#include "mental1104/semantic.h"
#include <type_traits>

class Handle : public mental1104::Movable {
public:
  Handle() = default;
};

static_assert(mental1104::is_move_only_v<Handle>, "Handle should be move-only");
```

**示例结果：**

```text
无标准输出；static_assert 通过，Handle 被识别为 move-only 类型。
```

**备注：**

- `is_move_only_v` 需要 C++17 inline variables。

### `Timed` 和 `make_timed`

- **类别：** 计时
- **类型：** 类模板和工厂函数
- **定义位置：** `cpp/include/mental1104/timed.h`
- **包含：** `#include "mental1104/timed.h"`
- **用途：** 包装函数指针，并在调用前后打印进入 / 退出计时。

**基础用法：**

```cpp
#include "mental1104/timed.h"

int add(int a, int b) { return a + b; }

int main() {
  auto timed_add = mental1104::make_timed(add, "add");
  return timed_add(1, 2);
}
```

**示例输出和返回值：**

```text
Entering add
Exiting add with <seconds> seconds
```

`timed_add(1, 2)` 返回 `3`；上面这个 `main` 会把进程退出码设为 `3`。

### 执行器和线程辅助工具

- **类别：** 并发
- **类型：** 接口、类、函数
- **定义位置：** `cpp/include/mental1104/concurrency/...`
- **包含：** `#include "mental1104/concurrency/thread/thread_pool_executor.h"`
- **用途：** 在线程池上运行 fire-and-forget 任务或返回 future 的工作。

**基础用法：**

```cpp
#include "mental1104/concurrency/thread/thread_pool_executor.h"
#include "mental1104/concurrency/thread/thread_util.h"
#include <iostream>

int main() {
  ThreadPool pool(2);
  auto result = pool.submit([] { return 42; });
  std::cout << result.get() << "\n";

  mental1104::ThreadPoolExecutor executor(2);
  executor.execute([] {});
  mental1104::sleep_for_ms(1);
}
```

**示例输出：**

```text
42
```

**备注：**

- `BoostAsioExecutor` 需要 Boost.Asio。

### C++20 coroutine 辅助工具

- **类别：** 并发
- **类型：** 类和别名
- **定义位置：** `cpp/include/mental1104/concurrency/coroutine/*.h`, `cpp/include/mental1104/concurrency/mn/*.h`
- **包含：** `#include "mental1104/concurrency/mn/mn_coroutine_pool.h"`
- **用途：** 在执行器支撑的池上调度仓库 `Task` coroutine。

**基础用法：**

```cpp
#include "mental1104/concurrency/mn/mn_coroutine_pool.h"

#if __cplusplus >= 202002L
mental1104::Task sample_task() { co_return; }

int main() {
  mental1104::MnCoroutinePool pool(2);
  pool.spawn(sample_task());
  pool.wait_all();
}
#endif
```

**示例结果：**

```text
无标准输出；sample_task 完成后 wait_all 返回。
```

**备注：**

- 需要 C++20。`AsyncSimpleCoroutineScheduler` 别名需要 `M1104_HAS_ASYNC_SIMPLE`。

### Bloom filter

- **类别：** 容器
- **类型：** 类模板和别名
- **定义位置：** `cpp/include/mental1104/bloom_filter.h`
- **包含：** `#include "mental1104/bloom_filter.h"`
- **用途：** 添加并测试可能存在的键，可选粗粒度锁保护。

**基础用法：**

```cpp
#include "mental1104/bloom_filter.h"

int main() {
  BloomFilter bf(1000, 0.01);
  bf.insert("alice");
  bool maybe = bf.contains("alice");

  CoarseLockStringBloomFilter locked(1000, 0.01);
  locked.insert("bob");
  return maybe && locked.contains("bob") ? 0 : 1;
}
```

**示例结果：**

```text
无标准输出；maybe 为 true，locked.contains("bob") 为 true，进程退出码为 0。
```

**备注：**

- 正结果表示“可能包含”；负结果表示一定不存在。

### Redis 锁

- **类别：** 网络与服务
- **类型：** 类和函数
- **定义位置：** `cpp/include/mental1104/redis_lock.h`
- **包含：** `#include "mental1104/redis_lock.h"`
- **用途：** 获取和释放基于 Redis 的锁。

**基础用法：**

```cpp
#include "mental1104/redis_lock.h"

int main() {
  auto redis = create_redis_from_env();
  if (!redis) {
    return 1;
  }
  RedisLock lock(redis, "common:lock");
  if (lock.try_lock(30000)) {
    lock.unlock();
  }
}
```

**示例结果：**

```text
未配置可连接的 Redis 时，create_redis_from_env() 失败并返回退出码 1。
连接成功且获取锁成功时无标准输出，try_lock(30000) 返回 true 后释放锁。
```

**备注：**

- 需要 Redis++ 和 `REDIS_HOST`/`REDIS_PORT`；`REDISCLI_AUTH` 可选。
- 待复核：此头文件暴露了 `using namespace sw::redis;` 和未放入命名空间的公共符号。

### `EpollServer`

- **类别：** 网络与服务
- **类型：** 类
- **定义位置：** `cpp/include/mental1104/net/epoll_server.h`
- **包含：** `#include "mental1104/net/epoll_server.h"`
- **用途：** 注册文件描述符回调并分发事件。

**基础用法：**

```cpp
#include "mental1104/net/epoll_server.h"

int main() {
  mental1104::EpollServer server;
  int fd = 0;
  server.add_fd(fd, EPOLLIN, [](int ready_fd) {
    (void)ready_fd;
  });
  server.dispatch_once(0);
  server.remove_fd(fd);
}
```

**示例结果：**

```text
无标准输出；dispatch_once(0) 执行一次非阻塞事件分发，然后 remove_fd(fd) 注销文件描述符。
```

**备注：**

- 待复核：平台行为取决于实现文件和可用事件 API。

### Stacktrace C API

- **类别：** 调试
- **类型：** C 结构体、枚举和函数
- **定义位置：** `cpp/include/mental1104/debug/stacktrace.h`
- **包含：** `#include "mental1104/debug/stacktrace.h"`
- **用途：** 配置原生 stacktrace 输出并转储当前线程。

**基础用法：**

```cpp
#include "mental1104/debug/stacktrace.h"

int main() {
  st_options_t opt{};
  opt.enable = 1;
  opt.max_frames = 32;
  st_init(&opt);
  st_dump_current_thread();
  st_shutdown();
}
```

**示例输出：**

```text
<当前线程的 stacktrace，具体帧名和路径取决于平台、编译选项和符号信息>
```

**备注：**

- 待复核：平台支持取决于链接的 stacktrace 实现。

### 高精度十进制计算器

- **类别：** 数值计算器
- **类型：** 别名和类
- **定义位置：** `cpp/include/mental1104/high_precision_decimal.h`
- **包含：** `#include "mental1104/high_precision_decimal.h"`
- **用途：** 构造计算器对象并读取格式化十进制字符串。

**基础用法：**

```cpp
#include "mental1104/high_precision_decimal.h"
#include <iostream>

int main() {
  PiCalculator pi(50);
  ECalculator e(50);
  FixedPointCalculator fixed(50);

  std::cout << pi.to_string() << "\n";
  std::cout << e.getDecimalSubstring(1, 8) << "\n";
  std::cout << fixed.to_string() << "\n";
}
```

**示例输出：**

```text
3.14159265358979323846264338327950288419716939937511
71828182
0.73908513...
```

**备注：**

- 需要带 MPFR/GMP 支持的 Boost.Multiprecision。
- 待复核：这些类目前位于全局命名空间，不同于本仓库多数 C++ API。
