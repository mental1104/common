using System;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using Mental1104.Files;
using Xunit;

namespace Mental1104.Tests;

/// <summary>只在 Linux 执行完整移动契约，其余平台明确显示为跳过。</summary>
public sealed class LinuxMoveTheoryAttribute : TheoryAttribute
{
    /// <summary>按真实运行平台设置跳过原因，无测试 I/O 副作用。</summary>
    public LinuxMoveTheoryAttribute() { if (!OperatingSystem.IsLinux()) Skip = "Linux statx move suite"; }
}

/// <summary>仅在 Linux 执行单个 POSIX 契约测试，其余平台明确跳过。</summary>
public sealed class LinuxMoveFactAttribute : FactAttribute
{
    /// <summary>根据运行平台设置跳过原因，不执行测试 I/O。</summary>
    public LinuxMoveFactAttribute() { if (!OperatingSystem.IsLinux()) Skip = "Linux statx move suite"; }
}

/// <summary>验证公开移动、真实跨卷、冲突和部分成功的调用方行为。</summary>
public sealed class MoveFileTests
{
    /// <summary>最小完整用法：准备资源、公开调用、核验结果，using 自动回收目录。</summary>
    [Fact]
    public void MinimalCompleteUsage()
    {
        using var root = new Root();
        string source = Path.Combine(root.Path, "source"), target = Path.Combine(root.Path, "target");
        // 源写入后关闭，调用者不在移动期间持有写入句柄。
        File.WriteAllText(source, "hello");
        if (!OperatingSystem.IsLinux() && !OperatingSystem.IsWindows())
        {
            Assert.Throws<PlatformNotSupportedException>(() => FileOperations.MoveFile(source, target));
            Assert.Equal("hello", File.ReadAllText(source));
            return;
        }
        MoveResult result = FileOperations.MoveFile(source, target);
        // 同时核验状态、目标内容与源清理；using 结束回收全部测试文件。
        Assert.Equal(OperatingSystem.IsWindows() ? MoveStatus.DurabilityUncertain : MoveStatus.Completed, result.Status);
        Assert.True(result.SourceRemoved);
        if (OperatingSystem.IsLinux()) Assert.Null(result.Error);
        Assert.Equal("hello", File.ReadAllText(target)); Assert.False(File.Exists(source));
    }

    /// <summary>cross 控制真实卷模式；验证空/大文件、普通权限、mtime、目录和悬空链接。</summary>
    [LinuxMoveTheory]
    [InlineData(false)]
    [InlineData(true)]
    public void FilesTreesAndMetadata(bool cross)
    {
        using var root = new Root(); using var dest = new Root(cross ? "/dev/shm" : root.Path);
        foreach (int size in new[] { 0, 17, 2 * 1024 * 1024 + 137 })
        {
            string source = Path.Combine(root.Path, "source"), target = Path.Combine(dest.Path, "target");
            byte[] data = Enumerable.Range(0, size).Select(n => (byte)(n % 251)).ToArray();
            File.WriteAllBytes(source, data);
            var stamp = new DateTime(2020, 9, 13, 12, 26, 40, DateTimeKind.Utc).AddTicks(1234567);
            File.SetLastWriteTimeUtc(source, stamp);
            if (OperatingSystem.IsLinux()) File.SetUnixFileMode(source, (UnixFileMode)0x1a0);
            ProbeCross(source, target, cross);
            Assert.Equal(MoveStatus.Completed, FileOperations.MoveFile(source, target).Status);
            Assert.Equal(data, File.ReadAllBytes(target)); Assert.False(File.Exists(source));
            Assert.Equal(stamp, File.GetLastWriteTimeUtc(target));
            if (OperatingSystem.IsLinux()) Assert.Equal((UnixFileMode)0x1a0, File.GetUnixFileMode(target));
            File.Delete(target);
        }
        string tree = Path.Combine(root.Path, "tree"), moved = Path.Combine(dest.Path, "tree-target");
        Directory.CreateDirectory(Path.Combine(tree, "empty")); File.WriteAllText(Path.Combine(tree, "data"), "tree");
        Assert.Equal(0, Link(Path.Combine(tree, "data"), Path.Combine(tree, "alias")));
        File.CreateSymbolicLink(Path.Combine(tree, "link"), "missing");
        Assert.Equal(MoveStatus.Completed, FileOperations.MoveFile(tree, moved).Status);
        Assert.True(Directory.Exists(Path.Combine(moved, "empty")));
        Assert.Equal("tree", File.ReadAllText(Path.Combine(moved, "data")));
        Assert.Equal("missing", new FileInfo(Path.Combine(moved, "link")).LinkTarget);
        File.CreateSymbolicLink(Path.Combine(root.Path, "root-link"), "missing-root");
        Assert.Equal(MoveStatus.Completed, FileOperations.MoveFile(Path.Combine(root.Path, "root-link"), Path.Combine(dest.Path, "moved-link")).Status);
    }

