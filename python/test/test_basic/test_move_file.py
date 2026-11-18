"""移动公开契约：同卷、真实跨卷、冲突和发布前后故障。"""

import errno
import os
import stat
import tempfile
from pathlib import Path
from typing import Iterator

import pytest

from mental1104 import MoveStatus, move_file
from mental1104.file.move import _MoveIO, _TreeCopy


@pytest.fixture(params=[False, True], ids=["same-volume", "real-cross-volume"])
def destination(request: pytest.FixtureRequest, tmp_path: Path) -> Iterator[Path]:
    """为 request 选择同卷或 /dev/shm 真实跨卷目标；tmp_path 为源区，退出时清理。"""
    if not request.param:
        yield tmp_path
        return
    shared = Path("/dev/shm")
    if not shared.is_dir() or shared.stat().st_dev == tmp_path.stat().st_dev:
        pytest.skip("second filesystem unavailable")
    with tempfile.TemporaryDirectory(prefix="common-move-", dir=shared) as folder:
        yield Path(folder)


@pytest.mark.skipif(os.name == "nt", reason="POSIX behavior suite; Windows has a restricted API")
class TestMoveFile:
    """验证 move_file 的可观察路径状态与故障边界，测试互不依赖。"""

    def test_minimal_complete_usage(self, tmp_path: Path) -> None:
        """tmp_path 隔离资源：准备源、调用公开 API、读取结果、验证内容和源清理。"""
        source, target = tmp_path / "source", tmp_path / "target"
        # 准备源资源；文件写入后关闭，避免生命周期跨越移动调用。
        source.write_bytes(b"hello")
        result = move_file(source, target)
        # 读取状态和实际内容；fixture 在用例结束时回收目录。
        assert result.status is MoveStatus.COMPLETED
        assert result.source_removed and result.error is None
        assert target.read_bytes() == b"hello" and not source.exists()

    @pytest.mark.parametrize("size", [0, 17, 2 * 1024 * 1024 + 137])
    def test_bytes_and_metadata(self, tmp_path: Path, destination: Path, size: int) -> None:
        """以 tmp_path/destination 验证 size 字节的同卷/真实跨卷内容、普通权限和 mtime。"""
        source, target = tmp_path / "source", destination / "target"
        data = (bytes(range(256)) * (size // 256 + 1))[:size]
        source.write_bytes(data)
        source.chmod(0o640)
        os.utime(source, ns=(1_600_000_000_123456789, 1_600_000_000_123456789))
        result = move_file(source, target)
        assert result.status is MoveStatus.COMPLETED and not source.exists()
        assert target.read_bytes() == data
        assert stat.S_IMODE(target.stat().st_mode) == 0o640
        assert target.stat().st_mtime_ns == 1_600_000_000_123456789

    def test_tree_and_links(self, tmp_path: Path, destination: Path) -> None:
        """tmp_path 中的目录、空目录及悬空链接被整体移动到 destination，链接目标不访问。"""
        source, target = tmp_path / "tree", destination / "target"
        (source / "empty").mkdir(parents=True)
        (source / "data").write_bytes(b"tree")
        os.link(source / "data", source / "alias")
        (source / "dangling").symlink_to("missing")
        result = move_file(source, target)
        assert result.status is MoveStatus.COMPLETED and not source.exists()
        assert (target / "empty").is_dir()
        assert (target / "data").read_bytes() == b"tree"
        assert (target / "alias").read_bytes() == b"tree"
        assert os.readlink(target / "dangling") == "missing"
        link = tmp_path / "root-link"
        link.symlink_to("missing-root")
        assert move_file(link, destination / "moved-link").status is MoveStatus.COMPLETED
        assert os.readlink(destination / "moved-link") == "missing-root"

    def test_conflict_and_overwrite(self, tmp_path: Path, destination: Path) -> None:
        """tmp_path/destination 的已有目标默认保持，显式 overwrite 才替换链接目录项。"""
        source, target = tmp_path / "source", destination / "target"
        source.write_bytes(b"new")
        target.symlink_to("missing")
        with pytest.raises(OSError):
            move_file(source, target)
        assert source.read_bytes() == b"new" and target.is_symlink()
        assert move_file(source, target, overwrite=True).status is MoveStatus.COMPLETED
        assert target.read_bytes() == b"new" and not target.is_symlink()

    def test_directory_conflict(self, tmp_path: Path, destination: Path) -> None:
        """tmp_path/destination 中非空目标目录不被合并或删除；空目录可按原生规则替换。"""
        source, target = tmp_path / "tree", destination / "target"
        source.mkdir()
        (source / "new").write_bytes(b"new")
        target.mkdir()
        (target / "old").write_bytes(b"old")
        source.chmod(0o500)  # 临时副本也恢复为只读，失败回收仍须恢复本次产物的清理权限。
        with pytest.raises(OSError):
            move_file(source, target, overwrite=True)
        source.chmod(0o700)
        assert not list(destination.glob(".move-*"))
        assert (source / "new").read_bytes() == b"new" and (target / "old").read_bytes() == b"old"
        (target / "old").unlink()
        assert move_file(source, target, overwrite=True).status is MoveStatus.COMPLETED

    def test_aliases_special_and_missing(self, tmp_path: Path) -> None:
        """tmp_path 中同路径/硬链接/目录后代/特殊文件和缺失父目录全部安全拒绝。"""
        source, alias = tmp_path / "source", tmp_path / "alias"
        source.write_bytes(b"safe")
        os.link(source, alias)
        for target in (source, alias, tmp_path / "missing" / "target"):
            with pytest.raises(OSError):
                move_file(source, target, overwrite=True)
        assert source.read_bytes() == alias.read_bytes() == b"safe"
        directory = tmp_path / "tree"
        directory.mkdir()
        with pytest.raises(OSError):
            move_file(directory, directory / "child")
        fifo = tmp_path / "fifo"
        os.mkfifo(fifo)
        with pytest.raises(OSError):
            move_file(fifo, tmp_path / "target")

    @pytest.mark.parametrize(
        "phase",
        [
            "copy",
            "interrupt",
            "race",
            "change",
            "delete",
            "publish-sync",
            "source-sync",
            "rename-denied",
        ],
    )
    def test_injected_boundaries(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, phase: str
    ) -> None:
        """在 tmp_path 的 phase 阶段通过 monkeypatch 注入故障，核验发布和源保留边界。"""
        source, target = tmp_path / "source", tmp_path / "target"
        source.write_bytes(b"original")
        native = _MoveIO.rename
        published = False

        def rename(src: Path, dst: Path, overwrite: bool) -> None:
            """首次 src 重命名注入 EXDEV，其余 dst 发布仍调用真实原子操作，overwrite 原样传递。"""
            nonlocal published
            if src == source:
                raise OSError(
                    errno.EACCES if phase == "rename-denied" else errno.EXDEV,
                    "injected rename failure",
                )
            if phase == "race":
                dst.write_bytes(b"competitor")
            native(src, dst, overwrite)
            published = True

        monkeypatch.setattr(_MoveIO, "rename", rename)
        if phase in ("copy", "interrupt", "change"):
            copy = _TreeCopy.copy

            def fail_copy(tree: _TreeCopy, src: Path, dst: Path) -> None:
                """tree 复制 src 到 dst；注入部分写入后 ENOSPC，或复制完成后更改源。"""
                if phase in ("copy", "interrupt"):
                    dst.write_bytes(b"partial")
                    raise OSError(
                        errno.ENOSPC if phase == "copy" else errno.EINTR, "injected copy failure"
                    )
                copy(tree, src, dst)
                src.write_bytes(b"modified-source")

            monkeypatch.setattr(_TreeCopy, "copy", fail_copy)
        if phase == "delete":

            def fail_remove(path: Path, directory: bool) -> None:
                """拒绝 path 的删除；directory 保留真实删除调用形状。"""
                raise OSError(errno.EACCES, "injected remove failure")

            monkeypatch.setattr(_MoveIO, "remove", fail_remove)
        if phase in ("publish-sync", "source-sync"):
            sync = _MoveIO.sync_dir

            def fail_sync(path: Path) -> None:
                """发布后按 phase 决定是否拒绝 path 同步，其余仍使用真实 fsync。"""
                if published and (phase == "publish-sync" or not source.exists()):
                    raise OSError(errno.EIO, "injected directory sync failure")
                sync(path)

            monkeypatch.setattr(_MoveIO, "sync_dir", fail_sync)
        if phase in ("copy", "interrupt", "race", "change", "rename-denied"):
            with pytest.raises(OSError):
                move_file(source, target)
            assert source.exists()
            assert target.read_bytes() == b"competitor" if phase == "race" else not target.exists()
        else:
            result = move_file(source, target)
            assert result.status is (
                MoveStatus.PUBLISHED_SOURCE_REMAINS
                if phase == "delete"
                else MoveStatus.DURABILITY_UNCERTAIN
            )
            assert result.source_removed is (phase == "source-sync")
            assert target.read_bytes() == b"original" and result.error is not None
        assert not list(tmp_path.glob(".move-*"))

    @pytest.mark.parametrize("phase", ["alias-race", "sync"])
    def test_native_rename_aftermath(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, phase: str
    ) -> None:
        """在 tmp_path 的原生 rename 后检查 phase 引起的源保留或同步不确定。"""
        source, target = tmp_path / "source", tmp_path / "target"
        source.write_bytes(b"safe")
        native = _MoveIO.rename

        def rename(src: Path, dst: Path, overwrite: bool) -> None:
            """src/dst 在实际覆盖命名前模拟硬链接竞争，overwrite 直接交给内核。"""
            if phase == "alias-race":
                os.link(src, dst)
            native(src, dst, overwrite)

        def sync(path: Path) -> None:
            """对 path 注入目录同步错误，保留真实命名后的路径状态。"""
            raise OSError(errno.EIO, "injected sync failure")

        monkeypatch.setattr(_MoveIO, "rename", rename)
        if phase == "sync":
            monkeypatch.setattr(_MoveIO, "sync_dir", sync)
        result = move_file(source, target, overwrite=True)
        assert result.status is (
            MoveStatus.PUBLISHED_SOURCE_REMAINS
            if phase == "alias-race"
            else MoveStatus.DURABILITY_UNCERTAIN
        )
        assert result.source_removed is (phase == "sync")
        assert target.read_bytes() == b"safe"

    def test_partial_directory_cleanup(
        self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        """在 tmp_path 真实删除一个子项后失败，目标完整可读，源树明确只清理一部分。"""
        source, target = tmp_path / "tree", tmp_path / "target"
        source.mkdir()
        (source / "a").write_bytes(b"a")
        (source / "b").write_bytes(b"b")
        native, remove = _MoveIO.rename, _MoveIO.remove
        count = 0

        def rename(src: Path, dst: Path, overwrite: bool) -> None:
            """src 是源根时模拟 EXDEV，其他 dst 发布使用真实原子 overwrite 规则。"""
            if src == source:
                raise OSError(errno.EXDEV, "injected cross-device rename")
            native(src, dst, overwrite)

        def partial_remove(path: Path, directory: bool) -> None:
            """第一次真实删除 path，第二次失败；directory 保持实际类型语义。"""
            nonlocal count
            count += 1
            if count == 2:
                raise OSError(errno.EACCES, "injected partial cleanup failure")
            remove(path, directory)

        monkeypatch.setattr(_MoveIO, "rename", rename)
        monkeypatch.setattr(_MoveIO, "remove", partial_remove)
        result = move_file(source, target)
        assert result.status is MoveStatus.PUBLISHED_SOURCE_REMAINS and not result.source_removed
        assert (target / "a").read_bytes() == b"a" and (target / "b").read_bytes() == b"b"
        assert len(list(source.iterdir())) == 1
