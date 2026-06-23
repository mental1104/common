# C++ utilities

Public C++ headers live under `cpp/include/mental1104`. Include examples below assume the repository CMake install or an include path that contains `cpp/include`.

## Maintenance rule

When adding a public function, class, struct, enum, alias, macro, reusable method, or public header, update this README with its category, include path, purpose, minimal call, and notes. If the API surface is public but not clearly stable, mark it `Needs review`.

## Categories

- Containers and strings
- Cache wrappers
- JSON
- Logging
- C API compatibility
- Resource management
- Random keys
- Semantic base types
- Timing
- Concurrency
- Network and services
- Debugging
- Numeric calculators

## Usage index

| Category | Name | Type | Include | Purpose |
|---|---|---|---|---|
| Containers and strings | `contains` | function template | `mental1104/util.h` | Check membership in STL containers and map keys. |
| Containers and strings | `to_lower_copy` | function | `mental1104/util.h` | Return a lowercase `std::string` copy. |
| Containers and strings | `ExponentialBackoff` | class | `mental1104/util.h` | Step through capped retry delays. |
| Cache wrappers | `LRUCache`, `make_lru_cache`, `make_cache` | class/functions | `mental1104/core/cache.h` | Wrap a callable with tuple-keyed LRU/unbounded caching. |
| Cache wrappers | `LFUCache`, `make_lfu_cache` | class/function | `mental1104/core/cache.h` | Wrap a callable with tuple-keyed LFU caching. |
| JSON | `JsonParser`, `parse_json`, `ParseResult`, `JsonDoc`, `JsonValueView` | enum/functions/classes | `mental1104/json.h` | Parse JSON through cJSON or RapidJSON and read values through a backend-neutral view. |
| Logging | `LogLevel`, `get_log_level`, `set_log_level`, `log`, `logf`, `M1104_LOG_*` | enum/functions/macros | `mental1104/log.h` | Emit leveled logs with stdout/stderr or spdlog when available. |
| C API compatibility | `COMMON_EXTERN_C_BEGIN`, `COMMON_EXTERN_C_END`, `COMMON_EXTERN_C` | macros | `mental1104/common/c_api_compat.h` | Wrap C declarations so the same header can be included from C and C++. |
| Resource management | `unique_fd` | class | `mental1104/c_api_raii.h` | Own a POSIX file descriptor. |
| Resource management | `unique_file`, `open_file` | class/function | `mental1104/c_api_raii.h` | Own a C `FILE*`. |
| Resource management | `scope_exit`, `make_scope_exit` | class/function | `mental1104/c_api_raii.h` | Run cleanup code at scope exit. |
| Random keys | `random_hex`, `key_with_random_suffix` | function templates | `mental1104/random.h` | Generate random hex strings and prefixed keys. |
| Semantic base types | `NonCopyable`, `Movable`, `Immovable`, `MENTAL1104_MOVE_ONLY`, `is_move_only_v` | structs/macro/trait | `mental1104/semantic.h` | Declare copy/move semantics for resource-owning types. |
| Timing | `Timed`, `make_timed` | class template/function | `mental1104/timed.h` | Wrap function pointers with simple entry/exit timing output. |
| Concurrency | `IExecutor` | interface class | `mental1104/concurrency/executor.h` | Common fire-and-forget executor interface. |
| Concurrency | `sleep_for`, `sleep_for_ms`, `ThreadPool` | functions/class | `mental1104/concurrency/thread/thread_util.h` | Sleep helpers and a future-returning thread pool. |
| Concurrency | `ThreadPoolExecutor`, `BoostAsioExecutor` | classes | `mental1104/concurrency/thread/*.h` | `IExecutor` adapters backed by local or Boost.Asio thread pools. |
| Concurrency | `Task`, `ICoroutineScheduler`, `BasicCoroutineScheduler`, `MnCoroutinePoolT`, `MnCoroutinePool`, `BoostMnCoroutinePool` | classes/aliases | `mental1104/concurrency/coroutine/*.h`, `mental1104/concurrency/mn/*.h` | C++20 coroutine scheduling over executor adapters. |
| Containers | `BasicBloomFilter`, `BloomFilter`, `CoarseLockBloomFilter`, `CoarseLockStringBloomFilter` | classes/aliases | `mental1104/bloom_filter.h` | Bloom filter variants for string or custom key membership checks. |
| Network and services | `RedisLock`, `create_redis_from_env` | class/function | `mental1104/redis_lock.h` | Redis-backed lock helper. |
| Network and services | `EpollServer` | class | `mental1104/net/epoll_server.h` | Register file descriptors and dispatch event callbacks. |
| Debugging | `st_options_t`, `st_init`, `st_shutdown`, `st_dump_current_thread` | C API | `mental1104/debug/stacktrace.h` | Initialize and dump native stacktrace output. |
| Numeric calculators | `high_precision`, `InfiniteDecimalCalculator`, `ECalculator`, `FixedPointCalculator`, `PiCalculator` | alias/classes | `mental1104/high_precision_decimal.h` | Produce decimal strings from high-precision calculators. |

