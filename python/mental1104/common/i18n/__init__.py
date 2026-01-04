"""
i18n runtime and tooling for mental1104 projects.

Runtime is MO-only to keep application startup predictable; PO parsing and
compilation live in the tools layer for CI/publishing.
"""

from .context import activate, get_locale, locale_context, reset_locale
from .json_localize import localize_json
from .placeholder import compare_placeholders, extract_placeholders
from .provider import FileMoProvider, I18nResourceProvider
from .runtime import I18n, normalize_locale

__all__ = [
    "FileMoProvider",
    "I18n",
    "I18nResourceProvider",
    "activate",
    "compare_placeholders",
    "extract_placeholders",
    "get_locale",
    "locale_context",
    "localize_json",
    "normalize_locale",
    "reset_locale",
]
