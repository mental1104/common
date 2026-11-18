"""文件、目录和符号链接的移动；跨卷发布与源清理具有独立的失败边界。"""

from __future__ import annotations

import ctypes
import errno
import os
import stat
import sys
import tempfile
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional, Union


class MoveStatus(Enum):
    """发布后的最终状态；持久化不确定时通过 source_removed 判断源清理情况。"""

    COMPLETED = "completed"
    PUBLISHED_SOURCE_REMAINS = "published_source_remains"
    DURABILITY_UNCERTAIN = "durability_uncertain"


@dataclass(frozen=True)
class MoveResult:
    """已发布结果：status 为阶段状态，source_removed 表示源已删除，error 为失败原因。"""

    status: MoveStatus
    source_removed: bool
    error: Optional[OSError] = None


@dataclass(frozen=True)
class _Entry:
    """复制清单中的原始路径及不跟随链接的元信息，用于发布前复核。"""

    path: Path
    info: os.stat_result


class _MoveIO:
    """封装原子命名与持久化边界，测试可替换单个真实 I/O 操作。"""

    @staticmethod
    def rename(source: Path, target: Path, overwrite: bool) -> None:
        """原子移动 source 到最终 target；overwrite=False 必须由内核排他裁决。"""
        if overwrite:
            os.replace(source, target)
            return
        if os.name == "nt":
            # Windows os.rename 不覆盖；禁止使用 os.replace 模拟排他发布。
            os.rename(source, target)
            return
        libc = ctypes.CDLL(None, use_errno=True)
        if sys.platform.startswith("linux"):
            operation = getattr(libc, "renameat2", None)
            if operation is None:
                raise OSError(errno.ENOTSUP, "atomic no-replace rename unavailable")
            operation.argtypes = [
                ctypes.c_int,
                ctypes.c_char_p,
                ctypes.c_int,
                ctypes.c_char_p,
                ctypes.c_uint,
            ]
            result = operation(-100, os.fsencode(source), -100, os.fsencode(target), 1)
        elif sys.platform == "darwin":
            operation = libc.renamex_np
            operation.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
            result = operation(os.fsencode(source), os.fsencode(target), 4)
        else:
            raise OSError(errno.ENOTSUP, "atomic no-replace rename unavailable")
        if result != 0:
            code = ctypes.get_errno()
            raise OSError(code, "atomic rename failed", str(target))

    @staticmethod
    def sync_dir(path: Path) -> None:
        """同步 path 的目录项；无法确认持久化时抛 OSError。"""
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
        try:
            os.fsync(fd)
        finally:
            os.close(fd)

    @staticmethod
    def remove(path: Path, directory: bool) -> None:
        """删除一个已核验的 path；directory 指定不跟随链接的目录类型。"""
        if directory:
            path.rmdir()
        else:
            path.unlink()

    @staticmethod
    def discard(path: Path) -> None:
        """尽力回收本次私有 path；临时目录可能已恢复只读权限，先恢复本用户清理权限，不跟随链接。"""
        try:
            if stat.S_ISDIR(path.lstat().st_mode):
                path.chmod(0o700)
                for child in path.iterdir():
                    _MoveIO.discard(child)
                path.rmdir()
            else:
                path.unlink()
        except OSError:
            pass  # 清理失败不能覆盖原始错误；绝不回滚已发布目标。


