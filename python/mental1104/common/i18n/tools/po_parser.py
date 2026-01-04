"""
Minimal PO parser supporting context, plural forms, fuzzy flags, and multiline
strings. Comments are ignored.
"""

from __future__ import annotations

import ast
from dataclasses import dataclass
from typing import Dict, List, Optional, Set, Union


@dataclass
class PoEntry:
    context: Optional[str]
    msgid: str
    msgid_plural: Optional[str]
    msgstr: Union[str, Dict[int, str]]
    flags: Set[str]


def _parse_quoted(raw: str) -> str:
    raw = raw.strip()
    if not raw:
        return ""
    try:
        return ast.literal_eval(raw)
    except Exception:
        # Fallback: strip surrounding quotes if literal_eval fails
        return raw.strip('"')


def parse_po(po_text: str) -> List[PoEntry]:
    entries: List[PoEntry] = []
    pending_flags: Set[str] = set()
    current = {"context": None, "msgid": None, "msgid_plural": None, "msgstr": None, "flags": set()}
    current_field = None  # (field, index)

    def ensure_flags():
        nonlocal pending_flags
        if pending_flags:
            current["flags"].update(pending_flags)
            pending_flags = set()

    def finish_entry():
        nonlocal current
        if current["msgid"] is None and current["msgstr"] is None and current["context"] is None:
            return
        entries.append(
            PoEntry(
                context=current["context"],
                msgid=current["msgid"] or "",
                msgid_plural=current["msgid_plural"],
                msgstr=current["msgstr"] if current["msgstr"] is not None else "",
                flags=set(current["flags"]),
            )
        )
        current = {
            "context": None,
            "msgid": None,
            "msgid_plural": None,
            "msgstr": None,
            "flags": set(),
        }

    for line in po_text.splitlines():
        stripped = line.strip()
        if not stripped:
            finish_entry()
            current_field = None
            pending_flags = set()
            continue
        if stripped.startswith("#"):
            if stripped.startswith("#,"):
                flags = stripped[2:].split(",")
                pending_flags.update(flag.strip() for flag in flags if flag.strip())
            continue
        if stripped.startswith("msgctxt "):
            ensure_flags()
            current["context"] = _parse_quoted(stripped[len("msgctxt ") :])
            current_field = ("context", None)
            continue
        if stripped.startswith("msgid_plural "):
            ensure_flags()
            current["msgid_plural"] = _parse_quoted(stripped[len("msgid_plural ") :])
            current_field = ("msgid_plural", None)
            continue
        if stripped.startswith("msgid "):
            ensure_flags()
            current["msgid"] = _parse_quoted(stripped[len("msgid ") :])
            current_field = ("msgid", None)
            continue
        if stripped.startswith("msgstr["):
            ensure_flags()
            idx_end = stripped.find("]")
            idx = int(stripped[len("msgstr[") : idx_end])
            text = stripped[idx_end + 1 :].strip()
            if current["msgstr"] is None or not isinstance(current["msgstr"], dict):
                current["msgstr"] = {}
            current["msgstr"][idx] = _parse_quoted(text)
            current_field = ("msgstr", idx)
            continue
        if stripped.startswith("msgstr "):
            ensure_flags()
            text = stripped[len("msgstr ") :]
            current["msgstr"] = _parse_quoted(text)
            current_field = ("msgstr", None)
            continue
        if stripped.startswith('"') and current_field:
            fragment = _parse_quoted(stripped)
            field, idx = current_field
            if field == "msgstr":
                if isinstance(current["msgstr"], dict):
                    current["msgstr"][idx] = (current["msgstr"].get(idx, "") or "") + fragment
                else:
                    current["msgstr"] = (current["msgstr"] or "") + fragment
            elif field == "msgid":
                current["msgid"] = (current["msgid"] or "") + fragment
            elif field == "msgid_plural":
                current["msgid_plural"] = (current["msgid_plural"] or "") + fragment
            elif field == "context":
                current["context"] = (current["context"] or "") + fragment
    finish_entry()
    return entries
