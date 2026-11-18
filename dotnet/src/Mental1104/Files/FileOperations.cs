using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

[assembly: InternalsVisibleTo("Mental1104.Tests")]

namespace Mental1104.Files;

/// <summary>目标已发布后的阶段；发布前失败通过异常报告。</summary>
public enum MoveStatus
{
    /// <summary>源已清理且必要的目录同步成功。</summary>
    Completed,
    /// <summary>目标已发布，源树可能只清理了一部分。</summary>
    PublishedSourceRemains,
    /// <summary>命名成功，但目录持久化无法确认。</summary>
    DurabilityUncertain
}

/// <summary>已发布结果；Error 为部分成功原因，SourceRemoved 表示源根已删除。</summary>
/// <param name="Status">最终阶段，持久化失败优先报告。</param>
/// <param name="SourceRemoved">源根目录项是否已删除。</param>
/// <param name="Error">Completed 时为 null，否则为具体原因。</param>
public sealed record MoveResult(MoveStatus Status, bool SourceRemoved, Exception? Error = null);

/// <summary>提供文件、目录与符号链接移动，不合并目录、不跟随链接；无共享可变状态。</summary>
public static class FileOperations
{
    /// <summary>Linux statx 的稳定 ABI 快照；只读取内核已初始化的基本字段。</summary>
    [StructLayout(LayoutKind.Explicit, Size = 256)]
    internal struct Snapshot
    {
        [FieldOffset(0)] internal uint Mask;
        [FieldOffset(28)] internal ushort Mode;
        [FieldOffset(32)] internal ulong Inode;
        [FieldOffset(40)] internal ulong Size;
        [FieldOffset(96)] internal long ChangeSeconds;
        [FieldOffset(104)] internal uint ChangeNanos;
        [FieldOffset(112)] internal long ModifiedSeconds;
        [FieldOffset(120)] internal uint ModifiedNanos;
        [FieldOffset(136)] internal uint DeviceMajor;
        [FieldOffset(140)] internal uint DeviceMinor;
        internal readonly bool Directory => (Mode & 0xf000) == 0x4000;
        internal readonly bool Link => (Mode & 0xf000) == 0xa000;
        internal readonly bool Regular => (Mode & 0xf000) == 0x8000;
    }

    /// <summary>源路径与复制前元信息，清单后序删除以免误删未知新增项。</summary>
    /// <param name="Path">源绝对路径。</param><param name="Info">不跟随链接的原快照。</param>
    private sealed record Entry(string Path, Snapshot Info);

    /// <summary>每次调用的真实 I/O 边界；测试可替换操作而不改变进程全局状态。</summary>
    internal class MoveIo
    {
        /// <summary>原子移动 source 到 target，overwrite 交由内核裁决；失败抛 IOException。</summary>
        internal virtual void Rename(string source, string target, bool overwrite)
        {
            int rc = overwrite ? RenamePosix(source, target) : RenameNoReplace(-100, source, -100, target, 1);
            if (rc != 0) throw NativeError("atomic rename failed");
        }

        /// <summary>同步 path 目录；不支持或同步失败时抛 IOException。</summary>
        internal virtual void SyncDirectory(string path)
        {
            int fd = Open(path, 0x10000 | 0x80000, 0);
            if (fd < 0) throw NativeError("open directory failed");
            using var handle = new SafeFileHandle((IntPtr)fd, true);
            if (Fsync(fd) != 0) throw NativeError("directory sync failed");
        }

        /// <summary>删除 path 的一项；directory 表示真实目录，绝不递归跟随链接。</summary>
        internal virtual void Remove(string path, bool directory)
        {
            if (directory) Directory.Delete(path); else File.Delete(path);
        }

        /// <summary>将 input 分块复制到 output，核验 expected 长度并逐字节读回。</summary>
        internal virtual void Copy(FileStream input, FileStream output, ulong expected)
        {
            byte[] a = new byte[65536], b = new byte[65536];
            ulong count = 0;
            int n;
            while ((n = input.Read(a)) != 0) { output.Write(a, 0, n); count += (uint)n; }
            if (count != expected) throw new IOException("copy size verification failed");
            output.Flush();
            input.Position = output.Position = 0;
            while ((n = ReadBlock(input, a)) != 0)
            {
                int m = ReadBlock(output, b);
                if (n != m || !a.AsSpan(0, n).SequenceEqual(b.AsSpan(0, m)))
                    throw new IOException("copy readback verification failed");
            }
            if (output.ReadByte() != -1) throw new IOException("copy length changed");
        }
    }

