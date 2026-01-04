"""
Validate PO files for placeholder consistency and common quality issues.
"""

from __future__ import annotations

from pathlib import Path
from typing import List, Set, Tuple

from ..placeholder import extract_placeholders
from .po_parser import PoEntry, parse_po


def _discover_po_files(po_root: Path) -> List[Tuple[str, str, Path]]:
    result: List[Tuple[str, str, Path]] = []
    for locale_dir in po_root.iterdir():
        if not locale_dir.is_dir():
            continue
        locale = locale_dir.name
        lc_messages = locale_dir / "LC_MESSAGES"
        if not lc_messages.is_dir():
            continue
        for po_path in lc_messages.glob("*.po"):
            domain = po_path.stem
            result.append((locale, domain, po_path))
    return result


def _check_entry(
    entry: PoEntry,
    locale: str,
    domain: str,
    po_path: Path,
    strict: bool,
) -> Tuple[List[str], List[str]]:
    errors: List[str] = []
    warnings: List[str] = []
    identifier = f"{po_path}: {locale}/{domain} msgid={entry.msgid!r}"
    source_placeholders = extract_placeholders(entry.msgid)
    if entry.msgid_plural:
        source_placeholders |= extract_placeholders(entry.msgid_plural)

    def record(issue: str, warning: bool = False):
        target = warnings if warning else errors
        target.append(f"{identifier} - {issue}")

    if "fuzzy" in entry.flags:
        record("fuzzy translation", warning=not strict)

    if entry.msgid == "":
        return errors, warnings  # header entry; skip placeholder/empty checks

    if isinstance(entry.msgstr, dict):
        for idx, text in sorted(entry.msgstr.items()):
            if text == "":
                record(f"empty plural form [{idx}]", warning=not strict)
            target_placeholders = extract_placeholders(text)
            missing = source_placeholders - target_placeholders
            extra = target_placeholders - source_placeholders
            if missing or extra:
                record(f"placeholders mismatch in plural[{idx}] missing={missing} extra={extra}")
    else:
        if entry.msgstr == "":
            record("empty translation", warning=not strict)
        target_placeholders = extract_placeholders(entry.msgstr)
        missing = source_placeholders - target_placeholders
        extra = target_placeholders - source_placeholders
        if missing or extra:
            record(f"placeholders mismatch missing={missing} extra={extra}")

    return errors, warnings


def check_po_tree(po_root: str | Path, strict: bool = True):
    """
    Validate PO files under `<po_root>/<locale>/LC_MESSAGES/*.po`.
    Placeholder mismatches and duplicates always raise. Empty/fuzzy entries are
    treated as errors in strict mode and warnings otherwise.
    """
    po_root = Path(po_root)
    errors: List[str] = []
    warnings: List[str] = []
    seen_keys: Set[Tuple[str, str, str, str]] = set()

    for locale, domain, po_path in _discover_po_files(po_root):
        entries = parse_po(po_path.read_text(encoding="utf-8"))
        for entry in entries:
            key = (locale, domain, entry.context or "", entry.msgid)
            if key in seen_keys:
                errors.append(f"{po_path}: duplicate key {key}")
            else:
                seen_keys.add(key)
            entry_errors, entry_warnings = _check_entry(entry, locale, domain, po_path, strict)
            errors.extend(entry_errors)
            warnings.extend(entry_warnings)

    if errors:
        raise ValueError("PO check failed:\n" + "\n".join(errors + warnings))
    if not strict:
        return warnings
    return warnings
