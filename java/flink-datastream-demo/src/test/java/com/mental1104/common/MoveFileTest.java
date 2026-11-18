package com.mental1104.common;

import static org.junit.jupiter.api.Assertions.*;
import static org.junit.jupiter.api.Assumptions.assumeTrue;

import com.sun.jna.NativeLibrary;
import com.sun.jna.Platform;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.attribute.FileTime;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.Comparator;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

/** 验证公开移动、真实跨卷与发布前后的故障语义，每个场景独占目录。 */
class MoveFileTest {
  @TempDir Path root;

  /** 最小完整调用：准备并关闭源、公开移动、检查结果，JUnit 自动回收 root。 */
  @Test
  void minimalCompleteUsage() throws IOException {
    Path source = root.resolve("source"), target = root.resolve("target");
    // 写入完成后关闭，移动期间没有活动写入者。
    Files.writeString(source, "hello");
    FileOperations.MoveResult result = FileOperations.moveFile(source, target);
    // 同时核验状态、目标内容和源清理；测试退出后临时资源自动回收。
    assertEquals(Platform.isWindows() ? FileOperations.MoveStatus.DURABILITY_UNCERTAIN
                                      : FileOperations.MoveStatus.COMPLETED,
        result.status);
    assertTrue(result.sourceRemoved);
    assertEquals("hello", Files.readString(target));
    assertFalse(Files.exists(source));
  }

  /** cross 控制真实卷模式，验证空/大文件、权限/mtime、目录和不跟随的链接。 */
  @ParameterizedTest
  @ValueSource(booleans = {false, true})
  void filesTreesAndMetadata(boolean cross) throws IOException {
    assumeTrue(!Platform.isWindows(), "POSIX behavior suite");
    try (Destination dest = new Destination(cross)) {
      for (int size : new int[] {0, 17, 2 * 1024 * 1024 + 137}) {
        Path source = root.resolve("source"), target = dest.path.resolve("target");
        byte[] data = new byte[size];
        for (int i = 0; i < size; ++i) data[i] = (byte) (i % 251);
        Files.write(source, data);
        Files.setPosixFilePermissions(source, PosixFilePermissions.fromString("rw-r-----"));
        FileTime stamp = FileTime.from(java.time.Instant.ofEpochSecond(1600000000L, 123456789));
        Files.setLastModifiedTime(source, stamp);
        probeCross(source, target, cross);
        assertEquals(
            FileOperations.MoveStatus.COMPLETED, FileOperations.moveFile(source, target).status);
        assertArrayEquals(data, Files.readAllBytes(target));
        assertFalse(Files.exists(source));
        assertEquals(stamp, Files.getLastModifiedTime(target));
        assertEquals(
            PosixFilePermissions.fromString("rw-r-----"), Files.getPosixFilePermissions(target));
        Files.delete(target);
      }
      Path source = root.resolve("tree"), target = dest.path.resolve("tree-target");
      Files.createDirectories(source.resolve("empty"));
      Files.writeString(source.resolve("data"), "tree");
      Files.createLink(source.resolve("alias"), source.resolve("data"));
      Files.createSymbolicLink(source.resolve("link"), Path.of("missing"));
      assertEquals(
          FileOperations.MoveStatus.COMPLETED, FileOperations.moveFile(source, target).status);
      assertTrue(Files.isDirectory(target.resolve("empty")));
      assertEquals("tree", Files.readString(target.resolve("data")));
      assertEquals(Path.of("missing"), Files.readSymbolicLink(target.resolve("link")));
      Files.createSymbolicLink(root.resolve("root-link"), Path.of("missing-root"));
      assertEquals(FileOperations.MoveStatus.COMPLETED,
          FileOperations.moveFile(root.resolve("root-link"), dest.path.resolve("moved-link"))
              .status);
    }
  }

