# Stacktrace 示例说明

本目录包含两个最小可运行示例：
- `crash_c.c`：纯 C 调用，使用多层函数调用链触发崩溃。
- `crash_cpp.cpp`：C++ 类成员函数调用链触发崩溃。

它们用于演示如何通过 `mental1104/debug/stacktrace.h` 获取结构化堆栈信息，并在崩溃时定位函数/文件/行号。

## 依赖与构建建议

为获得更准确的行号与堆栈，请使用以下编译参数：
- `-g -fno-omit-frame-pointer`
- `-rdynamic`（Linux 更易解析符号）

构建命令：

```sh
cmake -S cpp -B cpp/build
cmake --build cpp/build
```

生成的可执行文件通常位于：
- `cpp/build/bin/crash_c`
- `cpp/build/bin/crash_cpp`

## 使用方式

### 1) 直接运行

```sh
cpp/build/bin/crash_c
cpp/build/bin/crash_cpp
```

程序会先调用 `st_dump_current_thread()` 打印当前线程堆栈，然后触发空指针写入导致崩溃，输出崩溃堆栈。

### 2) 观察差异

- `crash_c.c`：函数调用链为多层 C 函数（例如 `st_crash_level1 -> st_crash_level2 -> ...`）。
- `crash_cpp.cpp`：调用链为类成员函数（例如 `CrashChain::Step1 -> Step2 -> Step3 -> Crash`）。

这能直观体现：同一套堆栈捕获逻辑在 C 与 C++ 场景下的调用栈差异与可读性价值。

## 代码结构要点

两个示例均：
- `#include "mental1104/debug/stacktrace.h"`
- 配置 `st_options_t` 后调用 `st_init(&opt)`
- 使用 `st_dump_current_thread()` 打印非崩溃堆栈
- 通过多层函数调用触发崩溃

如需自定义输出格式，可使用 `opt.format_kind` 或 `opt.formatter`。
