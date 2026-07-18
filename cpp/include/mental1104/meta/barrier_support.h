#ifndef MENTAL1104_META_BARRIER_SUPPORT_H
#define MENTAL1104_META_BARRIER_SUPPORT_H

#include "mental1104/meta/compiler_support.h"

// Some compilers accept -std=c++20 while using a standard library that does
// not implement std::barrier yet. Check the language level, header, and feature
// test macro together before selecting the standard-library implementation.
#if M1104_HAS_CXX20 && M1104_HAS_INCLUDE(<barrier>)
#include <barrier>
#endif

#if M1104_HAS_CXX20 && defined(__cpp_lib_barrier) &&                         \
    __cpp_lib_barrier >= 201907L
#define M1104_HAS_STD_BARRIER 1
#else
#define M1104_HAS_STD_BARRIER 0
#endif

#endif // MENTAL1104_META_BARRIER_SUPPORT_H
