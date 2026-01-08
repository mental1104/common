from __future__ import annotations

from ..config import ConnParams


def build_url(params: ConnParams) -> str:
    # sync: sqlite+pysqlite
    # async: sqlite+aiosqlite
    is_async = bool(params.options.get("is_async", False))
    driver = "aiosqlite" if is_async else "pysqlite"

    path = (params.ip or "").strip() or "app.sqlite3"
    if path == ":memory:":
        return f"sqlite+{driver}:///:memory:"
    return f"sqlite+{driver}:///{path}"
