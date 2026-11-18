package com.mental1104.common;

import com.sun.jna.Function;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Platform;
import com.sun.jna.WString;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.BasicFileAttributeView;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.attribute.FileTime;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** 文件、目录和符号链接的移动；无共享可变状态，目录不合并，链接不跟随。 */
public final class FileOperations {
  /** 工具类只提供静态入口，不创建实例。 */
  private FileOperations() {}

  /** 目标已发布后的状态；发布前失败通过 IOException 报告。 */
  public enum MoveStatus {
    /** 源已清理，必要的目录同步成功。 */
    COMPLETED,
    /** 目标已发布，源树可能只清理了一部分。 */
    PUBLISHED_SOURCE_REMAINS,
    /** 命名成功，但目录持久化无法确认。 */
    DURABILITY_UNCERTAIN
  }

  /** 已发布的不可变结果；持久化不确定时另查 sourceRemoved。 */
  public static final class MoveResult {
    /** 最终阶段，目录同步失败优先报告。 */
    public final MoveStatus status;
    /** 源根目录项是否已删除。 */
    public final boolean sourceRemoved;
    /** 完成时为 null，部分成功时为具体原因。 */
    public final IOException error;

    /**
     * 构造发布后的状态。
     * @param status 最终阶段
     * @param sourceRemoved 源根是否已删除
     * @param error 完成时为 null，否则为失败原因
     */
    private MoveResult(MoveStatus status, boolean sourceRemoved, IOException error) {
      this.status = status;
      this.sourceRemoved = sourceRemoved;
      this.error = error;
    }
  }

  /** 原生命名失败，保留 errno 以便只对 EXDEV 降级。 */
  static final class NativeIOException extends IOException {
    private static final long serialVersionUID = 1L;
    final int code;

    /** 保存 code 系统错误，operation 为 ASCII 失败阶段。 */
    NativeIOException(int code, String operation) {
      super(operation + " (native error " + code + ")");
      this.code = code;
    }
  }

  /** 不跟随链接的文件快照，把动态平台属性限制在读取边界。 */
  private static final class Entry {
    final Path path;
    final BasicFileAttributes basic;
    final int mode;
    final FileTime changed;

    /** 读取 path 的身份、普通权限与时间；不支持身份比较的文件系统明确拒绝。 */
    Entry(Path path) throws IOException {
      this.path = path;
      this.basic = Files.readAttributes(path, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
      if (basic.fileKey() == null)
        throw new IOException("source identity unavailable");
      if (Platform.isLinux() || Platform.isMac()) {
        Map<String, Object> attrs =
            Files.readAttributes(path, "unix:mode,ctime", LinkOption.NOFOLLOW_LINKS);
        this.mode = (Integer) attrs.get("mode");
        this.changed = (FileTime) attrs.get("ctime");
      } else {
        this.mode = 0;
        this.changed = basic.lastModifiedTime();
      }
    }

    /**
     * 验证当前项；identityOnly 用于全树复核后的清理，忽略自身清理造成的时间变化，冲突抛
     * IOException。
     */
    void verify(boolean identityOnly) throws IOException {
      Entry now = new Entry(path);
      if (!Objects.equals(basic.fileKey(), now.basic.fileKey()) || mode != now.mode
          || (!identityOnly
              && (basic.size() != now.basic.size()
                  || !basic.lastModifiedTime().equals(now.basic.lastModifiedTime())
                  || !changed.equals(now.changed)))) {
        throw new IOException("source changed during move");
      }
    }
  }

