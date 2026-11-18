#include <gtest/gtest.h>

#include <cerrno>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../src/file/move_io.h"
#include "mental1104/file/move.h"

#if !defined(_WIN32)
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using mental1104::file::move_file;
using mental1104::file::MoveStatus;

/// 尽力删除 path 的测试资源，不跟随链接，不传播异常。
void erase_tree(const std::string& path) {
    struct stat info;
    if (::lstat(path.c_str(), &info)) return;
    if (S_ISDIR(info.st_mode)) {
        ::chmod(path.c_str(), 0700);
        DIR* directory = ::opendir(path.c_str());
        if (directory) {
            while (const dirent* entry = ::readdir(directory)) {
                const std::string name(entry->d_name);
                if (name != "." && name != "..") erase_tree(path + "/" + name);
            }
            ::closedir(directory);
        }
        ::rmdir(path.c_str());
    } else
        ::unlink(path.c_str());
}

/// 为单个场景独占临时目录，并在退出时自动回收。
struct TempDirectory {
    std::string path;
    /// 在 parent 中创建私有测试目录，失败抛系统异常。
    explicit TempDirectory(const std::string& parent = "/tmp") {
        const std::string pattern = parent + "/common-move-XXXXXX";
        std::vector<char> text(pattern.begin(), pattern.end());
        text.push_back('\0');
        if (!::mkdtemp(text.data()))
            throw std::system_error(errno, std::generic_category());
        this->path = text.data();
    }
    /// 只回收本对象创建的目录树。
    ~TempDirectory() { erase_tree(this->path); }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
};

/// 写入 path 的 data 并关闭流；失败使当前场景失败。
void write_file(const std::string& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary);
    output << data;
    output.close();
    ASSERT_TRUE(output.good());
}
/// 读取 path 的全部测试字节，返回拥有内容的字符串。
std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}
/// 判断 path 的目录项是否存在，不跟随悬空链接。
bool exists(const std::string& path) {
    struct stat info;
    return ::lstat(path.c_str(), &info) == 0;
}
/// 检查 root 与 /dev/shm 的真实设备是否不同，决定能否执行跨卷验收。
bool second_filesystem(const std::string& root) {
    struct stat a, b;
    return ::stat(root.c_str(), &a) == 0 && ::stat("/dev/shm", &b) == 0 &&
           a.st_dev != b.st_dev;
}
}  // namespace

/// 最小完整用法：准备源、公开调用、结果读取；TempDirectory 回收资源。
TEST(MoveFile, MinimalCompleteUsage) {
    TempDirectory root;
    const auto source = root.path + "/source", target = root.path + "/target";
    // 准备并关闭源文件，移动期间没有活动写入者。
    {
        std::ofstream output(source, std::ios::binary);
        output << "hello";
        ASSERT_TRUE(output.good());
    }
    const auto result = move_file(source, target);
    // 同时断言返回状态和实际路径，离开作用域自动清理测试文件。
    EXPECT_EQ(result.status, MoveStatus::Completed);
    EXPECT_TRUE(result.source_removed);
    EXPECT_FALSE(result.error);
    EXPECT_EQ(read_file(target), "hello");
    EXPECT_FALSE(exists(source));
}

/// 用 volume 决定同卷或真实跨卷，覆盖空/大文件、权限/mtime、目录和链接。
class MoveFileVolumes : public testing::TestWithParam<bool> {};

