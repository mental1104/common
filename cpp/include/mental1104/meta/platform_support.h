#ifndef MENTAL1104_META_PLATFORM_SUPPORT_H
#define MENTAL1104_META_PLATFORM_SUPPORT_H

/// @file
/// 规范化编译目标的平台标记；C/C++ 均可独立包含，不依赖构建配置头。
/// 所有标记始终为 0 或 1；未知平台全部为 0，不据此推断 POSIX 能力。

/// 目标属于 Windows，包括 32 位和 64 位目标。
#if defined(_WIN32)
#define M1104_OS_WINDOWS 1
#else
#define M1104_OS_WINDOWS 0
#endif

/// 目标属于 Linux；仅描述目标平台，不保证具体系统 API 可用。
#if defined(__linux__)
#define M1104_OS_LINUX 1
#else
#define M1104_OS_LINUX 0
#endif

/// 目标属于 Apple 平台家族，不能替代 macOS 专用 API 的判断。
#if defined(__APPLE__) && defined(__MACH__)
#define M1104_OS_APPLE 1
#include <TargetConditionals.h>
#else
#define M1104_OS_APPLE 0
#endif

/// 目标为 macOS；其他 Apple 平台仍保持 M1104_OS_APPLE 为 1。
#if M1104_OS_APPLE && defined(TARGET_OS_OSX) && TARGET_OS_OSX
#define M1104_OS_MACOS 1
#else
#define M1104_OS_MACOS 0
#endif

#endif  // MENTAL1104_META_PLATFORM_SUPPORT_H
