from __future__ import annotations

from dataclasses import dataclass, field
from threading import Lock
from typing import Any, Dict, Mapping, Optional, Tuple

from sqlalchemy.engine import Engine
from sqlalchemy.ext.asyncio import AsyncEngine

from .client_async import AsyncSQLAlchemyClient
from .client_sync import SQLAlchemyClient
from .config import ClickHouseProfile, ConnParams, DBKind, SASettings
from .clickhouse_profiles import resolve_clickhouse_profile
from .clickhouse_adapter import ClickHouseExecutor, make_clickhouse_executor
from .factory import (
    create_async_sqlalchemy_client,
    create_async_sqlalchemy_client_from_dsn,
    create_sqlalchemy_client,
    create_sqlalchemy_client_from_dsn,
)


@dataclass(frozen=True)
class DBConfig:
    kind: DBKind
    db_name: str
    dsn: Optional[str] = None
    params: Optional[ConnParams] = None
    sa: SASettings = field(default_factory=SASettings)
    options: Mapping[str, Any] = field(default_factory=dict)
    profile: Optional[ClickHouseProfile] = None
    cluster: Optional[str] = None


class DBRegistry:
    def __init__(self) -> None:
        self._configs: Dict[Tuple[DBKind, str], DBConfig] = {}
        self._sync_clients: Dict[Tuple[DBKind, str], SQLAlchemyClient] = {}
        self._async_clients: Dict[Tuple[DBKind, str], AsyncSQLAlchemyClient] = {}
        self._clickhouse_exec: Dict[Tuple[DBKind, str], ClickHouseExecutor] = {}
        self._lock = Lock()

    def _key(self, kind: DBKind, db_name: str) -> Tuple[DBKind, str]:
        return (kind, db_name)

    def register_db(
        self,
        kind: DBKind,
        dsn: Optional[str] = None,
        params: Optional[ConnParams] = None,
        *,
        db_name: str = "default",
        options: Optional[Mapping[str, Any]] = None,
        profile: Optional[ClickHouseProfile | str] = None,
        sa: Optional[SASettings] = None,
        allow_overwrite: bool = False,
    ) -> DBConfig:
        if not dsn and params is None:
            raise ValueError("dsn or params must be provided")
        key = self._key(kind, db_name)
        if not allow_overwrite and key in self._configs:
            raise ValueError(f"db '{db_name}' with kind '{kind.value}' already registered")
        resolved_profile = None
        cluster = None
        if kind == DBKind.CLICKHOUSE:
            resolved_profile, cluster, merged_options = resolve_clickhouse_profile(options, profile)
        else:
            merged_options = dict(options or {})
        config = DBConfig(
            kind=kind,
            db_name=db_name,
            dsn=dsn,
            params=params,
            sa=sa or SASettings(),
            options=merged_options,
            profile=resolved_profile,
            cluster=cluster,
        )
        self._configs[key] = config
        return config

    def get_config(self, kind: DBKind, db_name: str = "default") -> DBConfig:
        key = self._key(kind, db_name)
        try:
            return self._configs[key]
        except KeyError as exc:
            raise KeyError(f"db '{db_name}' with kind '{kind.value}' is not registered") from exc

    def get_engine(self, kind: DBKind, db_name: str = "default") -> Engine:
        return self.get_client(kind, db_name).engine

    def get_async_engine(self, kind: DBKind, db_name: str = "default") -> AsyncEngine:
        return self.get_async_client(kind, db_name).engine

    def get_session_factory(self, kind: DBKind, db_name: str = "default"):
        return self.get_client(kind, db_name).SessionMaker

    def get_async_session_factory(self, kind: DBKind, db_name: str = "default"):
        return self.get_async_client(kind, db_name).SessionMaker

    def get_client(self, kind: DBKind, db_name: str = "default") -> SQLAlchemyClient:
        key = self._key(kind, db_name)
        if key in self._sync_clients:
            return self._sync_clients[key]
        with self._lock:
            if key in self._sync_clients:
                return self._sync_clients[key]
            cfg = self.get_config(kind, db_name)
            if self._is_clickhouse_connect(cfg):
                raise ValueError("clickhouse-connect driver does not provide SQLAlchemy client")
            client = self._build_sync_client(cfg)
            self._sync_clients[key] = client
            return client

    def get_async_client(self, kind: DBKind, db_name: str = "default") -> AsyncSQLAlchemyClient:
        key = self._key(kind, db_name)
        if key in self._async_clients:
            return self._async_clients[key]
        with self._lock:
            if key in self._async_clients:
                return self._async_clients[key]
            cfg = self.get_config(kind, db_name)
            if cfg.kind == DBKind.CLICKHOUSE:
                raise ValueError("clickhouse does not support async SQLAlchemy client")
            client = self._build_async_client(cfg)
            self._async_clients[key] = client
            return client

    def get_clickhouse_executor(self, kind: DBKind, db_name: str = "default") -> ClickHouseExecutor:
        key = self._key(kind, db_name)
        if key in self._clickhouse_exec:
            return self._clickhouse_exec[key]
        with self._lock:
            if key in self._clickhouse_exec:
                return self._clickhouse_exec[key]
            cfg = self.get_config(kind, db_name)
            if cfg.kind != DBKind.CLICKHOUSE:
                raise ValueError("clickhouse executor is only available for clickhouse kind")
            if not self._is_clickhouse_connect(cfg):
                raise ValueError("clickhouse executor is only for clickhouse-connect driver")
            executor = make_clickhouse_executor(cfg.dsn, cfg.params, cfg.options)
            self._clickhouse_exec[key] = executor
            return executor

    def is_clickhouse_connect(self, kind: DBKind, db_name: str = "default") -> bool:
        cfg = self.get_config(kind, db_name)
        return self._is_clickhouse_connect(cfg)

    def _is_clickhouse_connect(self, cfg: DBConfig) -> bool:
        if cfg.kind != DBKind.CLICKHOUSE:
            return False
        driver = str(cfg.options.get("driver", "sqlalchemy")).lower()
        return driver in ("connect", "clickhouse-connect", "native")

    def _merge_params(self, params: ConnParams, options: Mapping[str, Any]) -> ConnParams:
        if not options:
            return params
        merged = dict(params.options or {})
        merged.update(options)
        return ConnParams(
            ip=params.ip,
            port=params.port,
            database=params.database,
            user=params.user,
            password=params.password,
            options=merged,
        )

    def _build_sync_client(self, cfg: DBConfig) -> SQLAlchemyClient:
        if cfg.dsn:
            connect_args = dict(cfg.options.get("connect_args", {}) or {})
            return create_sqlalchemy_client_from_dsn(cfg.dsn, sa=cfg.sa, connect_args=connect_args)
        params = self._merge_params(cfg.params, cfg.options) if cfg.params else None
        if params is None:
            raise ValueError("params is required when dsn is not provided")
        return create_sqlalchemy_client(cfg.kind, params, sa=cfg.sa)

    def _build_async_client(self, cfg: DBConfig) -> AsyncSQLAlchemyClient:
        if cfg.dsn:
            connect_args = dict(cfg.options.get("connect_args", {}) or {})
            return create_async_sqlalchemy_client_from_dsn(
                cfg.dsn, sa=cfg.sa, connect_args=connect_args
            )
        params = self._merge_params(cfg.params, cfg.options) if cfg.params else None
        if params is None:
            raise ValueError("params is required when dsn is not provided")
        return create_async_sqlalchemy_client(cfg.kind, params, sa=cfg.sa)


