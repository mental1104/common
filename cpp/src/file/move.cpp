#include "mental1104/file/move.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "mental1104/c_api_raii.h"
#include "move_io.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#include <unistd.h>
#endif

namespace mental1104 {
namespace file {
namespace {

/// 将 code 转换为带 operation 的系统异常；发布后仅在局部捕获边界内使用。
/// @param code 系统错误值。@param operation ASCII 操作名称。
void fail(int code, const char* operation) {
    throw std::system_error(code, std::generic_category(), operation);
}

#if !defined(_WIN32)
/// 清单项保存源路径及复制前的 lstat；不记录所有权以免扩大元数据承诺。
struct Entry {
    std::string path;
    struct stat info;
};

/// 取得 path 的父目录，裸文件名使用当前目录。
/// @param path 无尾分隔符的路径。@return 非空父目录。
std::string parent(const std::string& path) {
    const std::size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? "."
                                    : (pos == 0 ? "/" : path.substr(0, pos));
}

/// 拒绝根、空路径及尾部点段；避免 rename 与 lstat 对尾分隔符解释不同。
/// @param path 调用方路径。@return 去掉普通尾分隔符的路径。
std::string normalized(std::string path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        fail(EINVAL, "invalid move path");
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    const std::string name = path.substr(path.find_last_of('/') + 1);
    if (path == "/" || name == "." || name == "..")
        fail(EINVAL, "invalid move path");
    return path;
}

/// 不跟随 path 的最终链接读取元信息。
/// @param path 已存在路径。@return lstat 快照，失败抛系统异常。
struct stat metadata(const std::string& path) {
    struct stat value;
    if (::lstat(path.c_str(), &value) != 0) fail(errno, "lstat source");
    return value;
}

/// 比较两个快照代表的设备、inode 及类型/权限。
/// @param a 原快照。@param b 当前快照。@return 身份和模式是否一致。
bool identity(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
           a.st_mode == b.st_mode;
}

/// 比较 path 与 before；目录删除子项后仅比较身份。
/// @param path 源路径。@param before 原快照。@param identity_only
/// 忽略大小和时间。
void unchanged(const std::string& path, const struct stat& before,
    bool identity_only = false) {
    const struct stat after = metadata(path);
#if defined(__APPLE__)
    const bool times =
        before.st_mtimespec.tv_sec == after.st_mtimespec.tv_sec &&
        before.st_mtimespec.tv_nsec == after.st_mtimespec.tv_nsec &&
        before.st_ctimespec.tv_sec == after.st_ctimespec.tv_sec &&
        before.st_ctimespec.tv_nsec == after.st_ctimespec.tv_nsec;
#else
    const bool times = before.st_mtim.tv_sec == after.st_mtim.tv_sec &&
                       before.st_mtim.tv_nsec == after.st_mtim.tv_nsec &&
                       before.st_ctim.tv_sec == after.st_ctim.tv_sec &&
                       before.st_ctim.tv_nsec == after.st_ctim.tv_nsec;
#endif
    if (!identity(before, after) ||
        (!identity_only && (before.st_size != after.st_size || !times)))
        fail(EBUSY, "source changed during move");
}

/// 同步已存在的 path 目录；无法确认时保留具体系统错误。
/// @param path 目录路径。@return 成功为空，失败为系统错误。
std::error_code sync_dir(const std::string& path) {
    unique_fd fd(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (!fd) return std::error_code(errno, std::generic_category());
    int rc;
    do {
        rc = ::fsync(fd.get());
    } while (rc < 0 && errno == EINTR);
    return rc == 0 ? std::error_code()
                   : std::error_code(errno, std::generic_category());
}

/// 执行原生原子命名；不以存在性预检查代替排他保证。
/// @param source 当前项。@param target 最终项。@param overwrite 是否允许替换。
/// @return 0 成功，-1 失败并设置 errno；不支持排他能力时保持源不变。
int rename_native(
    const std::string& source, const std::string& target, bool overwrite) {
    if (overwrite) return ::rename(source.c_str(), target.c_str());
#if defined(__linux__) && defined(SYS_renameat2)
    return static_cast<int>(::syscall(
        SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, target.c_str(), 1));
#elif defined(__APPLE__)
    return ::renamex_np(source.c_str(), target.c_str(), RENAME_EXCL);
#else
    errno = ENOTSUP;
    return -1;
#endif
}

/// 枚举目录的直接子项，跳过点段；错误不伪装为空目录。
/// @param path 可读目录。@return 排序的文件名列表。
std::vector<std::string> children(const std::string& path) {
    DIR* raw = ::opendir(path.c_str());
    if (!raw) fail(errno, "open directory");
    // 目录流仅借用到作用域结束，枚举失败仍保证关闭。
    auto guard = make_scope_exit([raw]() { ::closedir(raw); });
    std::vector<std::string> result;
    for (;;) {
        errno = 0;
        const dirent* item = ::readdir(raw);
        if (!item) {
            if (errno) fail(errno, "read directory");
            break;
        }
        const std::string name(item->d_name);
        if (name != "." && name != "..") result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

/// 尽力回收本次私有临时树；不跟随符号链接、不传播异常。
/// @param path 私有区域中的路径，调用方独占该区域。
void discard(const std::string& path) noexcept {
    try {
        const struct stat info = metadata(path);
        if (S_ISDIR(info.st_mode)) {
            ::chmod(path.c_str(), 0700);
            for (const auto& name : children(path)) discard(path + "/" + name);
            ::rmdir(path.c_str());
        } else {
            ::unlink(path.c_str());
        }
    } catch (...) { /* 清理失败不覆盖原始发布前错误。 */
    }
}

/// 在 EINTR 后重试，读取至缓冲区满或 EOF，以免短读导致误判。
/// @param fd 借用描述符。@param data 输出字节。@param size 缓冲区容量。
/// @return 实际字节数，0 为 EOF；错误抛系统异常。
std::size_t read_block(int fd, char* data, std::size_t size) {
    std::size_t count = 0;
    while (count < size) {
        const ssize_t n = ::read(fd, data + count, size - count);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) fail(errno, "read source");
        if (n == 0) break;
        count += static_cast<std::size_t>(n);
    }
    return count;
}

/// 将整个缓冲区写入 fd，处理短写、中断和零进展。
/// @param fd 借用输出描述符。@param data 输入字节。@param size 总字节数。
void write_all(int fd, const char* data, std::size_t size) {
    while (size) {
        const ssize_t n = ::write(fd, data, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) fail(n == 0 ? EIO : errno, "write temporary file");
        data += n;
        size -= static_cast<std::size_t>(n);
    }
}

/// 恢复必要时间，不复制所有者及特权位。
/// @param target 临时目标。@param info 原始时间。@param symlink
/// 是否仅修改链接本身。
void restore_times(
    const std::string& target, const struct stat& info, bool symlink) {
#if defined(__APPLE__)
    const timespec times[] = {info.st_atimespec, info.st_mtimespec};
#else
    const timespec times[] = {info.st_atim, info.st_mtim};
#endif
    if (::utimensat(AT_FDCWD, target.c_str(), times,
            symlink ? AT_SYMLINK_NOFOLLOW : 0) != 0)
        fail(errno, "restore modification time");
}

/// 复制普通文件并逐字节读回校验；临时项在私有父目录中独占创建。
/// @param source 源路径。@param target 临时目标。@param before 源快照。
void copy_regular(const std::string& source, const std::string& target,
    const struct stat& before, const detail::MoveIO& io) {
    unique_fd input(
        ::open(source.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    if (!input) fail(errno, "open source");
    struct stat opened;
    if (::fstat(input.get(), &opened) != 0) fail(errno, "stat open source");
    if (!identity(before, opened)) fail(EBUSY, "source replaced before copy");
    unique_fd output(
        ::open(target.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
    if (!output) fail(errno, "create temporary file");
    io.copy(
        input.get(), output.get(), static_cast<std::uint64_t>(before.st_size));
    if (::fchmod(output.get(), before.st_mode & 0777) != 0)
        fail(errno, "restore permissions");
    restore_times(target, before, false);
    if (::fsync(output.get()) != 0) fail(errno, "sync temporary file");
    if (::close(output.release()) != 0) fail(errno, "close temporary file");
}

/// 分块复制并读回核验借用的 input/output；expected 为复制前的长度。
/// @throws std::system_error 读写或校验失败，发布前保留源。
void copy_bytes(int input, int output, std::uint64_t expected) {
    std::array<char, 65536> a;
    std::array<char, 65536> b;
    std::uint64_t total = 0;
    for (;;) {
        const std::size_t n = read_block(input, a.data(), a.size());
        if (!n) break;
        write_all(output, a.data(), n);
        total += n;
    }
    if (total != expected) fail(EIO, "copy size verification failed");
    if (::lseek(input, 0, SEEK_SET) < 0 || ::lseek(output, 0, SEEK_SET) < 0)
        fail(errno, "seek verification");
    for (;;) {
        const std::size_t n = read_block(input, a.data(), a.size());
        const std::size_t m = read_block(output, b.data(), b.size());
        if (n != m || std::memcmp(a.data(), b.data(), n))
            fail(EIO, "copy readback verification failed");
        if (!n) break;
    }
}

/// 递归复制源树；链接只复制文本，特殊类型在发布前拒绝。
/// @param source 源项。@param target 私有目标。@param entries
/// 本次源清单，追加快照。
void copy_tree(const std::string& source, const std::string& target,
    std::vector<Entry>& entries, const detail::MoveIO& io) {
    const struct stat info = metadata(source);
    entries.push_back(Entry{source, info});
    if (S_ISREG(info.st_mode)) {
        copy_regular(source, target, info, io);
    } else if (S_ISLNK(info.st_mode)) {
        std::vector<char> text(256);
        for (;;) {
            const ssize_t n =
                ::readlink(source.c_str(), text.data(), text.size());
            if (n < 0) fail(errno, "read symbolic link");
            if (static_cast<std::size_t>(n) < text.size()) {
                const std::string link(
                    text.data(), static_cast<std::size_t>(n));
                if (::symlink(link.c_str(), target.c_str()) != 0)
                    fail(errno, "create symbolic link");
                break;
            }
            text.resize(text.size() * 2);
        }
        restore_times(target, info, true);
    } else if (S_ISDIR(info.st_mode)) {
        if (::mkdir(target.c_str(), 0700) != 0)
            fail(errno, "create temporary directory");
        unique_fd directory(
            ::open(target.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (!directory) fail(errno, "open temporary directory");
        for (const auto& name : children(source))
            copy_tree(source + "/" + name, target + "/" + name, entries, io);
        if (::fchmod(directory.get(), info.st_mode & 0777) != 0)
            fail(errno, "restore directory permissions");
        restore_times(target, info, false);
        if (::fsync(directory.get()) != 0)
            fail(errno, "sync temporary directory");
    } else {
        fail(ENOTSUP, "unsupported source type");
    }
    unchanged(source, info);
}

/// 复核 entries 的所有项；无法证明没有并发修改，因此调用方仍须独占源。
/// @param entries 复制时保存的只读清单。
void verify_tree(const std::vector<Entry>& entries) {
    for (const auto& entry : entries) unchanged(entry.path, entry.info);
}

/// 检查 target 父目录的真实祖先，拒绝移动目录到自身后代（包括符号链接别名）。
/// @param source 源根快照。@param target 最终目标路径。
void reject_descendant(const struct stat& source, const std::string& target) {
    if (!S_ISDIR(source.st_mode)) return;
    char* resolved = ::realpath(parent(target).c_str(), NULL);
    if (!resolved) fail(errno, "resolve target parent");
    const std::string real(resolved);
    std::free(resolved);
    std::string cursor = real;
    for (;;) {
        const struct stat ancestor = metadata(cursor);
        if (source.st_dev == ancestor.st_dev &&
            source.st_ino == ancestor.st_ino)
            fail(EINVAL, "target is inside source");
        if (cursor == "/") break;
        cursor = parent(cursor);
    }
}

/// 清理已经发布的源清单；每次删除后同步父目录，失败保留阶段信息。
/// @param entries 复制清单，后序处理。@return 完成、源残留或持久化不确定。
MoveResult remove_source(
    const std::vector<Entry>& entries, const detail::MoveIO& io) {
    try {
        verify_tree(entries);
    } catch (const std::system_error& e) {
        return {MoveStatus::PublishedSourceRemains, false, e.code()};
    }
    bool removed = false;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        try {
            const bool directory = S_ISDIR(it->info.st_mode);
            // 全树已经复核；删除硬链接兄弟会改变同 inode 的
            // ctime，此处只复核身份。
            unchanged(it->path, it->info, true);
            const int rc = io.remove(it->path, directory);
            if (rc != 0) fail(errno, "remove source");
            removed = it + 1 == entries.rend();
        } catch (const std::system_error& e) {
            return {MoveStatus::PublishedSourceRemains, false, e.code()};
        }
        const auto error = io.sync_directory(parent(it->path));
        if (error) return {MoveStatus::DurabilityUncertain, removed, error};
    }
    return {MoveStatus::Completed, true, {}};
}
#else
/// 将 UTF-8 路径转换为 Windows 宽字符串。
/// @param path UTF-8 路径。@return 拥有字符缓冲区；非法编码抛异常。
std::wstring wide(const std::string& path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        fail(EINVAL, "invalid move path");
    int n = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, NULL, 0);
    if (!n) fail(EINVAL, "invalid UTF-8 path");
    std::vector<wchar_t> buffer(static_cast<std::size_t>(n));
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, buffer.data(), n);
    return std::wstring(buffer.data());
}

/// 通过不跟随链接的句柄获取路径身份，并拒绝设备句柄。
/// @param path 宽路径。@return 卷与文件 ID 信息；失败抛系统异常。
BY_HANDLE_FILE_INFORMATION windows_info(const std::wstring& path) {
    HANDLE handle = CreateFileW(path.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::system_error(static_cast<int>(GetLastError()),
            std::system_category(), "open source");
    // 句柄仅在本次身份检查期间持有，所有异常路径均关闭。
    auto guard = make_scope_exit([handle]() { CloseHandle(handle); });
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileType(handle) != FILE_TYPE_DISK)
        fail(ENOTSUP, "unsupported source type");
    if (!GetFileInformationByHandle(handle, &info))
        throw std::system_error(static_cast<int>(GetLastError()),
            std::system_category(), "stat source");
    return info;
}
#endif
}  // namespace

MoveResult move_file(
    const std::string& source, const std::string& target, bool overwrite) {
#if defined(_WIN32)
    const auto src = wide(source);
    const auto dst = wide(target);
    const auto before = windows_info(src);
    try {
        const auto other = windows_info(dst);
        if (before.dwVolumeSerialNumber == other.dwVolumeSerialNumber &&
            before.nFileIndexHigh == other.nFileIndexHigh &&
            before.nFileIndexLow == other.nFileIndexLow)
            fail(EINVAL, "source and target are aliases");
    } catch (const std::system_error& e) {
        if (e.code() !=
                std::error_code(ERROR_FILE_NOT_FOUND, std::system_category()) &&
            e.code() !=
                std::error_code(ERROR_PATH_NOT_FOUND, std::system_category()))
            throw;
    }
    if (!MoveFileExW(src.c_str(), dst.c_str(),
            overwrite ? MOVEFILE_REPLACE_EXISTING : 0)) {
        const DWORD error = GetLastError();
        if (error == ERROR_NOT_SAME_DEVICE)
            fail(ENOTSUP, "cross-volume move unsupported on Windows");
        throw std::system_error(static_cast<int>(error), std::system_category(),
            "atomic rename failed");
    }
    return {MoveStatus::DurabilityUncertain, true,
        std::make_error_code(std::errc::operation_not_supported)};
#else
    return detail::move_with_io(source, target, overwrite, detail::MoveIO());
#endif
}

#if !defined(_WIN32)
namespace detail {
int MoveIO::rename(const std::string& source, const std::string& target,
    bool overwrite) const {
    return rename_native(source, target, overwrite);
}
std::error_code MoveIO::sync_directory(const std::string& path) const {
    return sync_dir(path);
}
int MoveIO::remove(const std::string& source, bool directory) const {
    return directory ? ::rmdir(source.c_str()) : ::unlink(source.c_str());
}
void MoveIO::copy(int input, int output, std::uint64_t expected) const {
    copy_bytes(input, output, expected);
}

MoveResult move_with_io(const std::string& source, const std::string& target,
    bool overwrite, const MoveIO& io) {
    const std::string src = normalized(source);
    const std::string dst = normalized(target);
    const struct stat before = metadata(src);
    if (!S_ISREG(before.st_mode) && !S_ISLNK(before.st_mode) &&
        !S_ISDIR(before.st_mode))
        fail(ENOTSUP, "unsupported source type");
    struct stat other;
    if (::lstat(dst.c_str(), &other) == 0) {
        if (before.st_dev == other.st_dev && before.st_ino == other.st_ino)
            fail(EINVAL, "source and target are aliases");
    } else if (errno != ENOENT)
        fail(errno, "stat target");
    reject_descendant(before, dst);
    if (io.rename(src, dst, overwrite) == 0) {
        // 竞争者若把目标改成源硬链接，覆盖 rename
        // 可成功但不移除源；必须复核结果。
        struct stat remaining;
        const int source_check = ::lstat(src.c_str(), &remaining);
        const bool removed = source_check != 0 && errno == ENOENT;
        const auto source_error =
            removed ? std::error_code()
                    : std::error_code(source_check == 0 ? EBUSY : errno,
                          std::generic_category());
        const auto target_error = io.sync_directory(parent(dst));
        const auto source_sync_error = io.sync_directory(parent(src));
        const auto error = target_error ? target_error : source_sync_error;
        if (error) return {MoveStatus::DurabilityUncertain, removed, error};
        return {removed ? MoveStatus::Completed
                        : MoveStatus::PublishedSourceRemains,
            removed, source_error};
    }
    if (errno != EXDEV) fail(errno, "atomic rename failed");
    std::string pattern = parent(dst) + "/.move-XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (!::mkdtemp(buffer.data()))
        fail(errno, "create private staging directory");
    const std::string stage(buffer.data());
    // 发布前失败只回收本次临时树，发布后不回滚目标。
    auto cleanup = make_scope_exit([&stage]() { discard(stage); });
    const std::string payload = stage + "/payload";
    std::vector<Entry> entries;
    copy_tree(src, payload, entries, io);
    verify_tree(entries);
    auto error = io.sync_directory(stage);
    if (!error) error = io.sync_directory(parent(dst));
    if (error) throw std::system_error(error, "sync before publication");
    if (io.rename(payload, dst, overwrite) != 0) fail(errno, "publish target");
    if (::rmdir(stage.c_str()) != 0)
        return {MoveStatus::DurabilityUncertain, false,
            std::error_code(errno, std::generic_category())};
    cleanup.dismiss();
    error = io.sync_directory(parent(dst));
    if (error) return {MoveStatus::DurabilityUncertain, false, error};
    return remove_source(entries, io);
}
}  // namespace detail
#endif
}  // namespace file
}  // namespace mental1104
