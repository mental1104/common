"""
Generate MO bytes compatible with gettext.GNUTranslations.
"""

from __future__ import annotations

import struct
from io import BytesIO
from typing import TYPE_CHECKING, Dict, Iterable, Tuple

if TYPE_CHECKING:
    from .po_parser import PoEntry


def _build_catalog(entries: Iterable[PoEntry]) -> Dict[str, str]:
    catalog: Dict[str, str] = {}
    for entry in entries:
        key = entry.msgid
        if entry.context:
            key = f"{entry.context}\x04{entry.msgid}"
        msgstr = entry.msgstr
        if isinstance(msgstr, dict):
            translation = "\x00".join(msgstr.get(idx, "") for idx in sorted(msgstr))
        else:
            translation = msgstr
        catalog[key] = translation
    # gettext defaults to ASCII if the header lacks charset; enforce UTF-8 so
    # non-ASCII translations do not explode at runtime.
    header = catalog.get("", "")
    header_lower = header.lower()
    if "charset=" not in header_lower:
        extra = "" if header.endswith("\n") or not header else "\n"
        header = (
            f"{header}{extra}Content-Type: text/plain; charset=UTF-8\n"
            if header
            else "Content-Type: text/plain; charset=UTF-8\n"
        )
        catalog[""] = header
    return catalog


def write_mo(entries: Iterable[PoEntry]) -> bytes:
    """
    Build a binary MO representation from PO entries.
    """
    catalog = _build_catalog(entries)
    keys = sorted(catalog.keys())
    ids = [key.encode("utf-8") for key in keys]
    strs = [catalog[key].encode("utf-8") for key in keys]

    n = len(keys)
    header_size = 7 * 4  # 7 unsigned integers
    orig_table_offset = header_size
    trans_table_offset = orig_table_offset + n * 8
    string_offset = trans_table_offset + n * 8

    orig_table: list[Tuple[int, int]] = []
    for msgid in ids:
        orig_table.append((len(msgid), string_offset))
        string_offset += len(msgid) + 1  # null terminator

    trans_table: list[Tuple[int, int]] = []
    for msgstr in strs:
        trans_table.append((len(msgstr), string_offset))
        string_offset += len(msgstr) + 1

    output = BytesIO()
    output.write(
        struct.pack("<IIIIIII", 0x950412DE, 0, n, orig_table_offset, trans_table_offset, 0, 0)
    )
    for length, offset in orig_table:
        output.write(struct.pack("<II", length, offset))
    for length, offset in trans_table:
        output.write(struct.pack("<II", length, offset))
    for msgid in ids:
        output.write(msgid + b"\0")
    for msgstr in strs:
        output.write(msgstr + b"\0")
    return output.getvalue()