    /// <summary>cross 控制卷模式；默认不覆盖链接，显式覆盖才替换，目录不合并。</summary>
    [LinuxMoveTheory]
    [InlineData(false)]
    [InlineData(true)]
    public void ConflictsAndOverwrite(bool cross)
    {
        using var root = new Root(); using var dest = new Root(cross ? "/dev/shm" : root.Path);
        string source = Path.Combine(root.Path, "source"), target = Path.Combine(dest.Path, "target");
        File.WriteAllText(source, "new"); File.CreateSymbolicLink(target, "missing");
        Assert.Throws<IOException>(() => FileOperations.MoveFile(source, target));
        Assert.Equal("new", File.ReadAllText(source)); Assert.Equal("missing", new FileInfo(target).LinkTarget);
        Assert.Equal(MoveStatus.Completed, FileOperations.MoveFile(source, target, true).Status);
        Assert.Equal("new", File.ReadAllText(target)); File.Delete(target);
        Directory.CreateDirectory(source); Directory.CreateDirectory(target); File.WriteAllText(Path.Combine(target, "old"), "old");
        File.WriteAllText(Path.Combine(source, "new"), "new");
        if (OperatingSystem.IsLinux()) File.SetUnixFileMode(source, (UnixFileMode)0x140);
        try { Assert.Throws<IOException>(() => FileOperations.MoveFile(source, target, true)); }
        finally { if (OperatingSystem.IsLinux()) File.SetUnixFileMode(source, (UnixFileMode)0x1c0); }
        Assert.Empty(Directory.GetDirectories(dest.Path, ".move-*"));
        Assert.Equal("old", File.ReadAllText(Path.Combine(target, "old")));
        File.Delete(Path.Combine(target, "old"));
        Assert.Equal(MoveStatus.Completed, FileOperations.MoveFile(source, target, true).Status);
    }

    /// <summary>同路径、硬链接、后代、FIFO 和缺失父目录均拒绝。</summary>
    [LinuxMoveFact]
    public void AliasesSpecialAndMissing()
    {
        using var root = new Root(); string source = Path.Combine(root.Path, "source"), alias = Path.Combine(root.Path, "alias");
        File.WriteAllText(source, "safe"); Assert.Equal(0, Link(source, alias));
        foreach (string target in new[] { source, alias, Path.Combine(root.Path, "missing/target") })
            Assert.Throws<IOException>(() => FileOperations.MoveFile(source, target, true));
        string tree = Path.Combine(root.Path, "tree"); Directory.CreateDirectory(tree);
        Assert.Throws<IOException>(() => FileOperations.MoveFile(tree, Path.Combine(tree, "child")));
        string fifo = Path.Combine(root.Path, "fifo"); Assert.Equal(0, Mkfifo(fifo, 0x180));
        Assert.Throws<IOException>(() => FileOperations.MoveFile(fifo, Path.Combine(root.Path, "target")));
        Assert.Equal("safe", File.ReadAllText(source));
    }

    /// <summary>phase 指定故障边界；验证发布前保留源/旧目标，发布后只报告部分成功。</summary>
    [LinuxMoveTheory]
    [InlineData("copy")]
    [InlineData("interrupt")]
    [InlineData("race")]
    [InlineData("change")]
    [InlineData("delete")]
    [InlineData("publish-sync")]
    [InlineData("source-sync")]
    [InlineData("rename-denied")]
    [InlineData("alias-race")]
    [InlineData("native-sync")]
    public void InjectedBoundaries(string phase)
    {
        using var root = new Root(); string source = Path.Combine(root.Path, "source"), target = Path.Combine(root.Path, "target");
        File.WriteAllText(source, "original"); var io = new FaultIo(source, phase);
        if (phase is "copy" or "interrupt" or "race" or "change" or "rename-denied")
        {
            Assert.Throws<IOException>(() => FileOperations.MoveWith(source, target, false, io));
            Assert.True(File.Exists(source));
            if (phase == "race") Assert.Equal("competitor", File.ReadAllText(target)); else Assert.False(File.Exists(target));
        }
        else
        {
            MoveResult result = FileOperations.MoveWith(source, target, phase == "alias-race", io);
            Assert.Equal(phase is "delete" or "alias-race" ? MoveStatus.PublishedSourceRemains : MoveStatus.DurabilityUncertain, result.Status);
            Assert.Equal(phase is "source-sync" or "native-sync", result.SourceRemoved); Assert.NotNull(result.Error);
            Assert.Equal("original", File.ReadAllText(target));
        }
        Assert.Empty(Directory.GetDirectories(root.Path, ".move-*"));
    }

