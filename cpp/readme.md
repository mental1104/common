# Stacktrace 崩溃堆栈捕获（C/C++）

本组件用于在致命崩溃时捕获堆栈，并输出结构化日志。默认输出 JSON Lines（每行一个 JSON），也支持 Python 风格的可读堆栈输出。支持 C 与 C++ 调用，包含原始地址与符号化帧（需调试符号）。

## 文件结构

- `include/mental1104/debug/stacktrace.h`（对外 C API）
- `src/debug/stacktrace_common.c`
- `src/debug/stacktrace_posix.c`（Linux/macOS）
- `src/debug/stacktrace_windows.c`（Windows）
- `examples/debug/stacktrace/crash_c.c`
- `examples/debug/stacktrace/crash_cpp.cpp`

## 构建

```sh
cmake -S cpp -B cpp/build
cmake --build cpp/build
```

构建目标：
- `stacktrace`（静态库）
- `crash_c` / `crash_cpp`（示例程序）

## 快速使用

```c
#include "mental1104/debug/stacktrace.h"

int main(void) {
  st_options_t opt = {0};
  opt.enable = 1;
  opt.max_frames = 64;
  opt.use_altstack = 1;
  opt.exit_on_fatal = 1;
  opt.output_fd = 2;
  opt.dump_maps = 1;
  opt.format_kind = ST_FORMAT_PYTHON; /* 可改为 ST_FORMAT_JSON */
  opt.emit_raw_frames = 1;

  st_init(&opt);
  st_dump_current_thread();

  /* 故意崩溃 */
  ((volatile int*)0x0)[1] = 1;
  return 0;
}
```

## 选项说明

`st_options_t`：
- `enable`：启用/禁用处理器。
- `max_frames`：最大帧数（默认 64，最大 256）。
- `use_altstack`：POSIX 使用 `sigaltstack`（默认 1）。
- `chain_previous`：是否链式调用旧 handler（默认 0）。
- `exit_on_fatal`：崩溃后 `_Exit`/`TerminateProcess`（默认 1）。
- `output_fd`：默认输出 FD（默认 2）。
- `write_cb`：自定义写入回调（崩溃路径要求 async‑signal‑safe）。
- `user`：写入回调用户指针。
- `symbolizer_path`：显式指定符号化器（优先于 `ST_SYMBOLIZER`）。
- `dump_maps`：Linux 输出 `/proc/self/maps`（默认 1）。
- `format_kind`：`ST_FORMAT_JSON`（默认）或 `ST_FORMAT_PYTHON`。
- `emit_raw_frames`：是否输出原始 PC（默认 1）。
- `formatter`：自定义格式化器（可插拔）。
- `formatter_user`：格式化器用户指针。

`st_init()` 和 `st_shutdown()` 可重复调用且幂等。`st_dump_current_thread()` 用于非崩溃场景主动打印。

## 格式化输出（可插拔）

- 内置格式：JSON / Python 风格。
- 设置 `format_kind` 可切换输出风格。
- 设置 `formatter` 可自定义输出格式（未实现的回调会回退到内置格式）。
- 注意：header/raw 帧可能在信号处理路径调用，回调必须 async‑signal‑safe。

自定义格式化示例（仅示意）：

```c
static void my_header(const st_context_t* ctx, st_write_fn write, void* user) {
  (void)user;
  write("MY_TRACE\n", 9);
  write(ctx->event, strlen(ctx->event));
  write("\n", 1);
}

static void my_frame(const st_frame_t* frame, st_write_fn write, void* user) {
  (void)user;
  write("  ", 2);
  write(frame->function, strlen(frame->function));
  write("\n", 1);
}

static const st_formatter_t k_my_formatter = {
  my_header,
  NULL,
  my_frame,
  NULL,
  NULL
};
```

## 符号化说明

### Linux
- 选择顺序：`symbolizer_path` → `ST_SYMBOLIZER` → `llvm-symbolizer` → `addr2line`
- 使用 `dladdr` 计算模块基址 + ASLR 偏移
- 可输出 `/proc/self/maps`

### macOS
- 选择顺序：`symbolizer_path` → `ST_SYMBOLIZER` → `xcrun atos` → `atos`
- 无 `/proc/self/maps`

### Windows
- 使用 DbgHelp：`SymInitialize` / `SymFromAddr` / `SymGetLineFromAddr64`
- 需要链接 `DbgHelp`

## 调试友好编译参数

建议：
- `-g -fno-omit-frame-pointer`
- `-rdynamic`（Linux 更易解析符号）

## Sanitizer 协作

推荐搭配：
- `-fsanitize=address,undefined`

本库不替代 sanitizer 输出，只补充结构化头信息和原始地址；POSIX 下符号化在子进程完成，避免在 handler 中做重工作。

## 输出格式

当 `format_kind=ST_FORMAT_JSON`：每行一个 JSON 对象：
- `header`：pid/tid、平台、signal/exception、fault address、ip/sp/bp
- `frame_raw`：`{frame_index, pc}`
- `frame`：`{function, file, line, column, module}`
- `maps`：Linux `/proc/self/maps`
- `footer`：退出码/信号

当 `format_kind=ST_FORMAT_PYTHON`：输出类似 Python 的堆栈格式，便于阅读。

## 示例说明

示例位于：`cpp/examples/debug/stacktrace/README.md`，包含 C 与 C++ 的堆栈差异演示。
