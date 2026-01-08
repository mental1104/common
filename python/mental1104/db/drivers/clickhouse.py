from __future__ import annotations

from typing import Any, Mapping, Optional
from urllib.parse import quote_plus

from ..config import ConnParams


def _auth(user: Optional[str], password: Optional[str]) -> str:
    if not user:
        return ""
    if password is None:
        return f"{quote_plus(user)}:@"
    return f"{quote_plus(user)}:{quote_plus(password)}@"


def _qs(query: Optional[Mapping[str, Any]]) -> str:
    if not query:
        return ""
    parts = []
    for key, value in query.items():
        if value is None:
            continue
        parts.append(f"{quote_plus(str(key))}={quote_plus(str(value))}")
    return ("?" + "&".join(parts)) if parts else ""


def build_url(params: ConnParams) -> str:
    """
    Default dialect is clickhousedb (clickhouse-connect).

    options:
      - scheme: "clickhousedb", "clickhousedb+connect", "clickhouse+http", "clickhouse+native", etc.
      - query: mapping of URL query params
      - distributed_force_global: True -> inject prefer_global_in_and_join=1
      - distributed_product_mode: optional (e.g. "global")
    """
    options = dict(params.options or {})
    scheme = str(options.get("scheme", "clickhousedb"))
    if scheme.startswith("clickhousedb"):
        try:
            import clickhouse_connect  # noqa: F401
        except Exception as exc:
            raise RuntimeError(
                "Missing dependency: clickhouse-connect (provides the clickhousedb dialect)."
            ) from exc
    host = params.ip
    port = params.port or 8123
    database = params.database or "default"
    auth = _auth(params.user, params.password)

    query = dict(options.get("query", {}) or {})
    if bool(options.get("distributed_force_global", False)):
        query.setdefault("prefer_global_in_and_join", 1)
    if options.get("distributed_product_mode") is not None:
        query.setdefault("distributed_product_mode", options.get("distributed_product_mode"))

    return f"{scheme}://{auth}{host}:{port}/{database}{_qs(query)}"
