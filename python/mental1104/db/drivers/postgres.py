from __future__ import annotations

from urllib.parse import quote_plus

from ..config import ConnParams


def build_url(params: ConnParams) -> str:
    is_async = bool(params.options.get("is_async", False))
    driver_key = "async_driver" if is_async else "driver"
    driver = str(params.options.get(driver_key, "psycopg"))

    host = params.ip
    port = params.port or 5432
    database = params.database or "postgres"

    user = params.user or ""
    password = params.password or ""

    auth = ""
    if user:
        if password:
            auth = f"{quote_plus(user)}:{quote_plus(password)}@"
        else:
            auth = f"{quote_plus(user)}@"

    query = dict(params.options.get("query", {}) or {})
    suffix = ""
    if query:
        suffix = "?" + "&".join(
            [f"{k}={quote_plus(str(v))}" for k, v in query.items()]
        )

    return f"postgresql+{driver}://{auth}{host}:{port}/{database}{suffix}"
