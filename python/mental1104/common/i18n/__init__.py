"""
i18n runtime and tooling for mental1104 projects.

Runtime is MO-only to keep application startup predictable; PO parsing and
compilation live in the tools layer for CI/publishing.
"""

from .context import activate, get_locale, locale_context, reset_locale
from .runtime import I18n, normalize_locale
from .provider import FileMoProvider, I18nResourceProvider
from .json_localize import localize_json
from .placeholder import compare_placeholders, extract_placeholders

__all__ = [
    "activate",
    "get_locale",
    "locale_context",
    "reset_locale",
    "I18n",
    "normalize_locale",
    "FileMoProvider",
    "I18nResourceProvider",
    "localize_json",
    "compare_placeholders",
    "extract_placeholders",
]