  /** cross 控制卷模式，默认保持已有链接，显式覆盖遵守原生目录冲突规则。 */
  @ParameterizedTest
  @ValueSource(booleans = {false, true})
  void conflictsAndOverwrite(boolean cross) throws IOException {
    assumeTrue(!Platform.isWindows(), "POSIX behavior suite");
    try (Destination dest = new Destination(cross)) {
      Path source = root.resolve("source"), target = dest.path.resolve("target");
      Files.writeString(source, "new");
      Files.createSymbolicLink(target, Path.of("missing"));
      assertThrows(IOException.class, () -> FileOperations.moveFile(source, target));
      assertEquals("new", Files.readString(source));
      assertTrue(Files.isSymbolicLink(target));
      assertEquals(FileOperations.MoveStatus.COMPLETED,
          FileOperations.moveFile(source, target, true).status);
      assertEquals("new", Files.readString(target));
      Files.delete(target);
      Files.createDirectory(source);
      Files.createDirectory(target);
      Files.writeString(target.resolve("old"), "old");
      Files.writeString(source.resolve("new"), "new");
      Files.setPosixFilePermissions(source, PosixFilePermissions.fromString("r-x------"));
      try {
        assertThrows(IOException.class, () -> FileOperations.moveFile(source, target, true));
      } finally {
        Files.setPosixFilePermissions(source, PosixFilePermissions.fromString("rwx------"));
      }
      try (Stream<Path> items = Files.list(dest.path)) {
        assertFalse(items.anyMatch(p -> p.getFileName().toString().startsWith(".move-")));
      }
      assertEquals("old", Files.readString(target.resolve("old")));
      Files.delete(target.resolve("old"));
      assertEquals(FileOperations.MoveStatus.COMPLETED,
          FileOperations.moveFile(source, target, true).status);
    }
  }

  /** 同路径/硬链接、目录后代、缺失父目录和 FIFO 均安全拒绝。 */
  @Test
  void aliasesSpecialAndMissing() throws IOException {
    assumeTrue(!Platform.isWindows(), "POSIX behavior suite");
    Path source = root.resolve("source"), alias = root.resolve("alias");
    Files.writeString(source, "safe");
    Files.createLink(alias, source);
    for (Path target : new Path[] {source, alias, root.resolve("missing/target")})
      assertThrows(IOException.class, () -> FileOperations.moveFile(source, target, true));
    Path tree = Files.createDirectory(root.resolve("tree"));
    assertThrows(IOException.class, () -> FileOperations.moveFile(tree, tree.resolve("child")));
    Path fifo = root.resolve("fifo");
    assertEquals(0,
        NativeLibrary.getInstance(Platform.C_LIBRARY_NAME)
            .getFunction("mkfifo")
            .invokeInt(new Object[] {fifo.toString(), 0600}));
    assertThrows(IOException.class, () -> FileOperations.moveFile(fifo, root.resolve("target")));
    assertEquals("safe", Files.readString(source));
  }

  /** phase 指定复制/发布/清理阶段，确保发布后不会再抛出误导性的普通失败异常。 */
  @ParameterizedTest
  @ValueSource(strings = {"copy", "interrupt", "race", "change", "delete", "publish-sync",
                   "source-sync", "rename-denied", "alias-race", "native-sync"})
  void
  injectedBoundaries(String phase) throws IOException {
    assumeTrue(!Platform.isWindows(), "POSIX fault injection");
    Path source = root.resolve("source"), target = root.resolve("target");
    Files.writeString(source, "original");
    FaultIo io = new FaultIo(source, phase);
    if (java.util.Set.of("copy", "interrupt", "race", "change", "rename-denied").contains(phase)) {
      assertThrows(IOException.class, () -> FileOperations.moveWith(source, target, false, io));
      assertTrue(Files.exists(source));
      if (phase.equals("race"))
        assertEquals("competitor", Files.readString(target));
      else
        assertFalse(Files.exists(target));
    } else {
      FileOperations.MoveResult result =
          FileOperations.moveWith(source, target, phase.equals("alias-race"), io);
      assertEquals(phase.equals("delete") || phase.equals("alias-race")
              ? FileOperations.MoveStatus.PUBLISHED_SOURCE_REMAINS
              : FileOperations.MoveStatus.DURABILITY_UNCERTAIN,
          result.status);
      assertEquals(
          phase.equals("source-sync") || phase.equals("native-sync"), result.sourceRemoved);
      assertNotNull(result.error);
      assertEquals("original", Files.readString(target));
    }
    try (Stream<Path> paths = Files.list(root)) {
      assertFalse(paths.anyMatch(p -> p.getFileName().toString().startsWith(".move-")));
    }
  }

