from __future__ import annotations

from contextlib import asynccontextmanager, contextmanager
from functools import partial
from typing import AsyncIterator, Iterator, Optional, Union

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import Session

from .client_async import AsyncSQLAlchemyClient
from .client_sync import SQLAlchemyClient
from .config import DBKind
from .registry import DBRegistry, DEFAULT_REGISTRY
from .session_context import (
    reset_current_async_session,
    reset_current_session,
    set_current_async_session,
    set_current_session,
)
from .clickhouse_adapter import ClickHouseExecutor, clickhouse_session_scope, clickhouse_tx_scope


SessionLike = Union[Session, ClickHouseExecutor]


@contextmanager
def session_scope(
    kind: Optional[DBKind] = None,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    client: Optional[SQLAlchemyClient] = None,
) -> Iterator[SessionLike]:
    if client is None:
        if kind is None:
            raise ValueError("kind is required when client is not provided")
        if registry.is_clickhouse_connect(kind, db_name):
            executor = registry.get_clickhouse_executor(kind, db_name)
            with clickhouse_session_scope(executor) as clickhouse_session:
                yield clickhouse_session
            return
        client = registry.get_client(kind, db_name)
    session: Session = client.SessionMaker()
    token = set_current_session(session)
    try:
        yield session
    finally:
        reset_current_session(token)
        session.close()


@contextmanager
def tx_scope(
    kind: Optional[DBKind] = None,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    client: Optional[SQLAlchemyClient] = None,
) -> Iterator[SessionLike]:
    if client is None:
        if kind is None:
            raise ValueError("kind is required when client is not provided")
        if registry.is_clickhouse_connect(kind, db_name):
            executor = registry.get_clickhouse_executor(kind, db_name)
            with clickhouse_tx_scope(executor) as clickhouse_session:
                yield clickhouse_session
            return
        client = registry.get_client(kind, db_name)
    session: Session = client.SessionMaker()
    token = set_current_session(session)
    try:
        with session.begin():
            yield session
    finally:
        reset_current_session(token)
        session.close()


@asynccontextmanager
async def async_session_scope(
    kind: Optional[DBKind] = None,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    client: Optional[AsyncSQLAlchemyClient] = None,
) -> AsyncIterator[AsyncSession]:
    if client is None:
        if kind is None:
            raise ValueError("kind is required when client is not provided")
        client = registry.get_async_client(kind, db_name)
    session: AsyncSession = client.SessionMaker()
    token = set_current_async_session(session)
    try:
        yield session
    finally:
        reset_current_async_session(token)
        await session.close()


@asynccontextmanager
async def async_tx_scope(
    kind: Optional[DBKind] = None,
    db_name: str = "default",
    *,
    registry: DBRegistry = DEFAULT_REGISTRY,
    client: Optional[AsyncSQLAlchemyClient] = None,
) -> AsyncIterator[AsyncSession]:
    if client is None:
        if kind is None:
            raise ValueError("kind is required when client is not provided")
        client = registry.get_async_client(kind, db_name)
    session: AsyncSession = client.SessionMaker()
    token = set_current_async_session(session)
    try:
        async with session.begin():
            yield session
    finally:
        reset_current_async_session(token)
        await session.close()


pg_session_scope = partial(session_scope, DBKind.POSTGRES)
mysql_session_scope = partial(session_scope, DBKind.MYSQL)
sqlite_session_scope = partial(session_scope, DBKind.SQLITE)
ck_session_scope = partial(session_scope, DBKind.CLICKHOUSE)

pg_tx_scope = partial(tx_scope, DBKind.POSTGRES)
mysql_tx_scope = partial(tx_scope, DBKind.MYSQL)
sqlite_tx_scope = partial(tx_scope, DBKind.SQLITE)
ck_tx_scope = partial(tx_scope, DBKind.CLICKHOUSE)