    /// <summary>读取 stream 至 buffer 满或 EOF，返回实际字节数；流归调用方所有。</summary>
    private static int ReadBlock(Stream stream, byte[] buffer)
    {
        int count = 0;
        while (count < buffer.Length)
        {
            int n = stream.Read(buffer, count, buffer.Length - count);
            if (n == 0) break;
            count += n;
        }
        return count;
    }

    /// <summary>从 errno 构建 IOException；operation 为 ASCII 上下文，内部原因保留数值错误码。</summary>
    private static IOException NativeError(string operation)
    {
        int code = Marshal.GetLastPInvokeError();
        return new IOException($"{operation} (errno {code})", new Win32Exception(code));
    }

    /// <summary>取得 path 的 statx 基本快照；fd 非负时改为检查该已打开文件。</summary>
    private static Snapshot Stat(string path, int fd = -1)
    {
        if (Statx(fd < 0 ? -100 : fd, fd < 0 ? path : "", fd < 0 ? 0x100 : 0x1000, 0x7ff, out Snapshot info) != 0)
            throw NativeError("stat source failed");
        if ((info.Mask & 0x7c3) != 0x7c3) throw new IOException("required source metadata unavailable");
        return info;
    }

    /// <summary>比较 a/b 的设备、inode 和模式，返回身份是否一致。</summary>
    private static bool Same(Snapshot a, Snapshot b) => a.Inode == b.Inode && a.DeviceMajor == b.DeviceMajor && a.DeviceMinor == b.DeviceMinor && a.Mode == b.Mode;

    /// <summary>复核 entry；identityOnly 用于全树复核后的清理，忽略自身清理造成的时间变化，冲突抛 IOException。</summary>
    private static void Verify(Entry entry, bool identityOnly = false)
    {
        Snapshot now = Stat(entry.Path);
        if (!Same(entry.Info, now) || (!identityOnly && (entry.Info.Size != now.Size || entry.Info.ModifiedSeconds != now.ModifiedSeconds || entry.Info.ModifiedNanos != now.ModifiedNanos || entry.Info.ChangeSeconds != now.ChangeSeconds || entry.Info.ChangeNanos != now.ChangeNanos)))
            throw new IOException("source changed during move");
    }

    /// <summary>将 info 的 mtime 恢复到 target 本身，link 控制不跟随链接；失败抛 IOException。</summary>
    private static void RestoreTime(string target, Snapshot info, bool link)
    {
        var time = new Timespec { Seconds = info.ModifiedSeconds, Nanos = info.ModifiedNanos };
        if (Utimens(-100, target, new[] { time, time }, link ? 0x100 : 0) != 0) throw NativeError("restore modification time failed");
    }

    /// <summary>递归复制 source 到私有 target，entries 追加快照，io 为本次 I/O 边界。</summary>
    private static void CopyTree(string source, string target, List<Entry> entries, MoveIo io)
    {
        if (!OperatingSystem.IsLinux()) throw new PlatformNotSupportedException("copy requires Linux");
        Snapshot info = Stat(source);
        var entry = new Entry(source, info);
        entries.Add(entry);
        if (info.Link)
        {
            string text = new FileInfo(source).LinkTarget ?? throw new IOException("cannot read symbolic link");
            File.CreateSymbolicLink(target, text);
            RestoreTime(target, info, true);
        }
        else if (info.Directory)
        {
            if (Mkdir(target, 0x1c0) != 0) throw NativeError("create temporary directory failed");
            foreach (string child in Directory.EnumerateFileSystemEntries(source).OrderBy(p => p, StringComparer.Ordinal))
                CopyTree(child, Path.Combine(target, Path.GetFileName(child)), entries, io);
            File.SetUnixFileMode(target, (UnixFileMode)(info.Mode & 0x1ff));
            RestoreTime(target, info, false);
            io.SyncDirectory(target);
        }
        else if (info.Regular)
        {
            // O_NOFOLLOW/O_NONBLOCK 防止最后一级替换成链接或 FIFO 后阻塞；祖先目录须由调用方独占。
            int fd = Open(source, 0x20000 | 0x80000 | 0x800, 0);
            if (fd < 0) throw NativeError("open source failed");
            using var handle = new SafeFileHandle((IntPtr)fd, true);
            if (!Same(info, Stat("", fd))) throw new IOException("source replaced before copy");
            using var input = new FileStream(handle, FileAccess.Read);
            using var output = new FileStream(target, new FileStreamOptions { Mode = FileMode.CreateNew, Access = FileAccess.ReadWrite, Share = FileShare.None, UnixCreateMode = UnixFileMode.UserRead | UnixFileMode.UserWrite });
            io.Copy(input, output, info.Size);
            File.SetUnixFileMode(target, (UnixFileMode)(info.Mode & 0x1ff));
            RestoreTime(target, info, false);
            output.Flush(true);
        }
        else throw new IOException("unsupported source type");
        Verify(entry);
    }