  /** 真实删除一个子项后失败，验证目标完整而源树部分残留。 */
  @Test
  void partialDirectoryCleanup() throws IOException {
    assumeTrue(!Platform.isWindows(), "POSIX fault injection");
    Path source = Files.createDirectory(root.resolve("tree")), target = root.resolve("target");
    Files.writeString(source.resolve("a"), "a");
    Files.writeString(source.resolve("b"), "b");
    FileOperations.MoveIo io = new FileOperations.MoveIo() {
      int count;
      /** src 是源根时注入 EXDEV，其余 dst/overwrite 仍使用真实原子发布。 */
      @Override
      void rename(Path src, Path dst, boolean overwrite) throws IOException {
        if (src.equals(source))
          throw new FileOperations.NativeIOException(18, "injected cross-device rename");
        super.rename(src, dst, overwrite);
      }
      /** 第一次真实删除 path，第二次失败，保留剩余源项。 */
      @Override
      void remove(Path path) throws IOException {
        if (++count == 2)
          throw new IOException("injected partial cleanup failure");
        super.remove(path);
      }
    };
    FileOperations.MoveResult result = FileOperations.moveWith(source, target, false, io);
    assertEquals(FileOperations.MoveStatus.PUBLISHED_SOURCE_REMAINS, result.status);
    assertFalse(result.sourceRemoved);
    assertEquals("a", Files.readString(target.resolve("a")));
    assertEquals("b", Files.readString(target.resolve("b")));
    try (Stream<Path> left = Files.list(source)) {
      assertEquals(1, left.count());
    }
  }

  /** 对 source/target 的独立原生命名证明 cross 场景确实收到 EXDEV，不把模拟路径当验收。 */
  private static void probeCross(Path source, Path target, boolean cross) {
    if (!cross)
      return;
    FileOperations.NativeIOException error = assertThrows(FileOperations.NativeIOException.class,
        () -> new FileOperations.MoveIo().rename(source, target, false));
    assertEquals(18, error.code);
  }

  /** 目标临时目录的所有者，真实跨卷时在 /dev/shm 创建；关闭后不留测试产物。 */
  private final class Destination implements AutoCloseable {
    final Path path;
    /** cross 决定是否使用第二挂载，无第二挂载时明确跳过。 */
    Destination(boolean cross) throws IOException {
      if (cross)
        assumeTrue(Files.isDirectory(Path.of("/dev/shm")), "second filesystem unavailable");
      path = Files.createTempDirectory(cross ? Path.of("/dev/shm") : root, "common-move-");
    }
    /** 不跟随链接回收本场景资源。 */
    @Override
    public void close() throws IOException {
      try (Stream<Path> paths = Files.walk(path)) {
        for (Path item : paths.sorted(Comparator.reverseOrder()).collect(Collectors.toList()))
          Files.delete(item);
      }
    }
  }

  /** 当前场景的 I/O 注入，不共享状态。 */
  private static final class FaultIo extends FileOperations.MoveIo {
    final Path source;
    final String phase;
    boolean published;
    /** source 为原始项，phase 为故障边界。 */
    FaultIo(Path source, String phase) {
      this.source = source;
      this.phase = phase;
    }
    /** src/dst 首次产生 EXDEV，overwrite 原样传递；发布仍调用真实内核。 */
    @Override
    void rename(Path src, Path dst, boolean overwrite) throws IOException {
      if (phase.equals("alias-race")) {
        Files.createLink(dst, src);
        super.rename(src, dst, overwrite);
        return;
      }
      if (phase.equals("native-sync")) {
        super.rename(src, dst, overwrite);
        return;
      }
      if (src.equals(source))
        throw new FileOperations.NativeIOException(
            phase.equals("rename-denied") ? 13 : 18, "injected rename failure");
      if (phase.equals("race"))
        Files.writeString(dst, "competitor");
      super.rename(src, dst, overwrite);
      published = true;
    }
    /** path 在发布后或源删除后同步失败，其余真实同步。 */
    @Override
    void syncDirectory(Path path) throws IOException {
      if (phase.equals("native-sync"))
        throw new IOException("injected directory sync failure");
      if (published
          && (phase.equals("publish-sync")
              || (phase.equals("source-sync") && !Files.exists(source))))
        throw new IOException("injected directory sync failure");
      super.syncDirectory(path);
    }
    /** path 为已核验源项，在 delete 阶段模拟权限失败。 */
    @Override
    void remove(Path path) throws IOException {
      if (phase.equals("delete"))
        throw new IOException("injected remove failure");
      super.remove(path);
    }
    /** input/output 为复制流，expected 为原长；模拟写满/中断或源变化。 */
    @Override
    void copy(FileChannel input, FileChannel output, long expected) throws IOException {
      if (phase.equals("copy") || phase.equals("interrupt")) {
        output.write(java.nio.ByteBuffer.wrap(new byte[] {1, 2, 3}));
        throw new FileOperations.NativeIOException(
            phase.equals("copy") ? 28 : 4, "injected copy failure");
      }
      super.copy(input, output, expected);
      if (phase.equals("change"))
        Files.writeString(source, "modified-source");
    }
  }
}
