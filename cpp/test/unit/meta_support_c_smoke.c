/// @file
/// 仅包含公开探测头，验证 C 消费方不需要 C++ 标准库或 target 编译宏。
#include "mental1104/meta/architecture_support.h"
#include "mental1104/meta/platform_support.h"

/// 将当前 C 编译目标的平台标记编码为位掩码，供 C++ 用例核对。
/// @return Windows/Linux/Apple/macOS 分别占第 0/1/2/3 位，无需释放资源。
int m1104_meta_platform_from_c(void) {
    return M1104_OS_WINDOWS | (M1104_OS_LINUX << 1) | (M1104_OS_APPLE << 2) |
           (M1104_OS_MACOS << 3);
}

/// 将当前 C 编译目标的架构标记编码为位掩码，供 C++ 用例核对。
/// @return ARM64/X86_64 分别占第 0/1 位，未覆盖架构返回 0。
int m1104_meta_architecture_from_c(void) {
    return M1104_ARCH_ARM64 | (M1104_ARCH_X86_64 << 1);
}
