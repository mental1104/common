from __future__ import annotations

from urllib.parse import quote_plus

from ..config import ConnParams


def build_url(params: ConnParams) -> str:
    is_async = bool(params.options.get("is_async", False))
    driver_key = "async_driver" if is_async else "driver"
    driver = str(params.options.get(driver_key, "pymysql"))

    host = params.ip
    port = params.port or 3306
    database = params.database or ""

    user = params.user or ""
    password = params.password or ""

    auth = ""
    if user:
        if password:
            auth = f"{quote_plus(user)}:{quote_plus(password)}@"
        else:
            auth = f"{quote_plus(user)}@"

    query = {"charset": "utf8mb4"}
    query.update(dict(params.options.get("query", {}) or {}))
    suffix = "?" + "&".join(
        [f"{k}={quote_plus(str(v))}" for k, v in query.items()]
    ) if query else ""

    return f"mysql+{driver}://{auth}{host}:{port}/{database}{suffix}"
