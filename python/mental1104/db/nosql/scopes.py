from __future__ import annotations

from contextlib import asynccontextmanager, contextmanager
from typing import AsyncIterator, Iterator, Optional

from pymongo.client_session import ClientSession

from .context import (
    AsyncMongoSession,
    MongoSession,
    reset_current_async_mongo_session,
    reset_current_mongo_session,
    set_current_async_mongo_session,
    set_current_mongo_session,
)
from .registry import DEFAULT_MONGO_REGISTRY, MongoRegistry


@contextmanager
def mongo_session_scope(
    name: str = "default",
    *,
    registry: MongoRegistry = DEFAULT_MONGO_REGISTRY,
    client=None,
    database: Optional[str] = None,
    use_session: bool = False,
) -> Iterator[MongoSession]:
    if client is None:
        client, db_name = registry.get_client(name)
    else:
        db_name = database or "test"
    db_name = database or db_name
    session: Optional[ClientSession] = None
    if use_session:
        session = client.start_session()
    mongo_session = MongoSession(client=client, db=client[db_name], session=session)
    token = set_current_mongo_session(mongo_session)
    try:
        yield mongo_session
    finally:
        reset_current_mongo_session(token)
        if session is not None:
            session.end_session()


@contextmanager
def mongo_tx_scope(
    name: str = "default",
    *,
    registry: MongoRegistry = DEFAULT_MONGO_REGISTRY,
    client=None,
    database: Optional[str] = None,
) -> Iterator[MongoSession]:
    with mongo_session_scope(
        name=name,
        registry=registry,
        client=client,
        database=database,
        use_session=True,
    ) as mongo_session:
        if mongo_session.session is None:
            raise RuntimeError("Mongo session is not available")
        try:
            mongo_session.session.start_transaction()
            yield mongo_session
            mongo_session.session.commit_transaction()
        except Exception:
            mongo_session.session.abort_transaction()
            raise


@asynccontextmanager
async def async_mongo_session_scope(
    name: str = "default",
    *,
    registry: MongoRegistry = DEFAULT_MONGO_REGISTRY,
    client=None,
    database: Optional[str] = None,
    use_session: bool = False,
) -> AsyncIterator[AsyncMongoSession]:
    if client is None:
        client, db_name = registry.get_async_client(name)
    else:
        db_name = database or "test"
    db_name = database or db_name

    if use_session:
        session = await client.start_session()
        async with session:
            mongo_session = AsyncMongoSession(client=client, db=client[db_name], session=session)
            token = set_current_async_mongo_session(mongo_session)
            try:
                yield mongo_session
            finally:
                reset_current_async_mongo_session(token)
        return

    mongo_session = AsyncMongoSession(client=client, db=client[db_name], session=None)
    token = set_current_async_mongo_session(mongo_session)
    try:
        yield mongo_session
    finally:
        reset_current_async_mongo_session(token)


@asynccontextmanager
async def async_mongo_tx_scope(
    name: str = "default",
    *,
    registry: MongoRegistry = DEFAULT_MONGO_REGISTRY,
    client=None,
    database: Optional[str] = None,
) -> AsyncIterator[AsyncMongoSession]:
    async with async_mongo_session_scope(
        name=name,
        registry=registry,
        client=client,
        database=database,
        use_session=True,
    ) as mongo_session:
        if mongo_session.session is None:
            raise RuntimeError("Mongo session is not available")
        async with mongo_session.session.start_transaction():
            yield mongo_session
