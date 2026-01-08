from __future__ import annotations

from typing import Any, Callable, Iterable, List, Mapping, Optional, Tuple

from sqlalchemy.engine import Engine
from sqlalchemy.ext.asyncio import AsyncEngine

from .config import ClickHouseProfile, ConnParams, DBKind, SASettings
from .orm_base import Base
from .registry import DBConfig, DEFAULT_REGISTRY, DBRegistry

MigrationHandler = Callable[[DBKind, str, DBRegistry], None]

_migration_handler: Optional[MigrationHandler] = None


def set_migration_handler(handler: MigrationHandler) -> None:
    global _migration_handler
    _migration_handler = handler


def run_migrations(
    kind: DBKind, db_name: str = "default", registry: DBRegistry = DEFAULT_REGISTRY
) -> None:
    if _migration_handler is None:
        raise RuntimeError("migration handler is not configured")
    _migration_handler(kind, db_name, registry)


def register_db_and_create(
    kind: DBKind,
    *,
    dsn: Optional[str] = None,
    params: Optional[ConnParams] = None,
    db_name: str = "default",
    options: Optional[Mapping[str, Any]] = None,
    profile: Optional[ClickHouseProfile | str] = None,
    sa: Optional[SASettings] = None,
    registry: DBRegistry = DEFAULT_REGISTRY,
    base: Base = Base,
    tables: Optional[Iterable] = None,
    create: bool = True,
    allow_overwrite: bool = False,
) -> DBConfig:
    cfg = registry.register_db(
        kind=kind,
        dsn=dsn,
        params=params,
        db_name=db_name,
        options=options,
        profile=profile,
        sa=sa,
        allow_overwrite=allow_overwrite,
    )
    if create:
        create_all(kind, db_name, registry=registry, base=base, tables=tables)
    return cfg


async def register_db_and_create_async(
    kind: DBKind,
    *,
    dsn: Optional[str] = None,
    params: Optional[ConnParams] = None,
    db_name: str = "default",
    options: Optional[Mapping[str, Any]] = None,
    profile: Optional[ClickHouseProfile | str] = None,
    sa: Optional[SASettings] = None,
    registry: DBRegistry = DEFAULT_REGISTRY,
    base: Base = Base,
    tables: Optional[Iterable] = None,
    create: bool = True,
    allow_overwrite: bool = False,
) -> DBConfig:
    cfg = registry.register_db(
        kind=kind,
        dsn=dsn,
        params=params,
        db_name=db_name,
        options=options,
        profile=profile,
        sa=sa,
        allow_overwrite=allow_overwrite,
    )
    if create:
        await create_all_async(kind, db_name, registry=registry, base=base, tables=tables)
    return cfg


def create_all(
    kind: DBKind,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    base: Base = Base,
    tables: Optional[Iterable] = None,
) -> None:
    cfg = registry.get_config(kind, db_name)
    if cfg.kind == DBKind.CLICKHOUSE and registry.is_clickhouse_connect(kind, db_name):
        raise RuntimeError("clickhouse-connect does not support SQLAlchemy create_all")
    target_tables = list(tables) if tables else list(base.metadata.tables.values())
    cluster_applied: List[Tuple[Any, Optional[str]]] = []
    if cfg.kind == DBKind.CLICKHOUSE:
        cluster = cfg.cluster
        if cfg.profile == ClickHouseProfile.DISTRIBUTED and not cluster:
            raise RuntimeError(
                "ClickHouse distributed profile requires cluster name. "
                "Set options={'cluster': '...'} or options={'on_cluster': '...'}."
            )
        if cluster:
            for table in target_tables:
                opts = table.dialect_options["clickhouse"]
                cluster_applied.append((table, opts.get("cluster")))
                opts["cluster"] = cluster
    engine: Engine = registry.get_engine(kind, db_name)
    try:
        base.metadata.create_all(bind=engine, tables=target_tables if tables else None)
    finally:
        for table, prev in cluster_applied:
            table.dialect_options["clickhouse"]["cluster"] = prev


async def create_all_async(
    kind: DBKind,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    base: Base = Base,
    tables: Optional[Iterable] = None,
) -> None:
    engine: AsyncEngine = registry.get_async_engine(kind, db_name)
    async with engine.begin() as conn:
        await conn.run_sync(base.metadata.create_all, tables=list(tables) if tables else None)


def drop_all(
    kind: DBKind,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    base: Base = Base,
    tables: Optional[Iterable] = None,
) -> None:
    cfg = registry.get_config(kind, db_name)
    if cfg.kind == DBKind.CLICKHOUSE and registry.is_clickhouse_connect(kind, db_name):
        raise RuntimeError("clickhouse-connect does not support SQLAlchemy drop_all")
    target_tables = list(tables) if tables else list(base.metadata.tables.values())
    cluster_applied: List[Tuple[Any, Optional[str]]] = []
    if cfg.kind == DBKind.CLICKHOUSE:
        cluster = cfg.cluster
        if cfg.profile == ClickHouseProfile.DISTRIBUTED and not cluster:
            raise RuntimeError(
                "ClickHouse distributed profile requires cluster name. "
                "Set options={'cluster': '...'} or options={'on_cluster': '...'}."
            )
        if cluster:
            for table in target_tables:
                opts = table.dialect_options["clickhouse"]
                cluster_applied.append((table, opts.get("cluster")))
                opts["cluster"] = cluster
    engine: Engine = registry.get_engine(kind, db_name)
    try:
        base.metadata.drop_all(bind=engine, tables=target_tables if tables else None)
    finally:
        for table, prev in cluster_applied:
            table.dialect_options["clickhouse"]["cluster"] = prev


async def drop_all_async(
    kind: DBKind,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    base: Base = Base,
    tables: Optional[Iterable] = None,
) -> None:
    engine: AsyncEngine = registry.get_async_engine(kind, db_name)
    async with engine.begin() as conn:
        await conn.run_sync(base.metadata.drop_all, tables=list(tables) if tables else None)