  /** 本次调用的 I/O 边界；包内测试逐阶段替换，不修改全局状态。 */
  static class MoveIo {
    /** 原子移动 source 到 target，overwrite 控制替换；失败保留原生错误码。 */
    void rename(Path source, Path target, boolean overwrite) throws IOException {
      try {
        Native.setLastError(0);
        int rc;
        if (Platform.isWindows()) {
          Function move = NativeLibrary.getInstance("kernel32")
                              .getFunction("MoveFileExW", Function.ALT_CONVENTION);
          rc = move.invokeInt(new Object[] {
              new WString(source.toString()), new WString(target.toString()), overwrite ? 1 : 0});
          if (rc == 0)
            throw new NativeIOException(Native.getLastError(), "atomic rename failed");
        } else {
          NativeLibrary libc = NativeLibrary.getInstance(Platform.C_LIBRARY_NAME);
          if (overwrite) {
            rc = libc.getFunction("rename").invokeInt(
                new Object[] {source.toString(), target.toString()});
          } else if (Platform.isLinux()) {
            rc = libc.getFunction("renameat2")
                     .invokeInt(new Object[] {-100, source.toString(), -100, target.toString(), 1});
          } else if (Platform.isMac()) {
            rc = libc.getFunction("renamex_np")
                     .invokeInt(new Object[] {source.toString(), target.toString(), 4});
          } else
            throw new IOException("atomic no-replace rename unsupported");
          if (rc != 0)
            throw new NativeIOException(Native.getLastError(), "atomic rename failed");
        }
      } catch (UnsatisfiedLinkError e) {
        throw new IOException("native atomic rename unavailable", e);
      }
    }

    /** 同步 path 的目录项；provider 不支持时报告 IOException，绝不假装持久化完成。 */
    void syncDirectory(Path path) throws IOException {
      if (Platform.isWindows())
        throw new IOException("directory sync unsupported on Windows");
      try (FileChannel channel = FileChannel.open(path, StandardOpenOption.READ)) {
        channel.force(true);
      }
    }

    /** 删除 path 的单个项，不递归删除未知新增项。 */
    void remove(Path path) throws IOException {
      Files.delete(path);
    }

    /** 分块复制 input 到 output，expected 是原始长度；文件由调用方关闭。 */
    void copy(FileChannel input, FileChannel output, long expected) throws IOException {
      byte[] a = new byte[65536], b = new byte[65536];
      long total = 0;
      for (;;) {
        int n = readBlock(input, a);
        if (n == 0)
          break;
        ByteBuffer bytes = ByteBuffer.wrap(a, 0, n);
        while (bytes.hasRemaining()) {
          if (output.write(bytes) == 0)
            throw new IOException("copy write made no progress");
        }
        total += n;
      }
      if (total != expected)
        throw new IOException("copy size verification failed");
      input.position(0);
      output.position(0);
      for (;;) {
        int n = readBlock(input, a), m = readBlock(output, b);
        if (n != m || !Arrays.equals(a, 0, n, b, 0, m))
          throw new IOException("copy readback verification failed");
        if (n == 0)
          break;
      }
    }
  }

  /** 读取 channel 至 buffer 满或 EOF，返回实际字节数；零进展明确报错。 */
  private static int readBlock(FileChannel channel, byte[] buffer) throws IOException {
    ByteBuffer bytes = ByteBuffer.wrap(buffer);
    while (bytes.hasRemaining()) {
      int n = channel.read(bytes);
      if (n < 0)
        break;
      if (n == 0)
        throw new IOException("copy read made no progress");
    }
    return bytes.position();
  }

  /** 返回 path 的非空父目录，调用方先将路径转为绝对形式。 */
  private static Path parent(Path path) throws IOException {
    Path result = path.getParent();
    if (result == null)
      throw new IOException("invalid move path");
    return result;
  }

  /** 为 target 恢复 entry 的普通权限和 mtime；link 时只修改链接本身。 */
  private static void restore(Path target, Entry entry, boolean link) throws IOException {
    if (!link)
      Files.setAttribute(target, "unix:mode", entry.mode & 0777, LinkOption.NOFOLLOW_LINKS);
    Files.getFileAttributeView(target, BasicFileAttributeView.class, LinkOption.NOFOLLOW_LINKS)
        .setTimes(entry.basic.lastModifiedTime(), null, null);
  }

