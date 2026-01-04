"""
Locale management using contextvars so async tasks inherit the active locale
without passing language parameters everywhere.
"""

from __future__ import annotations

from contextlib import contextmanager
from contextvars import ContextVar, Token

DEFAULT_LOCALE = "zh"
_locale_var: ContextVar[str] = ContextVar("locale", default=DEFAULT_LOCALE)


def get_locale() -> str:
    """
    Return the currently active locale from the context.
    """
    return _locale_var.get()


def activate(locale: str) -> Token[str]:
    """
    Set the current locale for the active context. Returns the context token so
    callers can restore the previous value if needed.
    """
    return _locale_var.set(locale)


def reset_locale(token: Token[str]) -> None:
    """
    Restore the locale to a previous token.
    """
    _locale_var.reset(token)


@contextmanager
def locale_context(locale: str):
    """
    Temporarily set the locale inside the context, restoring the previous
    value afterwards. Useful for tests or scoped overrides.
    """
    token = activate(locale)
    try:
        yield locale
    finally:
        reset_locale(token)
