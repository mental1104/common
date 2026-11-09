// 独立 C 编译单元只包含探测头；本文件核对 C/C++ 目标标记的一致性。
#include <gtest/gtest.h>

#include <string>

#include "mental1104/common/c_api_compat.h"
#include "mental1104/meta/architecture_support.h"
#include "mental1104/meta/platform_support.h"

/// 读取独立 C 编译单元的平台位掩码；无需参数或资源释放。
/// @return Windows/Linux/Apple/macOS 分别占第 0/1/2/3 位。
COMMON_EXTERN_C int m1104_meta_platform_from_c(void);

/// 读取独立 C 编译单元的架构位掩码；无需参数或资源释放。
/// @return ARM64/X86_64 分别占第 0/1 位。
COMMON_EXTERN_C int m1104_meta_architecture_from_c(void);

/// 最小完整用法：按编译目标选择平台和架构名称，并核对实际目标。
TEST(TargetSupportTest, SelectTargetWithPublicMacros) {
    // 使用平台标记选择目标专用逻辑；未知平台显式保留 unknown。
#if M1104_OS_WINDOWS
    const std::string platform = "windows";
#elif M1104_OS_MACOS
    const std::string platform = "macos";
#elif M1104_OS_APPLE
    const std::string platform = "apple";
#elif M1104_OS_LINUX
    const std::string platform = "linux";
#else
    const std::string platform = "unknown";
#endif

    // 架构表示编译目标，不是运行机器的 CPU 厂商。
#if M1104_ARCH_ARM64
    const std::string architecture = "arm64";
#elif M1104_ARCH_X86_64
    const std::string architecture = "x86_64";
#else
    const std::string architecture = "unknown";
#endif
    EXPECT_EQ(M1104_TEST_EXPECTED_PLATFORM, platform);
    // 独立 C 消费方核对当前 slice，兼容 Apple universal binary 的逐架构编译。
    const int c_architecture = m1104_meta_architecture_from_c();
    EXPECT_EQ(c_architecture == 1   ? "arm64"
              : c_architecture == 2 ? "x86_64"
                                    : "unknown",
        architecture);
}

/// C 与 C++ 独立包含相同公开头时必须得到一致且合法的标记。
TEST(TargetSupportTest, CAndCppAgreeOnTarget) {
    EXPECT_EQ(m1104_meta_platform_from_c(),
        M1104_OS_WINDOWS | (M1104_OS_LINUX << 1) | (M1104_OS_APPLE << 2) |
            (M1104_OS_MACOS << 3));
    EXPECT_EQ(m1104_meta_architecture_from_c(),
        M1104_ARCH_ARM64 | (M1104_ARCH_X86_64 << 1));
    EXPECT_LE(M1104_OS_WINDOWS + M1104_OS_LINUX + M1104_OS_APPLE, 1);
    EXPECT_LE(M1104_OS_MACOS, M1104_OS_APPLE);
    EXPECT_LE(M1104_ARCH_ARM64 + M1104_ARCH_X86_64, 1);
    for (const int flag : {M1104_OS_WINDOWS, M1104_OS_LINUX, M1104_OS_APPLE,
             M1104_OS_MACOS, M1104_ARCH_ARM64, M1104_ARCH_X86_64}) {
        EXPECT_TRUE(flag == 0 || flag == 1);
    }
}