/// 文件数据、必要元信息和符号链接文本在同卷/跨卷保持；目录不跟随链接。
TEST_P(MoveFileVolumes, FilesTreesAndMetadata) {
    TempDirectory root;
    if (GetParam() && !second_filesystem(root.path))
        GTEST_SKIP() << "second filesystem unavailable";
    TempDirectory dest(GetParam() ? "/dev/shm" : root.path);
    for (const std::size_t size :
        {std::size_t(0), std::size_t(17), std::size_t(2 * 1024 * 1024 + 137)}) {
        const auto source = root.path + "/source",
                   target = dest.path + "/target";
        std::string data(size, '\0');
        for (std::size_t i = 0; i < size; ++i)
            data[i] = static_cast<char>(i % 251);
        write_file(source, data);
        ASSERT_EQ(::chmod(source.c_str(), 0640), 0);
        const timespec times[] = {
            {1600000000, 123456789}, {1600000000, 123456789}};
        ASSERT_EQ(::utimensat(AT_FDCWD, source.c_str(), times, 0), 0);
        EXPECT_EQ(move_file(source, target).status, MoveStatus::Completed);
        EXPECT_EQ(read_file(target), data);
        EXPECT_FALSE(exists(source));
        struct stat info;
        ASSERT_EQ(::stat(target.c_str(), &info), 0);
        EXPECT_EQ(info.st_mode & 0777, 0640);
#if defined(__APPLE__)
        EXPECT_EQ(info.st_mtimespec.tv_nsec, 123456789);
#else
        EXPECT_EQ(info.st_mtim.tv_nsec, 123456789);
#endif
        ASSERT_EQ(::unlink(target.c_str()), 0);
    }
    const auto source = root.path + "/tree",
               target = dest.path + "/tree-target";
    ASSERT_EQ(::mkdir(source.c_str(), 0700), 0);
    ASSERT_EQ(::mkdir((source + "/empty").c_str(), 0700), 0);
    write_file(source + "/data", "tree");
    ASSERT_EQ(
        ::link((source + "/data").c_str(), (source + "/alias").c_str()), 0);
    ASSERT_EQ(::symlink("missing", (source + "/link").c_str()), 0);
    EXPECT_EQ(move_file(source, target).status, MoveStatus::Completed);
    EXPECT_EQ(read_file(target + "/data"), "tree");
    EXPECT_EQ(read_file(target + "/alias"), "tree");
    EXPECT_TRUE(exists(target + "/empty"));
    char link[64];
    const auto n = ::readlink((target + "/link").c_str(), link, sizeof(link));
    ASSERT_GT(n, 0);
    EXPECT_EQ(std::string(link, static_cast<std::size_t>(n)), "missing");
    ASSERT_EQ(::symlink("missing-root", (root.path + "/root-link").c_str()), 0);
    EXPECT_EQ(
        move_file(root.path + "/root-link", dest.path + "/moved-link").status,
        MoveStatus::Completed);
}

/// 默认不覆盖，显式覆盖按原生类型规则工作；非空目录不合并，空目录可替换。
TEST_P(MoveFileVolumes, ConflictAndOverwrite) {
    TempDirectory root;
    if (GetParam() && !second_filesystem(root.path))
        GTEST_SKIP() << "second filesystem unavailable";
    TempDirectory dest(GetParam() ? "/dev/shm" : root.path);
    const auto source = root.path + "/source", target = dest.path + "/target";
    write_file(source, "new");
    ASSERT_EQ(::symlink("missing", target.c_str()), 0);
    EXPECT_THROW(move_file(source, target), std::system_error);
    EXPECT_EQ(read_file(source), "new");
    EXPECT_EQ(move_file(source, target, true).status, MoveStatus::Completed);
    EXPECT_EQ(read_file(target), "new");
    ASSERT_EQ(::unlink(target.c_str()), 0);
    ASSERT_EQ(::mkdir(source.c_str(), 0700), 0);
    ASSERT_EQ(::mkdir(target.c_str(), 0700), 0);
    write_file(target + "/old", "old");
    write_file(source + "/new", "new");
    ASSERT_EQ(::chmod(source.c_str(), 0500), 0);
    EXPECT_THROW(move_file(source, target, true), std::system_error);
    ASSERT_EQ(::chmod(source.c_str(), 0700), 0);
    EXPECT_EQ(read_file(target + "/old"), "old");
    ASSERT_EQ(::unlink((target + "/old").c_str()), 0);
    EXPECT_EQ(move_file(source, target, true).status, MoveStatus::Completed);
}
INSTANTIATE_TEST_SUITE_P(
    SameAndRealCrossVolume, MoveFileVolumes, testing::Bool());

