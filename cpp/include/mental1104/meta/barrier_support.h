#ifndef MENTAL1104_META_BARRIER_SUPPORT_H
#define MENTAL1104_META_BARRIER_SUPPORT_H

#include "mental1104/meta/compiler_support.h"

// 仅语言版本达到 C++20 并不能保证当前标准库已经实现 std::barrier。
// 必须同时确认头文件可见，再通过特性测试宏判断接口版本，避免在旧版
// libstdc++、libc++ 或 MSVC STL 上误选标准库路径。
#if M1104_HAS_CXX20 && M1104_HAS_INCLUDE(<barrier>)
#include <barrier>
#endif

// M1104_HAS_STD_BARRIER 始终展开为 0 或 1，调用方只需使用统一的 #if
// 分支，不需要再次判断语言版本、头文件和特性测试宏。
#if M1104_HAS_CXX20 && defined(__cpp_lib_barrier) &&                         \
    __cpp_lib_barrier >= 201907L
#define M1104_HAS_STD_BARRIER 1
#else
#define M1104_HAS_STD_BARRIER 0
#endif

#endif // MENTAL1104_META_BARRIER_SUPPORT_H