  /** 将 source 复制到私有 target，entries 追加快照，io 为本次 I/O 依赖；不跟随链接。 */
  private static void copyTree(Path source, Path target, List<Entry> entries, MoveIo io)
      throws IOException {
    Entry entry = new Entry(source);
    entries.add(entry);
    if (entry.basic.isSymbolicLink()) {
      Files.createSymbolicLink(target, Files.readSymbolicLink(source));
      restore(target, entry, true);
    } else if (entry.basic.isDirectory()) {
      Files.createDirectory(target,
          PosixFilePermissions.asFileAttribute(PosixFilePermissions.fromString("rwx------")));
      List<Path> children;
      try (Stream<Path> stream = Files.list(source)) {
        children = stream.sorted().collect(Collectors.toList());
      }
      for (Path child : children) copyTree(child, target.resolve(child.getFileName()), entries, io);
      restore(target, entry, false);
      io.syncDirectory(target);
    } else if (entry.basic.isRegularFile()) {
      try (
          FileChannel input =
              FileChannel.open(source, StandardOpenOption.READ, LinkOption.NOFOLLOW_LINKS);
          FileChannel output = FileChannel.open(target,
              java.util.Set.of(
                  StandardOpenOption.CREATE_NEW, StandardOpenOption.READ, StandardOpenOption.WRITE),
              PosixFilePermissions.asFileAttribute(PosixFilePermissions.fromString("rw-------")))) {
        io.copy(input, output, entry.basic.size());
        restore(target, entry, false);
        output.force(true);
      }
    } else
      throw new IOException("unsupported source type");
    entry.verify(false);
  }

