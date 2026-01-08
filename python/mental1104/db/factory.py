from __future__ import annotations

from typing import Any, Dict, Optional, Tuple

from .client_async import AsyncSQLAlchemyClient, make_async_sqlalchemy_client
from .client_sync import SQLAlchemyClient, make_sqlalchemy_client
from .config import ConnParams, DBKind, SASettings
from .drivers.clickhouse import build_url as clickhouse_url
from .drivers.mysql import build_url as mysql_url
from .drivers.postgres import build_url as postgres_url
from .drivers.sqlite import build_url as sqlite_url


def _resolve_sa(params: ConnParams, sa: SASettings) -> Tuple[bool, int, int, int, int, bool]:
    overrides = dict(params.options.get("sa", {}) or {})
    echo = bool(overrides.get("echo", sa.echo))
    pool_size = int(overrides.get("pool_size", sa.pool_size))
    max_overflow = int(overrides.get("max_overflow", sa.max_overflow))
    pool_timeout = int(overrides.get("pool_timeout", sa.pool_timeout))
    pool_recycle = int(overrides.get("pool_recycle", sa.pool_recycle))
    pool_pre_ping = bool(overrides.get("pool_pre_ping", sa.pool_pre_ping))
    return echo, pool_size, max_overflow, pool_timeout, pool_recycle, pool_pre_ping


def create_sqlalchemy_client(
    kind: DBKind,
    params: ConnParams,
    sa: Optional[SASettings] = None,
) -> SQLAlchemyClient:
    sa = sa or SASettings()
    echo, pool_size, max_overflow, pool_timeout, pool_recycle, pool_pre_ping = _resolve_sa(
        params, sa
    )
    connect_args: Dict[str, Any] = dict(params.options.get("connect_args", {}) or {})

    if kind == DBKind.POSTGRES:
        url = postgres_url(params)
    elif kind == DBKind.MYSQL:
        url = mysql_url(params)
    elif kind == DBKind.SQLITE:
        url = sqlite_url(params)
        connect_args.setdefault("check_same_thread", False)
        overrides = dict(params.options.get("sa", {}) or {})
        pool_size = int(overrides.get("pool_size", 1))
        max_overflow = int(overrides.get("max_overflow", 0))
    elif kind == DBKind.CLICKHOUSE:
        url = clickhouse_url(params)
    else:
        raise ValueError("unsupported kind: %s" % kind)

    return make_sqlalchemy_client(
        url,
        echo=echo,
        pool_size=pool_size,
        max_overflow=max_overflow,
        pool_timeout=pool_timeout,
        pool_recycle=pool_recycle,
        pool_pre_ping=pool_pre_ping,
        connect_args=connect_args,
    )


def create_async_sqlalchemy_client(
    kind: DBKind,
    params: ConnParams,
    sa: Optional[SASettings] = None,
) -> AsyncSQLAlchemyClient:
    """
    Async is supported for Postgres/MySQL/SQLite. ClickHouse is not supported.
    """
    if kind == DBKind.CLICKHOUSE:
        raise ValueError("clickhouse does not support async client")

    sa = sa or SASettings()

    params_async = ConnParams(
        ip=params.ip,
        port=params.port,
        database=params.database,
        user=params.user,
        password=params.password,
        options={**dict(params.options), "is_async": True},
    )

    echo, pool_size, max_overflow, pool_timeout, pool_recycle, pool_pre_ping = _resolve_sa(
        params_async, sa
    )
    connect_args: Dict[str, Any] = dict(params_async.options.get("connect_args", {}) or {})

    if kind == DBKind.POSTGRES:
        url = postgres_url(params_async)
    elif kind == DBKind.MYSQL:
        url = mysql_url(params_async)
    elif kind == DBKind.SQLITE:
        url = sqlite_url(params_async)
        connect_args.setdefault("check_same_thread", False)
        overrides = dict(params_async.options.get("sa", {}) or {})
        pool_size = int(overrides.get("pool_size", 1))
        max_overflow = int(overrides.get("max_overflow", 0))
    else:
        raise ValueError("unsupported kind: %s" % kind)

    return make_async_sqlalchemy_client(
        url,
        echo=echo,
        pool_size=pool_size,
        max_overflow=max_overflow,
        pool_timeout=pool_timeout,
        pool_recycle=pool_recycle,
        pool_pre_ping=pool_pre_ping,
        connect_args=connect_args,
    )


def create_sqlalchemy_client_from_dsn(
    dsn: str,
    *,
    sa: Optional[SASettings] = None,
    connect_args: Optional[Dict[str, Any]] = None,
) -> SQLAlchemyClient:
    sa = sa or SASettings()
    echo, pool_size, max_overflow, pool_timeout, pool_recycle, pool_pre_ping = _resolve_sa(
        ConnParams(ip=""), sa
    )
    connect_args = dict(connect_args or {})
    dsn_lower = dsn.lower()
    if dsn_lower.startswith("sqlite"):
        connect_args.setdefault("check_same_thread", False)
        pool_size = 1
        max_overflow = 0
    return make_sqlalchemy_client(
        dsn,
        echo=echo,
        pool_size=pool_size,
        max_overflow=max_overflow,
        pool_timeout=pool_timeout,
        pool_recycle=pool_recycle,
        pool_pre_ping=pool_pre_ping,
        connect_args=connect_args,
    )


def create_async_sqlalchemy_client_from_dsn(
    dsn: str,
    *,
    sa: Optional[SASettings] = None,
    connect_args: Optional[Dict[str, Any]] = None,
) -> AsyncSQLAlchemyClient:
    sa = sa or SASettings()
    echo, pool_size, max_overflow, pool_timeout, pool_recycle, pool_pre_ping = _resolve_sa(
        ConnParams(ip=""), sa
    )
    connect_args = dict(connect_args or {})
    dsn_lower = dsn.lower()
    if dsn_lower.startswith("sqlite"):
        connect_args.setdefault("check_same_thread", False)
        pool_size = 1
        max_overflow = 0
    return make_async_sqlalchemy_client(
        dsn,
        echo=echo,
        pool_size=pool_size,
        max_overflow=max_overflow,
        pool_timeout=pool_timeout,
        pool_recycle=pool_recycle,
        pool_pre_ping=pool_pre_ping,
        connect_args=connect_args,
    )
