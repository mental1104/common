#ifndef MENTAL1104_FILE_MOVE_H
#define MENTAL1104_FILE_MOVE_H

#include <string>
#include <system_error>

namespace mental1104 {
namespace file {

/// 发布后的状态；跨卷发布和源清理不是一个事务。
enum class MoveStatus {
    Completed,  ///< 源已清理，必要的目录同步成功。
    PublishedSourceRemains,  ///< 目标已发布，源清理失败，目录可能只清理了一部分。
    DurabilityUncertain  ///< 命名成功，但目录持久化无法确认；查看
                         ///< source_removed。
};

/// 描述已发布结果；发布前失败通过 system_error 报告。
struct MoveResult {
    MoveStatus status;  ///< 最终阶段，目录同步失败优先于清理失败。
    bool source_removed;  ///< 是否已删除源根目录项。
    std::error_code error;  ///< Completed 时为空，其他情况为相应系统错误。
};

/// 移动普通文件、目录或符号链接；仅 EXDEV 时复制、读回校验、同步、发布、清理。
/// @param source 源最终路径；调用方须独占源树及祖先目录，不支持并发修改。
/// @param target 目标最终路径，父目录必须存在；不会合并目录或追加源文件名。
/// @param overwrite 默认禁止覆盖；使用内核排他命名，不支持时明确报错。
/// @return 已发布结果；同路径/硬链接别名拒绝，发布后不得盲目重试。
/// @throws std::system_error 发布前失败；临时产物尽力清理，源及旧目标不变。
/// POSIX 跨卷保留普通 rwx 权限、mtime 与链接文本；不保证 uid/gid、ACL、xattr、
/// 稀疏布局或硬链接拓扑。跨卷不是端到端原子操作，变化检测不能代替排他访问。
/// 路径为 UTF-8（POSIX 按原始字节）；Windows 仅支持同卷，持久化报告为不确定。
MoveResult move_file(const std::string& source, const std::string& target,
    bool overwrite = false);

}  // namespace file
}  // namespace mental1104

#endif