    /// <summary>取得 path 的父目录；传入绝对路径，返回非空目录。</summary>
    private static string Parent(string path) => Path.GetDirectoryName(path) ?? throw new IOException("invalid move path");

    /// <summary>识别能报告为移动结果的普通 I/O 故障，error 不含编程错误。</summary>
    private static bool IoFailure(Exception error) => error is IOException or UnauthorizedAccessException;

    /// <summary>执行 source 到 target 的移动，overwrite 控制替换，io 用于逐阶段故障验证。</summary>
    internal static MoveResult MoveWith(string source, string target, bool overwrite, MoveIo io)
    {
        if (OperatingSystem.IsWindows()) return WindowsMove(source, target, overwrite);
        if (!OperatingSystem.IsLinux() || RuntimeInformation.ProcessArchitecture is not (Architecture.X64 or Architecture.Arm64)) throw new PlatformNotSupportedException("POSIX move requires Linux x64/arm64 statx");
        string src = Path.GetFullPath(source), dst = Path.GetFullPath(target);
        Snapshot info = Stat(src);
        if (!info.Regular && !info.Directory && !info.Link) throw new IOException("unsupported source type");
        try
        {
            if (Same(info, Stat(dst))) throw new IOException("source and target are aliases");
        }
        catch (IOException e) when (e.InnerException is Win32Exception { NativeErrorCode: 2 }) { /* 目标不存在，由原子命名最终裁决。 */ }
        if (info.Directory)
        {
            // 遍历真实父路径的身份，即使其中有符号链接也能拒绝移入自身后代。
            string? cursor = Parent(dst);
            while (cursor is not null)
            {
                string resolved = Resolve(cursor);
                if (Same(info, Stat(resolved))) throw new IOException("target is inside source");
                cursor = Path.GetDirectoryName(resolved);
            }
        }
        try { io.Rename(src, dst, overwrite); }
        catch (IOException e) when (e.InnerException is Win32Exception { NativeErrorCode: 18 }) { return CrossMove(src, dst, overwrite, io); }
        // 覆盖 rename 可因竞争者创建源硬链接而成功但保留源，核验后才能报告完成。
        Exception? sourceError = null;
        try { Stat(src); sourceError = new IOException("source remains after rename"); }
        catch (IOException e) when (e.InnerException is Win32Exception { NativeErrorCode: 2 }) { }
        catch (Exception e) when (IoFailure(e)) { sourceError = e; }
        try { io.SyncDirectory(Parent(dst)); io.SyncDirectory(Parent(src)); }
        catch (Exception e) when (IoFailure(e)) { return new(MoveStatus.DurabilityUncertain, sourceError is null, e); }
        if (sourceError is not null) return new(MoveStatus.PublishedSourceRemains, false, sourceError);
        return new(MoveStatus.Completed, true);
    }

    /// <summary>解析 path 的真实路径，返回新字符串并释放 libc 缓冲区；失败抛 IOException。</summary>
    private static string Resolve(string path)
    {
        IntPtr value = Realpath(path, IntPtr.Zero);
        if (value == IntPtr.Zero) throw NativeError("resolve parent failed");
        try { return Marshal.PtrToStringUTF8(value) ?? throw new IOException("resolve parent failed"); }
        finally { Free(value); }
    }