/// 同路径/硬链接/后代、FIFO 及缺失目标父目录均不会导致源丢失。
TEST(MoveFile, RejectsAliasesSpecialAndMissing) {
    TempDirectory root;
    const auto source = root.path + "/source", alias = root.path + "/alias";
    write_file(source, "safe");
    ASSERT_EQ(::link(source.c_str(), alias.c_str()), 0);
    EXPECT_THROW(move_file(source, source, true), std::system_error);
    EXPECT_THROW(move_file(source, alias, true), std::system_error);
    EXPECT_THROW(
        move_file(source, root.path + "/missing/target"), std::system_error);
    EXPECT_EQ(read_file(source), "safe");
    const auto tree = root.path + "/tree";
    ASSERT_EQ(::mkdir(tree.c_str(), 0700), 0);
    EXPECT_THROW(move_file(tree, tree + "/child"), std::system_error);
    const auto fifo = root.path + "/fifo";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    EXPECT_THROW(move_file(fifo, root.path + "/target"), std::system_error);
}

namespace {
/// 故障仅属于单个场景；复用真实 I/O，其余状态不共享。
class FaultIO : public mental1104::file::detail::MoveIO {
public:
    /// source 为测试源，phase 为本次注入阶段。
    FaultIO(std::string source, std::string phase)
        : source_(std::move(source)), phase_(std::move(phase)) {}
    /// src/dst 原子命名；首次模拟跨卷，overwrite 原样传给真实内核调用。
    int rename(const std::string& src, const std::string& dst,
        bool overwrite) const override {
        if (this->phase_ == "alias-race") {
            if (::link(src.c_str(), dst.c_str()) != 0) return -1;
            return MoveIO::rename(src, dst, overwrite);
        }
        if (this->phase_ == "native-sync")
            return MoveIO::rename(src, dst, overwrite);
        if (src == this->source_) {
            errno = this->phase_ == "rename-denied" ? EACCES : EXDEV;
            return -1;
        }
        if (this->phase_ == "race") write_file(dst, "competitor");
        const int result = MoveIO::rename(src, dst, overwrite);
        this->published_ = result == 0;
        return result;
    }
    /// 按阶段拒绝 path 同步，其余保留真实 fsync；返回错误码。
    std::error_code sync_directory(const std::string& path) const override {
        if (this->phase_ == "native-sync")
            return std::error_code(EIO, std::generic_category());
        if (this->published_ &&
            (this->phase_ == "publish-sync" ||
                (this->phase_ == "source-sync" && !exists(this->source_))))
            return std::error_code(EIO, std::generic_category());
        return MoveIO::sync_directory(path);
    }
    /// 删除 source，directory 决定类型；delete 阶段返回权限错误。
    int remove(const std::string& source, bool directory) const override {
        if (this->phase_ == "delete") {
            errno = EACCES;
            return -1;
        }
        return MoveIO::remove(source, directory);
    }
    /// input/output 为借用描述符，expected
    /// 为原始长度；模拟写满、中断或复制后源变化。
    void copy(int input, int output, std::uint64_t expected) const override {
        if (this->phase_ == "copy" || this->phase_ == "interrupt") {
            ASSERT_EQ(::write(output, "partial", 7), 7);
            throw std::system_error(this->phase_ == "copy" ? ENOSPC : EINTR,
                std::generic_category(), "injected copy failure");
        }
        MoveIO::copy(input, output, expected);
        if (this->phase_ == "change")
            write_file(this->source_, "modified-source");
    }

private:
    std::string source_, phase_;
    mutable bool published_ = false;
};
}  // namespace

