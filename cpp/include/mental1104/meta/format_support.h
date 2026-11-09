// WALKTHROUGH: done
#ifndef MENTAL1104_META_FORMAT_SUPPORT_H
#define MENTAL1104_META_FORMAT_SUPPORT_H

#include "compiler_support.h"

// 保留既有可选头能力标记；带路径的头使用字符串探测，且不强制包含 SDK。
#if M1104_HAS_INCLUDE("fmt/ranges.h")
#define M1104_HAS_FMT_RANGES 1
#else
#define M1104_HAS_FMT_RANGES 0
#endif

#if M1104_HAS_INCLUDE("fmt/ostream.h")
#define M1104_HAS_FMT_OSTREAM 1
#else
#define M1104_HAS_FMT_OSTREAM 0
#endif

// 头文件可见不等于格式化能力完整；只信任标准库提供的特性宏，不自行补定义。
#if M1104_HAS_CXX20 && M1104_HAS_INCLUDE(<format>)
#include <format>
#endif

#if M1104_HAS_CXX20 && defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#define M1104_HAS_STD_FORMAT 1
#else
#define M1104_HAS_STD_FORMAT 0
#endif

#endif  // MENTAL1104_META_FORMAT_SUPPORT_H
