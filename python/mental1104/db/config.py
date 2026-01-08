from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Mapping, Optional
import os


class DBKind(str, Enum):
    POSTGRES = "postgres"
    MYSQL = "mysql"
    SQLITE = "sqlite"
    CLICKHOUSE = "clickhouse"


class ClickHouseProfile(str, Enum):
    DEFAULT = "default"
    DISTRIBUTED = "distributed"


@dataclass(frozen=True)
class ConnParams:
    ip: str
    port: Optional[int] = None
    database: Optional[str] = None
    user: Optional[str] = None
    password: Optional[str] = None
    options: Mapping[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class SASettings:
    echo: bool = False
    pool_size: int = 5
    max_overflow: int = 10
    pool_timeout: int = 30
    pool_recycle: int = 1800
    pool_pre_ping: bool = True


def conn_params_from_env(kind: DBKind, *, prefix: str = "") -> ConnParams:
    env = os.environ
    if kind == DBKind.POSTGRES:
        return ConnParams(
            ip=env[f"{prefix}PGHOST"],
            port=int(env.get(f"{prefix}PGPORT", "5432")),
            database=env.get(f"{prefix}PGDATABASE"),
            user=env.get(f"{prefix}PGUSER"),
            password=env.get(f"{prefix}PGPASSWORD"),
        )
    if kind == DBKind.MYSQL:
        return ConnParams(
            ip=env[f"{prefix}MYSQL_HOST"],
            port=int(env.get(f"{prefix}MYSQL_PORT", "3306")),
            database=env.get(f"{prefix}MYSQL_DATABASE"),
            user=env.get(f"{prefix}MYSQL_USER"),
            password=env.get(f"{prefix}MYSQL_PASSWORD"),
        )
    if kind == DBKind.SQLITE:
        return ConnParams(
            ip=env.get(f"{prefix}SQLITE_PATH", ":memory:"),
        )
    if kind == DBKind.CLICKHOUSE:
        return ConnParams(
            ip=env[f"{prefix}CLICKHOUSE_HOST"],
            port=int(env.get(f"{prefix}CLICKHOUSE_HTTP_PORT", "8123")),
            database=env.get(f"{prefix}CLICKHOUSE_DATABASE", "default"),
            user=env.get(f"{prefix}CLICKHOUSE_USER"),
            password=env.get(f"{prefix}CLICKHOUSE_PASSWORD"),
        )
    raise ValueError(f"unsupported kind: {kind}")
