"""
Runtime translation layer.

It only consumes compiled MO bytes to keep application startup fast and
deterministic; PO parsing/compilation stay in the tools layer.
"""

from __future__ import annotations

import functools
import gettext
from io import BytesIO
from typing import Optional, Set

from .context import get_locale


def normalize_locale(raw: Optional[str], default: str = "zh") -> str:
    """
    Normalize locale strings to base language codes.
    Examples: zh-CN -> zh, zh_Hans -> zh, en-US -> en.
    """
    if not raw:
        return default
    value = str(raw).strip()
    if not value:
        return default
    value = value.replace("_", "-")
    base = value.split("-")[0].lower()
    return base or default


class I18n:
    """
    Translation entry point. Keeps an internal LRU cache of loaded translations
    to avoid repeatedly parsing MO bytes.
    """

    def __init__(
        self,
        provider,
        default_locale: str = "zh",
        supported: Optional[Set[str]] = None,
    ):
        self.provider = provider
        self.default_locale = normalize_locale(default_locale, default_locale)
        self.supported = (
            {normalize_locale(loc, self.default_locale) for loc in supported} if supported else None
        )

        def _load(locale: str, domain: str):
            mo_bytes = self.provider.get_mo(locale, domain)
            if not mo_bytes:
                return gettext.NullTranslations()
            try:
                return gettext.GNUTranslations(BytesIO(mo_bytes))
            except Exception:
                # Keep runtime robust: if MO is corrupted, fall back to null
                return gettext.NullTranslations()

        self._load = functools.lru_cache(maxsize=128)(_load)

    def _pick_locale(self, locale: Optional[str]) -> str:
        normalized = normalize_locale(locale, self.default_locale)
        if self.supported is not None and normalized not in self.supported:
            return self.default_locale
        return normalized

    def t(self, msgid: str, *, domain: str = "messages", locale: Optional[str] = None) -> str:
        chosen = self._pick_locale(locale or get_locale())
        trans = self._load(chosen, domain)
        return trans.gettext(msgid)

    def tn(
        self,
        msgid: str,
        msgid_plural: str,
        n: int,
        *,
        domain: str = "messages",
        locale: Optional[str] = None,
    ) -> str:
        chosen = self._pick_locale(locale or get_locale())
        trans = self._load(chosen, domain)
        return trans.ngettext(msgid, msgid_plural, n)

    def tc(
        self,
        context: str,
        msgid: str,
        *,
        domain: str = "messages",
        locale: Optional[str] = None,
    ) -> str:
        """
        Contextual translation. gettext encodes context as `<ctx>\\x04<msgid>`.
        When missing, return the original `msgid` (not the encoded key).
        """
        chosen = self._pick_locale(locale or get_locale())
        trans = self._load(chosen, domain)
        key = f"{context}\x04{msgid}"
        translated = trans.gettext(key)
        return msgid if translated == key else translated