## Details

### `contains`, `to_lower_copy`, `ExponentialBackoff`

**Category:** Containers and strings  
**Type:** functions and class  
**Defined in:** `cpp/include/mental1104/util.h`  
**Include:** `#include "mental1104/util.h"`  
**Purpose:** General membership, lowercase conversion, and retry delay stepping.

**Basic usage:**

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

**Notes:**

- For maps, `contains` checks key existence.

### `LRUCache`, `LFUCache`, and cache factories

**Category:** Cache wrappers  
**Type:** class templates and factory functions  
**Defined in:** `cpp/include/mental1104/core/cache.h`  
**Include:** `#include "mental1104/core/cache.h"`  
**Purpose:** Memoize callable results by argument tuple.

**Basic usage:**

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

**Notes:**

- Template parameters are return type followed by argument types.

### JSON parsing

**Category:** JSON  
**Type:** enum, functions, classes  
**Defined in:** `cpp/include/mental1104/json.h`  
**Include:** `#include "mental1104/json.h"`  
**Purpose:** Parse JSON and read values without exposing backend-specific objects.

**Basic usage:**

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

**Notes:**

- Requires cJSON and RapidJSON headers/libraries as configured by the C++ build.

### Logging

**Category:** Logging  
**Type:** enum, functions, macros  
**Defined in:** `cpp/include/mental1104/log.h`  
**Include:** `#include "mental1104/log.h"`  
**Purpose:** Emit leveled messages and formatted messages.

**Basic usage:**

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

**Notes:**

- `MENTAL1104_LOG_LEVEL` can set the initial process-level log level.

### C API compatibility macros

**Category:** C API compatibility  
**Type:** macros  
**Defined in:** `cpp/include/mental1104/common/c_api_compat.h`  
**Include:** `#include "mental1104/common/c_api_compat.h"`  
**Purpose:** Share C declarations with C++ callers without repeating `extern "C"` guards.

**Basic usage:**

```c
#include "mental1104/common/c_api_compat.h"

COMMON_EXTERN_C_BEGIN

int common_add(int lhs, int rhs);
void common_reset(void);

COMMON_EXTERN_C_END
```

**Single declaration usage:**

```c
#include "mental1104/common/c_api_compat.h"

COMMON_EXTERN_C int common_add(int lhs, int rhs);
```

**Notes:**

- Use these macros only around C-compatible declarations.
- Do not wrap C++ classes, templates, overloaded functions, or namespace-only APIs.

### C API RAII wrappers

**Category:** Resource management  
**Type:** classes and factory function  
**Defined in:** `cpp/include/mental1104/c_api_raii.h`  
**Include:** `#include "mental1104/c_api_raii.h"`  
**Purpose:** Own C file handles, POSIX descriptors, and scope cleanup callbacks.

**Basic usage:**

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

**Notes:**

- `unique_fd` is only available on non-Windows platforms.

### Random keys

**Category:** Random keys  
**Type:** function templates  
**Defined in:** `cpp/include/mental1104/random.h`  
**Include:** `#include "mental1104/random.h"`  
**Purpose:** Build random suffixes for temporary keys and lock values.

**Basic usage:**

```cpp
#include "mental1104/random.h"
#include <iostream>

int main() {
  std::cout << mental1104::random_hex<>(2) << "\n";
  std::cout << mental1104::key_with_random_suffix<>("job", 1) << "\n";
}
```

### Semantic base types

