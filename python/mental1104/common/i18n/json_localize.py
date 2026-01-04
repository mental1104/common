"""
Recursive JSON localization helper.

The rule: default language lives in the base key (e.g., `name`), other
languages live in suffixed siblings (e.g., `name_en`). At runtime we overwrite
the base with the active locale and drop suffixed keys so callers do not need
branching logic.
"""

from __future__ import annotations

from typing import Any, Dict, Iterable

from .context import get_locale
from .runtime import normalize_locale

DEFAULT_SUFFIX_MAP: Dict[str, str] = {
    "zh": "_zh",
    "en": "_en",
    "ja": "_ja",
}


def _has_suffix(key: str, suffixes: Iterable[str]) -> bool:
    return any(key.endswith(suffix) for suffix in suffixes)


def localize_json(
    obj: Any,
    *,
    locale: str | None = None,
    default_locale: str = "zh",
    suffix_map: Dict[str, str] | None = None,
) -> Any:
    suffix_map = suffix_map or DEFAULT_SUFFIX_MAP
    target_locale = normalize_locale(locale or get_locale(), default_locale)
    suffix = suffix_map.get(target_locale, f"_{target_locale}")
    known_suffixes = set(suffix_map.values())

    def _walk(value: Any) -> Any:
        if isinstance(value, dict):
            result = {}
            keys = set(value.keys())
            base_keys = {k for k in keys if not _has_suffix(k, known_suffixes)}
            for base_key in base_keys:
                override_key = f"{base_key}{suffix}"
                override_val = value.get(override_key)
                base_val = value.get(base_key)
                chosen = base_val
                if override_val not in (None, ""):
                    chosen = override_val
                elif base_val is None and override_key in value:
                    # No base value but override exists: adopt override
                    chosen = override_val
                result[base_key] = _walk(chosen)

            # Handle suffix-only keys when the base is missing
            for key in keys:
                for known_suffix in known_suffixes:
                    if key.endswith(known_suffix):
                        base = key[: -len(known_suffix)]
                        if base not in result and base not in base_keys and known_suffix == suffix:
                            result[base] = _walk(value[key])
                        break
            return result
        if isinstance(value, list):
            return [_walk(item) for item in value]
        return value

    return _walk(obj)
