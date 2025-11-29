#ifndef MENTAL1104_META_COMPILER_SUPPORT_H
#define MENTAL1104_META_COMPILER_SUPPORT_H

// Normalize the C++ version macro:
// - MSVC sets __cplusplus inconsistently; _MSVC_LANG is the reliable one there.
#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#  define M1104_CPLUSPLUS _MSVC_LANG
#else
#  define M1104_CPLUSPLUS __cplusplus
#endif

// Version constants (C++26 is a placeholder until standardized)
#define M1104_CXX98   199711L
#define M1104_CXX11   201103L
#define M1104_CXX14   201402L
#define M1104_CXX17   201703L
#define M1104_CXX20   202002L
#define M1104_CXX23   202302L
#define M1104_CXX26   202600L  // placeholder; update when the standard finalizes

// Convenience feature-level flags
#if M1104_CPLUSPLUS >= M1104_CXX11
#  define M1104_HAS_CXX11 1
#else
#  define M1104_HAS_CXX11 0
#endif

#if M1104_CPLUSPLUS >= M1104_CXX14
#  define M1104_HAS_CXX14 1
#else
#  define M1104_HAS_CXX14 0
#endif

#if M1104_CPLUSPLUS >= M1104_CXX17
#  define M1104_HAS_CXX17 1
#else
#  define M1104_HAS_CXX17 0
#endif

#if M1104_CPLUSPLUS >= M1104_CXX20
#  define M1104_HAS_CXX20 1
#else
#  define M1104_HAS_CXX20 0
#endif

#if M1104_CPLUSPLUS >= M1104_CXX23
#  define M1104_HAS_CXX23 1
#else
#  define M1104_HAS_CXX23 0
#endif

#if M1104_CPLUSPLUS >= M1104_CXX26
#  define M1104_HAS_CXX26 1
#else
#  define M1104_HAS_CXX26 0
#endif

#include <string>
#if M1104_HAS_CXX17
#  include <string_view>
#endif

// __has_include wrapper: returns 1 when the header is reachable via the normal include search path.
#if defined(__has_include)
#  define M1104_HAS_INCLUDE(header) __has_include(header)
#else
#  define M1104_HAS_INCLUDE(header) 0
#endif

// string_view feature flag：C++17 起有 std::string_view，低于此关闭。
#if M1104_HAS_CXX17
#  define M1104_HAS_STRING_VIEW 1
#else
#  define M1104_HAS_STRING_VIEW 0
#endif

namespace mental1104 {
#if M1104_HAS_STRING_VIEW
using string_view = std::string_view; // C++17 起用 std::string_view
#else
using string_view = std::string;      // C++11/14 回退到 std::string 保持接口一致
#endif
} // namespace mental1104

#endif // MENTAL1104_META_COMPILER_SUPPORT_H
