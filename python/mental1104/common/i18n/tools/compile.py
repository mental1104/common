"""
Compile PO resources into MO files.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Iterable, Optional, Set, Tuple

from .mo_writer import write_mo
from .po_parser import parse_po


def po_text_to_mo_bytes(po_text: str) -> bytes:
    entries = parse_po(po_text)
    return write_mo(entries)


def _discover_po_files(
    po_root: Path, locales: Optional[Set[str]], domains: Optional[Set[str]]
) -> Iterable[Tuple[str, str, Path]]:
    for locale_dir in po_root.iterdir():
        if not locale_dir.is_dir():
            continue
        locale = locale_dir.name
        if locales and locale not in locales:
            continue
        lc_messages = locale_dir / "LC_MESSAGES"
        if not lc_messages.is_dir():
            continue
        for po_path in lc_messages.glob("*.po"):
            domain = po_path.stem
            if domains and domain not in domains:
                continue
            yield locale, domain, po_path


def _compile_with_msgfmt(msgfmt_path: str, po_path: Path, mo_path: Path) -> bool:
    tmp_fd, tmp_name = tempfile.mkstemp(dir=mo_path.parent, prefix="mo_", suffix=".mo")
    os.close(tmp_fd)
    try:
        result = subprocess.run(
            [msgfmt_path, "-o", tmp_name, str(po_path)],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            return False
        os.replace(tmp_name, mo_path)
        return True
    finally:
        if os.path.exists(tmp_name):
            os.remove(tmp_name)


def _compile_with_python(po_path: Path, mo_path: Path):
    po_text = po_path.read_text(encoding="utf-8")
    mo_bytes = po_text_to_mo_bytes(po_text)
    mo_path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=mo_path.parent, delete=False) as tmp:
        tmp.write(mo_bytes)
        tmp.flush()
        os.fsync(tmp.fileno())
        tmp_name = tmp.name
    os.replace(tmp_name, mo_path)


def compile_po_tree(
    po_root: str | Path,
    mo_root: str | Path,
    domains: Optional[Set[str]] = None,
    locales: Optional[Set[str]] = None,
    use_msgfmt_if_available: bool = True,
):
    po_root = Path(po_root)
    mo_root = Path(mo_root)
    msgfmt_path = shutil.which("msgfmt") if use_msgfmt_if_available else None

    for locale, domain, po_path in _discover_po_files(po_root, domains, locales):
        mo_path = mo_root / locale / "LC_MESSAGES" / f"{domain}.mo"
        mo_path.parent.mkdir(parents=True, exist_ok=True)
        if msgfmt_path:
            success = _compile_with_msgfmt(msgfmt_path, po_path, mo_path)
            if success:
                continue
        _compile_with_python(po_path, mo_path)