  /** 尽力清理 stage 私有树；先恢复临时目录清理权限，不跟随链接，不覆盖原始异常。 */
  private static void discard(Path stage) {
    try {
      BasicFileAttributes info =
          Files.readAttributes(stage, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
      if (info.isDirectory()) {
        Files.setPosixFilePermissions(stage, PosixFilePermissions.fromString("rwx------"));
        List<Path> children;
        try (Stream<Path> stream = Files.list(stage)) {
          children = stream.collect(Collectors.toList());
        }
        for (Path child : children) discard(child);
      }
      Files.delete(stage);
    } catch (IOException ignored) { /* 尽力清理。 */
    }
  }

  /** 执行跨卷 source 到 target 的协议；overwrite 控制最终发布，io 保留阶段边界。 */
  private static MoveResult crossMove(Path source, Path target, boolean overwrite, MoveIo io)
      throws IOException {
    if (Platform.isWindows())
      throw new IOException("cross-volume move unsupported on Windows");
    Path stage = Files.createTempDirectory(parent(target), ".move-",
        PosixFilePermissions.asFileAttribute(PosixFilePermissions.fromString("rwx------")));
    try {
      Path payload = stage.resolve("payload");
      List<Entry> entries = new ArrayList<>();
      copyTree(source, payload, entries, io);
      for (Entry entry : entries) entry.verify(false);
      io.syncDirectory(stage);
      io.syncDirectory(parent(target));
      io.rename(payload, target, overwrite);
      // 从这里开始不再抛出普通 I/O 异常，避免误报为“未发布”。
      try {
        Files.delete(stage);
        io.syncDirectory(parent(target));
      } catch (IOException e) {
        return new MoveResult(MoveStatus.DURABILITY_UNCERTAIN, false, e);
      }
      try {
        for (Entry entry : entries) entry.verify(false);
      } catch (IOException e) {
        return new MoveResult(MoveStatus.PUBLISHED_SOURCE_REMAINS, false, e);
      }
      for (int i = entries.size() - 1; i >= 0; --i) {
        Entry entry = entries.get(i);
        // 全树已复核；删除硬链接兄弟会改变 ctime，此处只复核身份。
        try {
          entry.verify(true);
          io.remove(entry.path);
        } catch (IOException e) {
          return new MoveResult(MoveStatus.PUBLISHED_SOURCE_REMAINS, false, e);
        }
        try {
          io.syncDirectory(parent(entry.path));
        } catch (IOException e) {
          return new MoveResult(MoveStatus.DURABILITY_UNCERTAIN, i == 0, e);
        }
      }
      return new MoveResult(MoveStatus.COMPLETED, true, null);
    } finally {
      discard(stage);
    }
  }

  /** 用 io 移动 source 到 target，overwrite 控制原子覆盖；发布前抛 IOException。 */
  static MoveResult moveWith(Path source, Path target, boolean overwrite, MoveIo io)
      throws IOException {
    Path src = source.toAbsolutePath().normalize(), dst = target.toAbsolutePath().normalize();
    Entry entry = new Entry(src);
    if (!entry.basic.isRegularFile() && !entry.basic.isDirectory() && !entry.basic.isSymbolicLink())
      throw new IOException("unsupported source type");
    try {
      Entry other = new Entry(dst);
      if (Objects.equals(entry.basic.fileKey(), other.basic.fileKey()))
        throw new IOException("source and target are aliases");
    } catch (NoSuchFileException ignored) { /* 最终冲突仍由原子命名裁决。 */
    }
    if (entry.basic.isDirectory() && parent(dst).toRealPath().startsWith(src.toRealPath()))
      throw new IOException("target is inside source");
    try {
      io.rename(src, dst, overwrite);
    } catch (NativeIOException e) {
      if (e.code != (Platform.isWindows() ? 17 : 18))
        throw e;
      return crossMove(src, dst, overwrite, io);
    }
    // 覆盖 rename 遇到竞争者创建的源硬链接时可能成功却不删除源，必须检查实际效果。
    IOException sourceError = null;
    try {
      new Entry(src);
      sourceError = new IOException("source remains after rename");
    } catch (NoSuchFileException ignored) { /* 源目录项已经移除。 */
    } catch (IOException e) {
      sourceError = e;
    }
    try {
      io.syncDirectory(parent(dst));
      io.syncDirectory(parent(src));
    } catch (IOException e) {
      return new MoveResult(MoveStatus.DURABILITY_UNCERTAIN, sourceError == null, e);
    }
    if (sourceError != null)
      return new MoveResult(MoveStatus.PUBLISHED_SOURCE_REMAINS, false, sourceError);
    return new MoveResult(MoveStatus.COMPLETED, true, null);
  }

  /**
   * 默认排他移动 source 到最终 target。
   * @param source 已存在源项，调用方须独占源树及祖先目录
   * @param target 父目录已存在的最终路径，不合并目录
   * @return 已发布结果，调用方检查状态后决定恢复，不盲目重试
   * @throws IOException 发布前失败，源与旧目标保持，临时产物尽力回收
   */
  public static MoveResult moveFile(Path source, Path target) throws IOException {
    return moveFile(source, target, false);
  }

  /**
   * 移动普通文件、目录或链接；只有明确跨卷错误才复制、校验、同步、发布、清理。
   * @param source 已存在源项；调用方必须独占源树、祖先目录及目标父目录
   * @param target 最终路径，父目录必须存在；同路径/硬链接别名拒绝
   * @param overwrite false 使用内核排他重命名，true 按原生类型规则替换
   * @return 已发布结果；目录同步失败优先报告 DURABILITY_UNCERTAIN
   * @throws IOException 发布前错误；跨卷非端到端原子，变化检测不能替代排他访问
   * <p>跨卷只保留普通权限、mtime、链接文本；不保证 uid/gid、ACL、xattr、稀疏与硬链接拓扑。
   * Windows 仅支持同卷并报告持久化不确定；非默认本地文件 provider 明确拒绝。
   */
  public static MoveResult moveFile(Path source, Path target, boolean overwrite)
      throws IOException {
    if (source.getFileSystem() != java.nio.file.FileSystems.getDefault()
        || target.getFileSystem() != java.nio.file.FileSystems.getDefault())
      throw new IOException("non-native filesystem unsupported");
    return moveWith(source, target, overwrite, new MoveIo());
  }
}