DEFAULT_REGISTRY = DBRegistry()


def register_db(
    kind: DBKind,
    dsn: Optional[str] = None,
    params: Optional[ConnParams] = None,
    *,
    db_name: str = "default",
    options: Optional[Mapping[str, Any]] = None,
    profile: Optional[ClickHouseProfile | str] = None,
    sa: Optional[SASettings] = None,
    allow_overwrite: bool = False,
) -> DBConfig:
    return DEFAULT_REGISTRY.register_db(
        kind=kind,
        dsn=dsn,
        params=params,
        db_name=db_name,
        options=options,
        profile=profile,
        sa=sa,
        allow_overwrite=allow_overwrite,
    )


def get_engine(kind: DBKind, db_name: str = "default") -> Engine:
    return DEFAULT_REGISTRY.get_engine(kind, db_name)


def get_async_engine(kind: DBKind, db_name: str = "default") -> AsyncEngine:
    return DEFAULT_REGISTRY.get_async_engine(kind, db_name)


def get_session_factory(kind: DBKind, db_name: str = "default"):
    return DEFAULT_REGISTRY.get_session_factory(kind, db_name)


def get_async_session_factory(kind: DBKind, db_name: str = "default"):
    return DEFAULT_REGISTRY.get_async_session_factory(kind, db_name)


def get_clickhouse_executor(kind: DBKind, db_name: str = "default") -> ClickHouseExecutor:
    return DEFAULT_REGISTRY.get_clickhouse_executor(kind, db_name)
