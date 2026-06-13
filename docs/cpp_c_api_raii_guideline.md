# C++ 中 C API 使用规范与 RAII 封装指南

## 1. 总原则

业务代码优先使用 C++ 标准库。C API 不是不能用，但裸资源所有权不应扩散到业务层。

凡是需要手动释放的资源，都必须 RAII 化。C API 应限制在边界层，例如 platform、io、net、third_party wrapper 等模块。common 层只提供小而稳定的基础工具，不承载业务语义。

## 2. 不推荐直接使用的 C API

| C API / C 风格 | 推荐 C++ 替代 | 原因 | 例外情况 |
| --- | --- | --- | --- |
| `malloc/free` | `std::vector`、`std::string`、`std::unique_ptr`、`std::make_unique` | 手动释放容易泄漏，异常路径难维护 | 和只接受 C buffer 的第三方 API 互操作 |
| `char*` 字符串处理、`strlen/strcpy/strcat/strcmp` | `std::string`、`mental1104::string_view`、标准算法 | 缓冲区长度和结尾字符容易出错 | C ABI、协议解析边界、第三方库回调 |
| `sprintf/snprintf/printf` | 项目 logging wrapper、iostream、`std::format` 或已有格式化封装 | 格式串和参数类型不匹配风险高 | C API 要求写入固定 buffer，且长度检查明确 |
| `scanf/sscanf/atoi/strtol` | `std::from_chars`、`std::stoi`、明确 parser | 错误处理容易被忽略，locale/溢出语义不直观 | 兼容历史格式或 C 库接口边界 |
| C array + len | `std::array`、`std::vector`、`std::span` 或迭代器区间 | 长度与指针分离，越界风险高 | C ABI 或系统调用参数 |
| `qsort/bsearch` | `std::sort`、`std::lower_bound`、ranges 算法 | 类型擦除和回调比较不如模板算法安全 | C ABI 暴露的排序/查找回调 |
| `rand/srand` | `<random>` 引擎和分布 | 全局状态、随机质量和线程语义差 | 精确复现历史 C 行为 |
| `time_t/tm/strftime` 的业务时间计算 | `<chrono>`、明确时区/格式化封装 | 时区、DST、线程安全语义复杂 | 系统 API 或 C 库只暴露这些类型 |
| `stat/opendir/readdir/remove/rename` | `std::filesystem` | 跨平台差异由标准库封装，错误处理更一致 | 需要 POSIX 特有标志或目录 fd 语义 |
| `pthread_mutex_lock/unlock` | `std::mutex` + `std::lock_guard` / `std::unique_lock` | 手动 unlock 容易在异常和多 return 分支泄漏 | 需要 pthread 特有属性、跨进程锁或 C ABI |

## 3. 仍然值得封装的 C / POSIX API

| 资源类型 | 常见 C API | 推荐封装方式 | 是否本次已落地 |
| --- | --- | --- | --- |
| fd | `open/close/read/write`、`pipe` | `mental1104::unique_fd` | 是，POSIX-only |
| `FILE*` | `fopen/fclose/fread/fwrite` | `mental1104::unique_file` | 是 |
| 临时 cleanup | `close/fclose/free/unlink` 等小范围清理 | `mental1104::scope_exit` / `make_scope_exit` | 是 |
| socket | `socket/close/send/recv` | 复用 `unique_fd` 思路，必要时另建 `unique_socket` | 否，后续按平台差异单独设计 |
| mmap | `mmap/munmap` | `mapped_region` | 否，后续可做 |
| 动态库 | `dlopen/dlclose/dlsym` | `shared_library` | 否，后续可做 |
| epoll/kqueue | `epoll_create1/epoll_ctl/epoll_wait`、`kqueue/kevent` | `epoll_handle` / `kqueue_handle` | 否，后续可做 |
| 第三方 C 库句柄 | `sqlite3*`、`CURL*`、`SSL*`、`redisContext*` | `std::unique_ptr<T, Deleter>` 或小型 wrapper | 文档示例 |

## 4. RAII wrapper 设计规范

构造时接管资源，析构时释放资源。wrapper 必须禁止拷贝，支持移动；move 后源对象必须处于无效但可析构状态。

`get()` 只观察资源，不转移所有权。`release()` 转移所有权，调用方负责释放。`reset()` 必须先释放旧资源，再接管新资源。析构函数不得抛异常，无效资源析构必须安全。

wrapper 不应该隐藏资源创建错误。资源创建失败应由调用方通过 `errno`、`std::error_code`、`std::optional`、`expected` 或项目错误类型处理。

## 5. 使用示例

`unique_fd` 管理 `pipe/open` 返回的 fd：

```cpp
#include "mental1104/c_api_raii.h"

#include <fcntl.h>
#include <unistd.h>

int fd = ::open("/tmp/input.txt", O_RDONLY);
if (fd < 0) {
  // 读取 errno 并返回错误
}
mental1104::unique_fd input(fd);

char buffer[128];
const ssize_t n = ::read(input.get(), buffer, sizeof(buffer));
```

`unique_file` 管理 `fopen` 返回的 `FILE*`：

```cpp
#include "mental1104/c_api_raii.h"

auto file = mental1104::open_file("/tmp/output.txt", "wb");
if (!file) {
  // 读取 errno 并返回错误
}
std::fputs("hello\n", file.get());
```

`scope_exit` 管理临时 cleanup：

```cpp
#include "mental1104/c_api_raii.h"

bool committed = false;
auto cleanup = mental1104::make_scope_exit([&]() {
  if (!committed) {
    // 回滚临时状态
  }
});

// ... 多个 return / throw 分支 ...
committed = true;
cleanup.dismiss();
```

`std::unique_ptr<T, Deleter>` 管理第三方 C 库指针资源：

```cpp
struct sqlite3;
extern "C" int sqlite3_close(sqlite3 *);

struct sqlite3_deleter {
  void operator()(sqlite3 *db) const noexcept {
    if (db != nullptr) {
      (void)sqlite3_close(db);
    }
  }
};

using unique_sqlite_db = std::unique_ptr<sqlite3, sqlite3_deleter>;
```

## 6. Code Review Checklist

- 是否出现裸 `new/delete/malloc/free`？
- 是否出现 fd/socket/`FILE*`/C handle 裸持有？
- 是否有资源申请后多个 return 分支？
- 是否有异常路径导致泄漏？
- 是否能用标准库替代？
- 是否应该用 `unique_fd`、`unique_file`、`scope_exit`？
- 是否误用 `memcpy` 复制非 trivially copyable 对象？
- 是否把 `errno` 泄漏到业务逻辑深处？
- 是否使用 `strlen/strcpy/sprintf/atoi/rand/qsort` 等本可替代的 C API？