    /// <summary>对 source/target 的独立原生命名确认 cross=true 确实产生 EXDEV，而非模拟跨卷。</summary>
    private static void ProbeCross(string source, string target, bool cross)
    {
        if (!cross) return;
        IOException error = Assert.Throws<IOException>(() => new FileOperations.MoveIo().Rename(source, target, false));
        Assert.Equal(18, Assert.IsType<Win32Exception>(error.InnerException).NativeErrorCode);
    }

    /// <summary>真实删除一个子项后失败，目标完整、源树部分残留。</summary>
    [LinuxMoveFact]
    public void PartialDirectoryCleanup()
    {
        using var root = new Root(); string source = Path.Combine(root.Path, "tree"), target = Path.Combine(root.Path, "target");
        Directory.CreateDirectory(source); File.WriteAllText(Path.Combine(source, "a"), "a"); File.WriteAllText(Path.Combine(source, "b"), "b");
        MoveResult result = FileOperations.MoveWith(source, target, false, new PartialIo(source));
        Assert.Equal(MoveStatus.PublishedSourceRemains, result.Status); Assert.False(result.SourceRemoved);
        Assert.Equal("a", File.ReadAllText(Path.Combine(target, "a"))); Assert.Equal("b", File.ReadAllText(Path.Combine(target, "b")));
        Assert.Single(Directory.GetFiles(source));
    }

    /// <summary>source 为本次源根，第二次删除时模拟权限失败。</summary>
    private sealed class PartialIo(string source) : FileOperations.MoveIo
    {
        private int count;
        /// <summary>src 为源根时注入 EXDEV，其余 dst/overwrite 使用真实发布。</summary>
        internal override void Rename(string src, string dst, bool overwrite) { if (src == source) throw new IOException("injected cross-device rename", new Win32Exception(18)); base.Rename(src, dst, overwrite); }
        /// <summary>第一次按 directory 类型删除 path，第二次失败，保留剩余源项。</summary>
        internal override void Remove(string path, bool directory) { if (++count == 2) throw new IOException("injected partial cleanup failure"); base.Remove(path, directory); }
    }

    /// <summary>拥有单场景临时目录，自动清理资源。</summary>
    private sealed class Root : IDisposable
    {
        internal string Path { get; }
        /// <summary>在 parent 创建测试目录；省略时使用系统临时目录。</summary>
        internal Root(string? parent = null) { Path = System.IO.Path.Combine(parent ?? System.IO.Path.GetTempPath(), "common-move-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(Path); }
        /// <summary>回收本次场景的私有目录。</summary>
        public void Dispose() { Directory.Delete(Path, true); }
    }

    /// <summary>逐调用的故障 I/O，source 和 phase 不影响其他测试。</summary>
    private sealed class FaultIo(string source, string phase) : FileOperations.MoveIo
    {
        private bool published;
        /// <summary>src/dst 首次模拟 EXDEV，overwrite 原样传到真实发布；race 模拟竞争者。</summary>
        internal override void Rename(string src, string dst, bool overwrite)
        {
            if (phase == "alias-race") { if (Link(src, dst) != 0) throw new IOException("create alias failed"); base.Rename(src, dst, overwrite); return; }
            if (phase == "native-sync") { base.Rename(src, dst, overwrite); return; }
            if (src == source) throw new IOException("injected rename failure", new Win32Exception(phase == "rename-denied" ? 13 : 18));
            if (phase == "race") File.WriteAllText(dst, "competitor");
            base.Rename(src, dst, overwrite); published = true;
        }
        /// <summary>path 在发布或源删除后同步失败，其余均为真实 fsync。</summary>
        internal override void SyncDirectory(string path)
        {
            if (phase == "native-sync") throw new IOException("injected directory sync failure");
            if (published && (phase == "publish-sync" || (phase == "source-sync" && !File.Exists(source)))) throw new IOException("injected directory sync failure");
            base.SyncDirectory(path);
        }
        /// <summary>path 为已核验源项，directory 指定类型；模拟清理权限失败。</summary>
        internal override void Remove(string path, bool directory) { if (phase == "delete") throw new IOException("injected remove failure"); base.Remove(path, directory); }
        /// <summary>input/output 为复制流，expected 为原始长度；模拟写满、中断和源变化。</summary>
        internal override void Copy(FileStream input, FileStream output, ulong expected)
        {
            if (phase is "copy" or "interrupt") { output.Write(new byte[] { 1, 2, 3 }); throw new IOException("injected copy failure", new Win32Exception(phase == "copy" ? 28 : 4)); }
            base.Copy(input, output, expected); if (phase == "change") File.WriteAllText(source, "modified-source");
        }
    }

    /// <summary>测试 source/target 硬链接别名，返回原始系统结果。</summary>
    [DllImport("libc", EntryPoint = "link", SetLastError = true)]
    private static extern int Link(string source, string target);
    /// <summary>以 mode 创建 path FIFO，验证特殊类型拒绝。</summary>
    [DllImport("libc", EntryPoint = "mkfifo", SetLastError = true)]
    private static extern int Mkfifo(string path, uint mode);
}