/// 覆盖复制、发布竞争、源变化、删除和发布前后同步失败，不能把已发布误报为未发布。
TEST(MoveFile, InjectedFailureBoundaries) {
    for (const std::string phase :
        {"copy", "interrupt", "race", "change", "delete", "publish-sync",
            "source-sync", "rename-denied", "alias-race", "native-sync"}) {
        TempDirectory root;
        const auto source = root.path + "/source",
                   target = root.path + "/target";
        write_file(source, "original");
        FaultIO io(source, phase);
        if (phase == "copy" || phase == "interrupt" || phase == "race" ||
            phase == "change" || phase == "rename-denied") {
            EXPECT_THROW(mental1104::file::detail::move_with_io(
                             source, target, false, io),
                std::system_error);
            EXPECT_TRUE(exists(source));
            if (phase == "race")
                EXPECT_EQ(read_file(target), "competitor");
            else
                EXPECT_FALSE(exists(target));
        } else {
            const auto result = mental1104::file::detail::move_with_io(
                source, target, phase == "alias-race", io);
            EXPECT_EQ(result.status, phase == "delete" || phase == "alias-race"
                                         ? MoveStatus::PublishedSourceRemains
                                         : MoveStatus::DurabilityUncertain);
            EXPECT_EQ(result.source_removed,
                phase == "source-sync" || phase == "native-sync");
            EXPECT_TRUE(result.error);
            EXPECT_EQ(read_file(target), "original");
        }
    }
}
/// 删除第一个子项后第二次失败，验证目标完整且源树只清理一部分。
TEST(MoveFile, PartialDirectoryCleanup) {
    /// 本次源根及删除计数仅属于一个场景，不与其他测试共享。
    class PartialIO : public mental1104::file::detail::MoveIO {
    public:
        /// 接收 source 根路径，作为首次 EXDEV 注入条件。
        explicit PartialIO(std::string source) : source_(std::move(source)) {}
        /// src 为源根时模拟跨卷，其他 dst/overwrite 使用真实原子命名。
        int rename(const std::string& src, const std::string& dst,
            bool overwrite) const override {
            if (src == this->source_) {
                errno = EXDEV;
                return -1;
            }
            return MoveIO::rename(src, dst, overwrite);
        }
        /// source 的第一项按 directory 类型真实删除，第二次返回权限错误。
        int remove(const std::string& source, bool directory) const override {
            if (++this->count_ == 2) {
                errno = EACCES;
                return -1;
            }
            return MoveIO::remove(source, directory);
        }

    private:
        std::string source_;
        mutable unsigned count_ = 0;
    };
    TempDirectory root;
    const auto source = root.path + "/tree", target = root.path + "/target";
    ASSERT_EQ(::mkdir(source.c_str(), 0700), 0);
    write_file(source + "/a", "a");
    write_file(source + "/b", "b");
    const auto result = mental1104::file::detail::move_with_io(
        source, target, false, PartialIO(source));
    EXPECT_EQ(result.status, MoveStatus::PublishedSourceRemains);
    EXPECT_FALSE(result.source_removed);
    EXPECT_EQ(read_file(target + "/a"), "a");
    EXPECT_EQ(read_file(target + "/b"), "b");
    EXPECT_NE(exists(source + "/a"), exists(source + "/b"));
}
#else
#include <windows.h>
/// Windows 同卷最小用法：创建源、原生移动、检查持久化受限结果并释放资源。
TEST(MoveFile, MinimalCompleteUsage) {
    char folder[MAX_PATH], source[MAX_PATH];
    ASSERT_GT(GetTempPathA(MAX_PATH, folder), 0u);
    ASSERT_NE(GetTempFileNameA(folder, "m11", 0, source), 0u);
    const std::string target = std::string(source) + "-target";
    {
        std::ofstream output(source);
        output << "hello";
    }
    const auto result = mental1104::file::move_file(source, target);
    EXPECT_EQ(result.status, mental1104::file::MoveStatus::DurabilityUncertain);
    EXPECT_TRUE(result.source_removed);
    std::ifstream input(target);
    std::string content;
    input >> content;
    EXPECT_EQ(content, "hello");
    input.close();
    DeleteFileA(target.c_str());
}
#endif