class _TreeCopy:
    """构建不跟随符号链接的复制清单，逐文件读回验证并保留普通权限/mtime。"""

    def __init__(self) -> None:
        """创建仅属于本次移动的清单；不共享可变状态。"""
        self.entries: list[_Entry] = []

    @staticmethod
    def unchanged(path: Path, before: os.stat_result, *, identity_only: bool = False) -> None:
        """核验 path 仍代表 before；identity_only 用于全树已复核后的源清理，忽略自身清理导致的时间变化。"""
        after = path.lstat()
        if (after.st_dev, after.st_ino, after.st_mode) != (
            before.st_dev,
            before.st_ino,
            before.st_mode,
        ):
            raise OSError(errno.EBUSY, "source identity changed", str(path))
        if not identity_only and (after.st_size, after.st_mtime_ns, after.st_ctime_ns) != (
            before.st_size,
            before.st_mtime_ns,
            before.st_ctime_ns,
        ):
            raise OSError(errno.EBUSY, "source changed during move", str(path))

    def copy(self, source: Path, target: Path) -> None:
        """复制 source 到私有区域的 target；遇到特殊类型、复制/校验错误立即停止。"""
        before = source.lstat()
        self.entries.append(_Entry(source, before))
        if stat.S_ISLNK(before.st_mode):
            target.symlink_to(os.readlink(source))
            os.utime(target, ns=(before.st_atime_ns, before.st_mtime_ns), follow_symlinks=False)
        elif stat.S_ISDIR(before.st_mode):
            target.mkdir(mode=0o700)
            for child in sorted(source.iterdir()):
                self.copy(child, target / child.name)
            os.chmod(target, stat.S_IMODE(before.st_mode) & 0o777)
            os.utime(target, ns=(before.st_atime_ns, before.st_mtime_ns))
            _MoveIO.sync_dir(target)
        elif stat.S_ISREG(before.st_mode):
            # O_NOFOLLOW 防止最后一级符号链接替换；调用方仍须独占源树及祖先目录。
            fd = os.open(
                source, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
            )
            with os.fdopen(fd, "rb") as reader, os.fdopen(
                os.open(target, os.O_RDWR | os.O_CREAT | os.O_EXCL, 0o600), "w+b"
            ) as writer:
                opened = os.fstat(reader.fileno())
                if (opened.st_dev, opened.st_ino) != (before.st_dev, before.st_ino):
                    raise OSError(errno.EBUSY, "source replaced before copy")
                count = 0
                while True:
                    data = reader.read(65536)
                    if not data:
                        break
                    writer.write(data)
                    count += len(data)
                writer.flush()
                if count != before.st_size:
                    raise OSError(errno.EIO, "copy size verification failed")
                reader.seek(0)
                writer.seek(0)
                while True:
                    data = reader.read(65536)
                    if data != writer.read(65536):
                        raise OSError(errno.EIO, "copy readback verification failed")
                    if not data:
                        break
                os.fchmod(writer.fileno(), stat.S_IMODE(before.st_mode) & 0o777)
                os.utime(writer.fileno(), ns=(before.st_atime_ns, before.st_mtime_ns))
                os.fsync(writer.fileno())
        else:
            raise OSError(errno.ENOTSUP, "unsupported source type", str(source))
        self.unchanged(source, before)

    def verify(self) -> None:
        """在发布及源清理前复核整个源清单；检测只能发现可观察变化，不能替代排他访问。"""
        for entry in self.entries:
            self.unchanged(entry.path, entry.info)

    def remove_source(self) -> MoveResult:
        """后序删除并同步清单，返回发布后状态；不递归删除未知新增项。"""
        try:
            self.verify()
        except OSError as error:
            return MoveResult(MoveStatus.PUBLISHED_SOURCE_REMAINS, False, error)
        for entry in reversed(self.entries):
            try:
                directory = stat.S_ISDIR(entry.info.st_mode)
                # 删除硬链接兄弟会改变同 inode 的 ctime；全树已复核，此处只比较身份。
                self.unchanged(entry.path, entry.info, identity_only=True)
                _MoveIO.remove(entry.path, directory)
            except OSError as error:
                return MoveResult(MoveStatus.PUBLISHED_SOURCE_REMAINS, False, error)
            try:
                _MoveIO.sync_dir(entry.path.parent)
            except OSError as error:
                return MoveResult(MoveStatus.DURABILITY_UNCERTAIN, entry is self.entries[0], error)
        return MoveResult(MoveStatus.COMPLETED, True)


