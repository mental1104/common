from __future__ import annotations

import os
import re
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence, Union

# 用法: 规则返回新文件名/Path, 返回 None 表示跳过该文件。
RenameRule = Callable[[Path], Union[str, Path, None]]
# 用法: 带序号的规则, (path, index) -> 新文件名/Path 或 None。
IndexedRenameRule = Callable[[Path, int], Union[str, Path, None]]


# 用法: 一个重命名操作, 表示 src -> dst。
@dataclass(frozen=True)
class RenameOp:
    src: Path
    dst: Path


# 用法: list_files("dir", recursive=True) 返回目录内文件 Path 列表。
def list_files(
    directory: Path | str,
    *,
    recursive: bool = False,
    sort_key: Callable[[Path], Any] | None = str,
    predicate: Callable[[Path], bool] | None = None,
) -> list[Path]:
    """Return file paths in a directory, optionally recursive and filtered."""
    base = Path(directory)
    if not base.is_dir():
        raise NotADirectoryError(str(base))
    if recursive:
        paths = [path for path in base.rglob("*") if path.is_file()]
    else:
        paths = [path for path in base.iterdir() if path.is_file()]
    if predicate is not None:
        paths = [path for path in paths if predicate(path)]
    if sort_key is not None:
        paths = sorted(paths, key=sort_key)
    return paths


# 用法: build_rename_plan(paths, rule, root=...) 生成重命名计划。
def build_rename_plan(
    paths: Iterable[Path | str],
    rename: RenameRule,
    *,
    root: Path | str | None = None,
    skip_same: bool = True,
) -> list[RenameOp]:
    """Build rename operations using a rule that maps a path to a new name."""
    root_path = Path(root) if root is not None else None
    plan: list[RenameOp] = []
    for path in paths:
        src = Path(path)
        target = rename(src)
        if target is None:
            continue
        dst = _resolve_destination(src, target, root_path)
        if skip_same and dst == src:
            continue
        plan.append(RenameOp(src=src, dst=dst))
    validate_rename_plan(plan)
    return plan


# 用法: build_indexed_rename_plan(paths, rule, start=0) 生成带序号的计划。
def build_indexed_rename_plan(
    paths: Sequence[Path | str],
    rename: IndexedRenameRule,
    *,
    root: Path | str | None = None,
    skip_same: bool = True,
    start: int = 0,
) -> list[RenameOp]:
    """Build rename operations with a rule that receives a stable index."""
    root_path = Path(root) if root is not None else None
    plan: list[RenameOp] = []
    for offset, path in enumerate(paths):
        src = Path(path)
        target = rename(src, start + offset)
        if target is None:
            continue
        dst = _resolve_destination(src, target, root_path)
        if skip_same and dst == src:
            continue
        plan.append(RenameOp(src=src, dst=dst))
    validate_rename_plan(plan)
    return plan


# 用法: plan_directory_rename(dir, rule) 直接对目录生成计划。
def plan_directory_rename(
    directory: Path | str,
    rename: RenameRule,
    *,
    recursive: bool = False,
    sort_key: Callable[[Path], Any] | None = str,
    predicate: Callable[[Path], bool] | None = None,
    root: Path | str | None = None,
    skip_same: bool = True,
) -> list[RenameOp]:
    """List files under a directory and build a rename plan."""
    paths = list_files(
        directory, recursive=recursive, sort_key=sort_key, predicate=predicate
    )
    return build_rename_plan(paths, rename, root=root, skip_same=skip_same)


# 用法: plan_directory_rename_indexed(dir, rule) 按索引生成计划。
def plan_directory_rename_indexed(
    directory: Path | str,
    rename: IndexedRenameRule,
    *,
    recursive: bool = False,
    sort_key: Callable[[Path], Any] | None = str,
    predicate: Callable[[Path], bool] | None = None,
    root: Path | str | None = None,
    skip_same: bool = True,
    start: int = 0,
) -> list[RenameOp]:
    """List files under a directory and build an indexed rename plan."""
    paths = list_files(
        directory, recursive=recursive, sort_key=sort_key, predicate=predicate
    )
    return build_indexed_rename_plan(
        paths, rename, root=root, skip_same=skip_same, start=start
    )


# 用法: apply_rename_plan(plan, dry_run=True/False, conflict_policy="raise|skip")。
def apply_rename_plan(
    plan: Sequence[RenameOp],
    *,
    dry_run: bool = False,
    conflict_policy: str = "raise",
    create_dirs: bool = False,
    rename_func: Callable[[os.PathLike | str, os.PathLike | str], Any] = os.rename,
    temp_prefix: str = ".rename_tmp_",
) -> list[RenameOp]:
    """Apply rename operations with collision-safe two-phase renames."""
    ops = [op for op in plan if op.src != op.dst]
    if not ops:
        return []
    validate_rename_plan(ops)
    if conflict_policy not in {"raise", "skip"}:
        raise ValueError("conflict_policy must be 'raise' or 'skip'")
    missing = [op.src for op in ops if not op.src.exists()]
    if missing:
        missing_list = ", ".join(str(path) for path in missing)
        raise FileNotFoundError(f"Missing rename sources: {missing_list}")
    if conflict_policy == "raise":
        src_set = {op.src for op in ops}
        conflicts = [op for op in ops if op.dst.exists() and op.dst not in src_set]
        if conflicts:
            conflict_list = ", ".join(str(op.dst) for op in conflicts)
            raise FileExistsError(f"Rename destination already exists: {conflict_list}")
    else:
        ops = list(ops)
        while True:
            src_set = {op.src for op in ops}
            conflicts = [op for op in ops if op.dst.exists() and op.dst not in src_set]
            if not conflicts:
                break
            ops = [op for op in ops if op not in conflicts]
            if not ops:
                return []
        validate_rename_plan(ops)
    if dry_run:
        return list(ops)
    temp_ops = _build_temp_ops(ops, temp_prefix)
    for temp_op in temp_ops:
        if create_dirs:
            temp_op.dst.parent.mkdir(parents=True, exist_ok=True)
        rename_func(temp_op.src, temp_op.dst)
    for op, temp_op in zip(ops, temp_ops):
        if create_dirs:
            op.dst.parent.mkdir(parents=True, exist_ok=True)
        rename_func(temp_op.dst, op.dst)
    return list(ops)


