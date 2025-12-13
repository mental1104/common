"""
Shared placeholder helpers for runtime and tools.
"""

from __future__ import annotations

import re
from typing import Set, Tuple

PLACEHOLDER_PATTERN = re.compile(r"\{([a-zA-Z0-9_]+)\}")


def extract_placeholders(text: str) -> Set[str]:
    """
    Extract `{name}` placeholders from a message string.
    """
    return set(PLACEHOLDER_PATTERN.findall(text or ""))


def compare_placeholders(msgid: str, msgstr: str) -> Tuple[Set[str], Set[str]]:
    """
    Compare placeholders between source and translation.
    Returns (missing_in_translation, extra_in_translation).
    """
    src = extract_placeholders(msgid)
    tgt = extract_placeholders(msgstr)
    return src - tgt, tgt - src