def move_file(
    source: Union[str, Path], target: Union[str, Path], *, overwrite: bool = False
) -> MoveResult:
    """移动普通文件、目录或链接，target 始终是最终路径，不合并目录。

    Args:
        source: 已存在的源项；调用方必须保证源树、祖先目录及目标父目录不被并发修改。
        target: 最终路径，父目录必须存在；同路径、硬链接别名、源目录的后代均拒绝。
        overwrite: 默认 False，原子拒绝已存在的目标；True 按原生 rename 类型规则替换。
    Returns:
        已发布的 MoveResult。持久化失败优先报告 DURABILITY_UNCERTAIN；此时不继续删除源。
    Raises:
        OSError: 发布前失败，源与旧目标保持；临时项尽力清理。特殊文件/不支持的平台明确拒绝。

    跨卷非端到端原子；只保留普通 rwx 权限、mtime 和链接文本，不保证 uid/gid、ACL、
    xattr、稀疏布局及硬链接拓扑。发布后不得盲目重试，应先检查结果及两端路径。
    Windows 仅支持同卷，目录同步不能确认时返回持久化不确定。
    """
    src, dst = Path(source).absolute(), Path(target).absolute()
    before = src.lstat()
    if not (
        stat.S_ISREG(before.st_mode) or stat.S_ISDIR(before.st_mode) or stat.S_ISLNK(before.st_mode)
    ):
        raise OSError(errno.ENOTSUP, "unsupported source type", str(src))
    if src.name in ("", ".", "..") or dst.name in ("", ".", ".."):
        raise OSError(errno.EINVAL, "invalid move path")
    try:
        other = dst.lstat()
    except FileNotFoundError:
        other = None
    if other is not None and (before.st_dev, before.st_ino) == (other.st_dev, other.st_ino):
        raise OSError(errno.EINVAL, "source and target are aliases")
    if stat.S_ISDIR(before.st_mode) and (
        src.resolve() == dst.parent.resolve() or src.resolve() in dst.parent.resolve().parents
    ):
        raise OSError(errno.EINVAL, "target is inside source")
    try:
        _MoveIO.rename(src, dst, overwrite)
    except OSError as error:
        if error.errno != errno.EXDEV and getattr(error, "winerror", None) != 17:
            raise
    else:
        # 覆盖模式遇到竞争者创建的源硬链接时，POSIX rename 可能成功但不删除源。
        source_error: Optional[OSError] = None
        try:
            src.lstat()
        except FileNotFoundError:
            pass
        except OSError as error:
            source_error = error
        else:
            source_error = OSError(errno.EBUSY, "source remains after rename")
        try:
            _MoveIO.sync_dir(dst.parent)
            _MoveIO.sync_dir(src.parent)
        except OSError as error:
            return MoveResult(MoveStatus.DURABILITY_UNCERTAIN, source_error is None, error)
        if source_error is not None:
            return MoveResult(MoveStatus.PUBLISHED_SOURCE_REMAINS, False, source_error)
        return MoveResult(MoveStatus.COMPLETED, True)

    if os.name == "nt":
        raise OSError(errno.ENOTSUP, "cross-volume move unsupported on Windows")
    # 私有容器保护已经恢复源权限的 payload；最终竞争由第二次原子 rename 裁决。
    stage = Path(tempfile.mkdtemp(prefix=".move-", dir=dst.parent))
    tree = _TreeCopy()
    try:
        payload = stage / "payload"
        tree.copy(src, payload)
        tree.verify()
        _MoveIO.sync_dir(stage)
        _MoveIO.sync_dir(dst.parent)
        _MoveIO.rename(payload, dst, overwrite)
        try:
            stage.rmdir()
            _MoveIO.sync_dir(dst.parent)
        except OSError as error:
            return MoveResult(MoveStatus.DURABILITY_UNCERTAIN, False, error)
        return tree.remove_source()
    finally:
        # 发布前失败也只回收本次私有区域；不触碰旧目标和源。
        _MoveIO.discard(stage)


__all__ = ["MoveResult", "MoveStatus", "move_file"]