**Category:** Semantic base types  
**Type:** structs, macro, trait  
**Defined in:** `cpp/include/mental1104/semantic.h`  
**Include:** `#include "mental1104/semantic.h"`  
**Purpose:** Express copy/move constraints in public resource-owning types.

**Basic usage:**

```cpp
#include "mental1104/semantic.h"
#include <type_traits>

class Handle : public mental1104::Movable {
public:
  Handle() = default;
};

static_assert(mental1104::is_move_only_v<Handle>, "Handle should be move-only");
```

**Notes:**

- `is_move_only_v` requires C++17 inline variables.

### `Timed` and `make_timed`

**Category:** Timing  
**Type:** class template and factory function  
**Defined in:** `cpp/include/mental1104/timed.h`  
**Include:** `#include "mental1104/timed.h"`  
**Purpose:** Wrap a function pointer and print entry/exit timing around calls.

**Basic usage:**

```cpp
#include "mental1104/timed.h"

int add(int a, int b) { return a + b; }

int main() {
  auto timed_add = mental1104::make_timed(add, "add");
  return timed_add(1, 2);
}
```

### Executor and thread helpers

**Category:** Concurrency  
**Type:** interface, classes, functions  
**Defined in:** `cpp/include/mental1104/concurrency/...`  
**Include:** `#include "mental1104/concurrency/thread/thread_pool_executor.h"`  
**Purpose:** Run fire-and-forget tasks or future-returning work on thread pools.

**Basic usage:**

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

**Notes:**

- `BoostAsioExecutor` requires Boost.Asio.

### C++20 coroutine helpers

**Category:** Concurrency  
**Type:** classes and aliases  
**Defined in:** `cpp/include/mental1104/concurrency/coroutine/*.h`, `cpp/include/mental1104/concurrency/mn/*.h`  
**Include:** `#include "mental1104/concurrency/mn/mn_coroutine_pool.h"`  
**Purpose:** Schedule repository `Task` coroutines over executor-backed pools.

**Basic usage:**

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

**Notes:**

- C++20 is required. `AsyncSimpleCoroutineScheduler` aliases require `M1104_HAS_ASYNC_SIMPLE`.

### Bloom filters

**Category:** Containers  
**Type:** class templates and aliases  
**Defined in:** `cpp/include/mental1104/bloom_filter.h`  
**Include:** `#include "mental1104/bloom_filter.h"`  
**Purpose:** Add and test possibly-present keys with optional coarse-grained locking.

**Basic usage:**

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

**Notes:**

- A positive result means "may contain"; a negative result means absent.

### Redis lock

**Category:** Network and services  
**Type:** class and function  
**Defined in:** `cpp/include/mental1104/redis_lock.h`  
**Include:** `#include "mental1104/redis_lock.h"`  
**Purpose:** Acquire and release a Redis-backed lock.

**Basic usage:**

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

**Notes:**

- Requires Redis++ and `REDIS_HOST`/`REDIS_PORT`; `REDISCLI_AUTH` is optional.
- Needs review: this header exposes `using namespace sw::redis;` and un-namespaced public symbols.

### `EpollServer`

**Category:** Network and services  
**Type:** class  
**Defined in:** `cpp/include/mental1104/net/epoll_server.h`  
**Include:** `#include "mental1104/net/epoll_server.h"`  
**Purpose:** Register file descriptor callbacks and dispatch events.

**Basic usage:**

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

**Notes:**

- Needs review: platform behavior depends on the implementation file and available event API.

### Stacktrace C API

**Category:** Debugging  
**Type:** C structs, enum, and functions  
**Defined in:** `cpp/include/mental1104/debug/stacktrace.h`  
**Include:** `#include "mental1104/debug/stacktrace.h"`  
**Purpose:** Configure native stacktrace output and dump the current thread.

**Basic usage:**

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

**Notes:**

- Needs review: platform support depends on the linked stacktrace implementation.

### High-precision decimal calculators

**Category:** Numeric calculators  
**Type:** alias and classes  
**Defined in:** `cpp/include/mental1104/high_precision_decimal.h`  
**Include:** `#include "mental1104/high_precision_decimal.h"`  
**Purpose:** Construct calculator objects and read formatted decimal strings.

**Basic usage:**

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

**Notes:**

- Requires Boost.Multiprecision with MPFR/GMP support.
- Needs review: these classes are currently in the global namespace, unlike most C++ APIs in this repository.
