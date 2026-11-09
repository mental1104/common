#ifndef MENTAL1104_META_ARCHITECTURE_SUPPORT_H
#define MENTAL1104_META_ARCHITECTURE_SUPPORT_H

/// @file
/// 规范化编译目标的指令集标记；C/C++ 均可独立包含，无标准库依赖。
/// 不检测运行机器、CPU 厂商、指针宽度或 SIMD 能力。
/// 所有标记始终为 0 或 1；未覆盖的架构全部为 0。

/// 目标采用原生 ARM64；ARM64EC 的混合 ABI 不归入原生 ARM64 或 X86_64。
#if !defined(_M_ARM64EC) && \
    (defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
#define M1104_ARCH_ARM64 1
#else
#define M1104_ARCH_ARM64 0
#endif

/// 目标采用 X86_64（亦称 AMD64），同时适用于 Intel 和 AMD 的该架构目标。
// MSVC 的 ARM64EC 也定义 _M_AMD64/_M_X64，不能据此选择 x86 寄存器布局。
#if !defined(_M_ARM64EC) && (defined(__x86_64__) || defined(__amd64__) || \
                                defined(_M_X64) || defined(_M_AMD64))
#define M1104_ARCH_X86_64 1
#else
#define M1104_ARCH_X86_64 0
#endif

#endif  // MENTAL1104_META_ARCHITECTURE_SUPPORT_H