# 用法: rename_with_suffix(".m4a") 仅替换后缀名。
def rename_with_suffix(suffix: str) -> RenameRule:
    """Return a rule that replaces the file suffix while keeping the stem."""
    normalized = _normalize_suffix(suffix)

    def _rename(path: Path) -> str:
        return path.with_suffix(normalized).name

    return _rename


# 用法: rename_with_regex_group(r"\\[(\\d{3})\\]", suffix=".mp4")。
def rename_with_regex_group(
    pattern: str,
    group: int | str = 1,
    *,
    suffix: str | None = None,
    flags: int = 0,
) -> RenameRule:
    """Return a rule that extracts a regex group from the filename."""
    regex = re.compile(pattern, flags)
    normalized_suffix = None if suffix is None else _normalize_suffix(suffix)

    def _rename(path: Path) -> str | None:
        match = regex.search(path.name)
        if not match:
            return None
        token = match.group(group)
        new_suffix = normalized_suffix if normalized_suffix is not None else path.suffix
        return f"{token}{new_suffix}"

    return _rename


# 用法: rename_with_index(start=1, width=3, suffix=".dat") 生成序号名。
def rename_with_index(
    *,
    start: int = 0,
    width: int | None = None,
    prefix: str = "",
    suffix: str | None = None,
) -> IndexedRenameRule:
    """Return a rule that renames files to a sequential index."""
    if width is not None and width < 0:
        raise ValueError("width must be >= 0")
    normalized_suffix = None if suffix is None else _normalize_suffix(suffix)

    def _rename(path: Path, index: int) -> str:
        value = index + start
        label = f"{value:0{width}d}" if width else str(value)
        new_suffix = normalized_suffix if normalized_suffix is not None else path.suffix
        return f"{prefix}{label}{new_suffix}"

    return _rename


# 用法: validate_rename_plan(plan) 检查重复源/目标。
def validate_rename_plan(plan: Sequence[RenameOp]) -> None:
    """Validate rename operations for duplicate sources or destinations."""
    src_seen: set[Path] = set()
    dst_seen: set[Path] = set()
    for op in plan:
        if op.src in src_seen:
            raise ValueError(f"Duplicate source in plan: {op.src}")
        if op.dst in dst_seen:
            raise ValueError(f"Duplicate destination in plan: {op.dst}")
        src_seen.add(op.src)
        dst_seen.add(op.dst)


# 用法: 计算目标路径, 支持绝对路径/相对路径/指定 root。
def _resolve_destination(
    src: Path, target: str | Path, root: Path | None
) -> Path:
    dst = Path(target)
    if dst.is_absolute():
        return dst
    if root is not None:
        return root / dst
    if dst.parent == Path("."):
        return src.with_name(dst.name)
    return src.parent / dst


# 用法: 为每个操作生成临时路径, 支持两阶段重命名。
def _build_temp_ops(plan: Sequence[RenameOp], temp_prefix: str) -> list[RenameOp]:
    used = {op.src for op in plan} | {op.dst for op in plan}
    temp_ops: list[RenameOp] = []
    for op in plan:
        temp_path = _unique_temp_path(op.src, used, temp_prefix)
        used.add(temp_path)
        temp_ops.append(RenameOp(src=op.src, dst=temp_path))
    return temp_ops


# 用法: 基于随机 token 生成唯一临时文件名。
def _unique_temp_path(src: Path, used: set[Path], temp_prefix: str) -> Path:
    while True:
        token = uuid.uuid4().hex
        candidate = src.with_name(f"{temp_prefix}{token}{src.suffix}")
        if candidate not in used and not candidate.exists():
            return candidate


# 用法: normalize ".mp4"/"mp4"/"" 统一后缀格式。
def _normalize_suffix(suffix: str) -> str:
    if suffix == "":
        return ""
    return suffix if suffix.startswith(".") else f".{suffix}"


__all__ = [
    "RenameOp",
    "apply_rename_plan",
    "build_indexed_rename_plan",
    "build_rename_plan",
    "list_files",
    "plan_directory_rename",
    "plan_directory_rename_indexed",
    "rename_with_index",
    "rename_with_regex_group",
    "rename_with_suffix",
    "validate_rename_plan",
]
