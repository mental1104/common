from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Mapping, Optional
from urllib.parse import urlparse

from .config import ConnParams
from .session_context import (
    get_current_clickhouse_session,
    reset_current_clickhouse_session,
    set_current_clickhouse_session,
)


@dataclass
class ClickHouseExecutor:
    client: Any

    def execute(self, sql: str, params: Optional[Mapping[str, Any]] = None) -> Any:
        return self.client.command(sql, parameters=params or {})

    def select(self, sql: str, params: Optional[Mapping[str, Any]] = None):
        result = self.client.query(sql, parameters=params or {})
        return result.result_rows

    def close(self) -> None:
        try:
            self.client.close()
        except Exception:
            pass


class ClickHouseSessionAware:
    def _session(self) -> ClickHouseExecutor:
        current = get_current_clickhouse_session()
        if current is None:
            raise RuntimeError(
                "No current ClickHouse session. Use clickhouse_session_scope()/clickhouse_tx_scope()."
            )
        return current


def _parse_clickhouse_dsn(dsn: str):
    parsed = urlparse(dsn)
    return {
        "host": parsed.hostname,
        "port": parsed.port,
        "username": parsed.username,
        "password": parsed.password,
        "database": parsed.path.lstrip("/") or None,
    }


def make_clickhouse_executor(
    dsn: Optional[str],
    params: Optional[ConnParams],
    options: Mapping[str, Any],
) -> ClickHouseExecutor:
    try:
        import clickhouse_connect
    except Exception as exc:
        raise RuntimeError("Missing dependency: clickhouse-connect") from exc

    kwargs = dict(options or {})
    if dsn:
        kwargs.update({k: v for k, v in _parse_clickhouse_dsn(dsn).items() if v is not None})
    elif params:
        kwargs.setdefault("host", params.ip)
        kwargs.setdefault("port", params.port or 8123)
        kwargs.setdefault("username", params.user or None)
        kwargs.setdefault("password", params.password or None)
        kwargs.setdefault("database", params.database or "default")
    else:
        raise ValueError("dsn or params must be provided for clickhouse-connect")

    client = clickhouse_connect.get_client(**kwargs)
    return ClickHouseExecutor(client=client)


class _ClickHouseSessionScope:
    def __init__(self, executor: ClickHouseExecutor):
        self._executor = executor
        self._token = None

    def __enter__(self) -> ClickHouseExecutor:
        self._token = set_current_clickhouse_session(self._executor)
        return self._executor

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._token is not None:
            reset_current_clickhouse_session(self._token)


def clickhouse_session_scope(executor: ClickHouseExecutor) -> _ClickHouseSessionScope:
    return _ClickHouseSessionScope(executor)


def clickhouse_tx_scope(executor: ClickHouseExecutor) -> _ClickHouseSessionScope:
    # ClickHouse does not support multi-statement transactions; this is a no-op wrapper.
    return _ClickHouseSessionScope(executor)
