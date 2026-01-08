from __future__ import annotations

from contextlib import asynccontextmanager, contextmanager
from typing import AsyncIterator, Iterator

from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import Session

from .client_async import AsyncSQLAlchemyClient
from .client_sync import SQLAlchemyClient
from .session_context import (
    reset_current_async_session,
    reset_current_session,
    set_current_async_session,
    set_current_session,
)


class UnitOfWork:
    def __init__(self, client: SQLAlchemyClient):
        self._client = client

    @contextmanager
    def __call__(self) -> Iterator[Session]:
        with self._client.session_scope() as session:
            token = set_current_session(session)
            try:
                yield session
            finally:
                reset_current_session(token)


class AsyncUnitOfWork:
    def __init__(self, client: AsyncSQLAlchemyClient):
        self._client = client

    @asynccontextmanager
    async def __call__(self) -> AsyncIterator[AsyncSession]:
        async with self._client.session_scope() as session:
            token = set_current_async_session(session)
            try:
                yield session
            finally:
                reset_current_async_session(token)