    /// <summary>跨卷复制 src 到 dst；overwrite 控制原子发布，io 保留 I/O 阶段边界。</summary>
    private static MoveResult CrossMove(string src, string dst, bool overwrite, MoveIo io)
    {
        string stage;
        while (true)
        {
            stage = Path.Combine(Parent(dst), ".move-" + Guid.NewGuid().ToString("N"));
            if (Mkdir(stage, 0x1c0) == 0) break;
            if (Marshal.GetLastPInvokeError() != 17) throw NativeError("create private staging directory failed");
        }
        try
        {
            string payload = Path.Combine(stage, "payload");
            var entries = new List<Entry>();
            CopyTree(src, payload, entries, io);
            foreach (Entry entry in entries) Verify(entry);
            io.SyncDirectory(stage);
            io.SyncDirectory(Parent(dst));
            io.Rename(payload, dst, overwrite);
            // 发布后的错误必须成为结果，不能让调用者误以为目标不存在。
            try { Directory.Delete(stage); io.SyncDirectory(Parent(dst)); }
            catch (Exception e) when (IoFailure(e)) { return new(MoveStatus.DurabilityUncertain, false, e); }
            try { foreach (Entry entry in entries) Verify(entry); }
            catch (Exception e) when (IoFailure(e)) { return new(MoveStatus.PublishedSourceRemains, false, e); }
            for (int i = entries.Count - 1; i >= 0; --i)
            {
                Entry entry = entries[i];
                // 全树已复核；删除硬链接兄弟会改变 ctime，此处仅复核身份。
                try { Verify(entry, true); io.Remove(entry.Path, entry.Info.Directory); }
                catch (Exception e) when (IoFailure(e)) { return new(MoveStatus.PublishedSourceRemains, false, e); }
                try { io.SyncDirectory(Parent(entry.Path)); }
                catch (Exception e) when (IoFailure(e)) { return new(MoveStatus.DurabilityUncertain, i == 0, e); }
            }
            return new(MoveStatus.Completed, true);
        }
        finally
        {
            // 只回收本次临时项；清理失败不覆盖原始异常。
            Discard(stage);
        }
    }

    /// <summary>尽力回收本次私有 path；恢复临时目录清理权限，不跟随链接，不修改源或已发布目标。</summary>
    private static void Discard(string path)
    {
        try
        {
            if (!OperatingSystem.IsLinux()) return;
            Snapshot info = Stat(path);
            if (info.Directory) { File.SetUnixFileMode(path, (UnixFileMode)0x1c0); foreach (string child in Directory.GetFileSystemEntries(path)) Discard(child); Directory.Delete(path); }
            else File.Delete(path);
        }
        catch (Exception e) when (IoFailure(e)) { /* 清理错误不能覆盖原始失败。 */ }
    }

    /// <summary>移动普通文件、目录或链接；发布前抛异常，发布后返回明确的部分成功状态。</summary>
    /// <param name="source">已有源路径；调用方必须独占源树及祖先目录。</param>
    /// <param name="target">父目录已存在的最终路径，不合并目录；同路径/硬链接别名拒绝。</param>
    /// <param name="overwrite">默认 false，内核原子拒绝覆盖；true 按 rename 类型规则替换。</param>
    /// <returns>已发布结果；检查 Status、SourceRemoved、Error 后再决定恢复，禁止盲目重试。</returns>
    /// <exception cref="IOException">发布前失败，源和旧目标保持，临时项尽力清理。</exception>
    /// <remarks>Linux x64/arm64 使用 statx，仅保留普通权限、mtime、链接文本；不保证 uid/gid、ACL、xattr、稀疏及硬链接拓扑。
    /// 跨卷非端到端原子，快照检测不能替代排他访问。Windows 仅支持同卷并报告持久化不确定，macOS 明确拒绝。</remarks>
    public static MoveResult MoveFile(string source, string target, bool overwrite = false) => MoveWith(source, target, overwrite, new MoveIo());

    /// <summary>Windows 同卷移动 source 到 target；overwrite 控制原子替换，不让系统自动复制跨卷数据。</summary>
    private static MoveResult WindowsMove(string source, string target, bool overwrite)
    {
        string src = Path.GetFullPath(source), dst = Path.GetFullPath(target);
        WindowsInfo before = WindowsStat(src);
        try
        {
            WindowsInfo other = WindowsStat(dst);
            if (before.Volume == other.Volume && before.IndexHigh == other.IndexHigh && before.IndexLow == other.IndexLow)
                throw new IOException("source and target are aliases");
        }
        catch (IOException e) when (e.InnerException is Win32Exception { NativeErrorCode: 2 or 3 }) { /* 目标不存在，由内核裁决最终冲突。 */ }
        if (!MoveFileEx(src, dst, overwrite ? 1u : 0u))
        {
            if (Marshal.GetLastPInvokeError() == 17) throw new IOException("cross-volume move unsupported on Windows");
            throw NativeError("atomic rename failed");
        }
        return new(MoveStatus.DurabilityUncertain, true, new IOException("directory sync unsupported on Windows"));
    }

