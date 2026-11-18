#ifndef MENTAL1104_PRIVATE_MOVE_IO_H
#define MENTAL1104_PRIVATE_MOVE_IO_H

#include <cstdint>

#include "mental1104/file/move.h"

#if !defined(_WIN32)
namespace mental1104 {
namespace file {
namespace detail {
/// 私有 POSIX I/O
/// 边界；行为测试逐调用注入故障，不修改进程全局状态，不安装此头。
class MoveIO {
public:
    /// 允许测试依赖通过基类引用使用，析构无系统副作用。
    virtual ~MoveIO() = default;
    /// 原子将 source 命名为 target；overwrite 决定内核排他规则。
    /// @param source 原始项。@param target 最终项。
    /// @param overwrite 是否允许原子替换。
    /// @return 0 成功，-1 失败并设置 errno。
    virtual int rename(const std::string& source, const std::string& target,
        bool overwrite) const;
    /// 同步 path 目录；返回原始错误，不伪装成持久化完成。
    /// @param path 已存在目录。@return 成功为空，否则为系统错误。
    virtual std::error_code sync_directory(const std::string& path) const;
    /// 删除 source 单项，directory 决定 rmdir/unlink；返回原始系统结果。
    /// @param source 已核验源项。@param directory 是否是真实目录。
    /// @return 0 成功，-1 失败并设置 errno。
    virtual int remove(const std::string& source, bool directory) const;
    /// 将 input 分块复制到 output 并读回；expected
    /// 为原始字节数，描述符均为借用。
    /// @param input 可读源描述符。@param output 可读写临时描述符。
    /// @param expected 原始长度；不匹配或 I/O 失败时抛 system_error。
    virtual void copy(int input, int output, std::uint64_t expected) const;
};

/// 按 io 的操作边界移动 source 到 target；overwrite 控制最终覆盖。
/// @param source 源项。@param target 最终项。@param overwrite 是否覆盖。
/// @param io 本次调用的 I/O 依赖，生命周期覆盖整个调用。
/// @return 与公开 move_file 完全相同的已发布状态；发布前抛 system_error。
MoveResult move_with_io(const std::string& source, const std::string& target,
    bool overwrite, const MoveIO& io);
}  // namespace detail
}  // namespace file
}  // namespace mental1104
#endif
#endif
