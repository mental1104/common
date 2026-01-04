"""
Resource provider abstraction so runtime stays decoupled from storage.
File provider follows the standard `<root>/<locale>/LC_MESSAGES/<domain>.mo`
layout that gettext tooling expects.
"""

from __future__ import annotations

from pathlib import Path
from typing import Optional, Protocol


class I18nResourceProvider(Protocol):
    """
    Protocol describing how MO bytes are fetched. Using a Protocol keeps the
    runtime testable and allows custom storage backends.
    """

    def get_mo(self, locale: str, domain: str) -> Optional[bytes]:  # pragma: no cover - Protocol
        ...


class FileMoProvider:
    """
    Load `.mo` files from disk using the gettext directory layout.
    """

    def __init__(self, mo_root: Path):
        self.mo_root = Path(mo_root)

    def get_mo(self, locale: str, domain: str) -> Optional[bytes]:
        path = self.mo_root / locale / "LC_MESSAGES" / f"{domain}.mo"
        if not path.exists():
            return None
        return path.read_bytes()