    /// <summary>不跟随 path 的 reparse point，返回卷/文件 ID；设备句柄明确拒绝。</summary>
    private static WindowsInfo WindowsStat(string path)
    {
        using SafeFileHandle handle = CreateFile(path, 0, 7, IntPtr.Zero, 3, 0x02200000, IntPtr.Zero);
        if (handle.IsInvalid) throw NativeError("open source failed");
        if (GetFileType(handle) != 1) throw new IOException("unsupported source type");
        if (!GetFileInformation(handle, out WindowsInfo info)) throw NativeError("stat source failed");
        return info;
    }

    /// <summary>Windows BY_HANDLE_FILE_INFORMATION ABI，使用文件 ID 检查硬链接别名。</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct WindowsInfo
    {
        internal uint Attributes;
        internal System.Runtime.InteropServices.ComTypes.FILETIME Creation, Access, Write;
        internal uint Volume, SizeHigh, SizeLow, Links, IndexHigh, IndexLow;
    }

    /// <summary>打开 path 的磁盘目录项；access/share/creation/flags 是 Win32 原始标记，返回拥有的句柄。</summary>
    /// <param name="path">UTF-16 路径。</param><param name="access">访问位。</param><param name="share">共享位。</param>
    /// <param name="security">安全描述符，零表示默认。</param><param name="creation">打开已有项。</param>
    /// <param name="flags">不跟随 reparse point，支持目录。</param><param name="template">模板句柄，零表示无。</param>
    /// <returns>由 SafeFileHandle 关闭的句柄，IsInvalid 表示失败。</returns>
    [DllImport("kernel32.dll", EntryPoint = "CreateFileW", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    /// <summary>返回 handle 的 Win32 类型，磁盘类型为 1，句柄仍由调用方拥有。</summary>
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint GetFileType(SafeFileHandle handle);
    /// <summary>读取 handle 的身份到 info，返回系统成功标记。</summary>
    [DllImport("kernel32.dll", EntryPoint = "GetFileInformationByHandle", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetFileInformation(SafeFileHandle handle, out WindowsInfo info);
    /// <summary>按 flags 将 source 原子命名为 target，返回系统成功标记。</summary>
    [DllImport("kernel32.dll", EntryPoint = "MoveFileExW", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool MoveFileEx(string source, string target, uint flags);

    /// <summary>POSIX 纳秒时间 ABI，由 Linux 64 位 time_t 平台使用。</summary>
    [StructLayout(LayoutKind.Sequential)]
    private struct Timespec { internal long Seconds; internal long Nanos; }

    /// <summary>取得 path 或 fd 的 statx 快照，flags 控制链接语义，mask 为所需字段。</summary>
    [DllImport("libc", EntryPoint = "statx", SetLastError = true)]
    private static extern int Statx(int fd, string path, int flags, uint mask, out Snapshot info);
    /// <summary>将 source 排他命名为 target，两个 fd 为 AT_FDCWD，flags 为 NOREPLACE。</summary>
    [DllImport("libc", EntryPoint = "renameat2", SetLastError = true)]
    private static extern int RenameNoReplace(int sourceFd, string source, int targetFd, string target, uint flags);
    /// <summary>将 source 原子替换为 target，返回原始 POSIX 结果。</summary>
    [DllImport("libc", EntryPoint = "rename", SetLastError = true)]
    private static extern int RenamePosix(string source, string target);
    /// <summary>按 flags/mode 打开 path，返回描述符或 -1。</summary>
    [DllImport("libc", EntryPoint = "open", SetLastError = true)]
    private static extern int Open(string path, int flags, uint mode);
    /// <summary>同步 fd 的数据/目录项，返回原始 POSIX 结果。</summary>
    [DllImport("libc", EntryPoint = "fsync", SetLastError = true)]
    private static extern int Fsync(int fd);
    /// <summary>以 mode 独占创建 path 目录，返回原始 POSIX 结果。</summary>
    [DllImport("libc", EntryPoint = "mkdir", SetLastError = true)]
    private static extern int Mkdir(string path, uint mode);
    /// <summary>按 flags 将 times 应用于 fd/path，返回原始 POSIX 结果。</summary>
    [DllImport("libc", EntryPoint = "utimensat", SetLastError = true)]
    private static extern int Utimens(int fd, string path, Timespec[] times, int flags);
    /// <summary>解析 path；buffer 为零时分配新缓冲区，由调用方 free。</summary>
    [DllImport("libc", EntryPoint = "realpath", SetLastError = true)]
    private static extern IntPtr Realpath(string path, IntPtr buffer);
    /// <summary>释放 realpath 返回的 pointer；不接管其他资源。</summary>
    [DllImport("libc", EntryPoint = "free")]
    private static extern void Free(IntPtr pointer);
}
