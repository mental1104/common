from __future__ import annotations

from contextlib import asynccontextmanager
from dataclasses import dataclass
from typing import Any, AsyncIterator, Dict, Optional

from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncEngine, AsyncSession, async_sessionmaker, create_async_engine


@dataclass(frozen=True)
class AsyncSQLAlchemyClient:
    engine: AsyncEngine
    SessionMaker: async_sessionmaker

    @asynccontextmanager
    async def session_scope(self) -> AsyncIterator[AsyncSession]:
        session: AsyncSession = self.SessionMaker()
        try:
            yield session
            await session.commit()
        except Exception:
            await session.rollback()
            raise
        finally:
            await session.close()

    async def ping(self) -> None:
        async with self.engine.connect() as conn:
            await conn.execute(text("SELECT 1"))

    async def close(self) -> None:
        await self.engine.dispose()


def make_async_sqlalchemy_client(
    url: str,
    *,
    echo: bool,
    pool_size: int,
    max_overflow: int,
    pool_timeout: int,
    pool_recycle: int,
    pool_pre_ping: bool,
    connect_args: Optional[Dict[str, Any]] = None,
) -> AsyncSQLAlchemyClient:
    engine = create_async_engine(
        url,
        echo=echo,
        pool_size=pool_size,
        max_overflow=max_overflow,
        pool_timeout=pool_timeout,
        pool_recycle=pool_recycle,
        pool_pre_ping=pool_pre_ping,
        future=True,
        connect_args=connect_args or {},
    )
    SessionMaker = async_sessionmaker(bind=engine, autoflush=False, expire_on_commit=False)
    return AsyncSQLAlchemyClient(engine=engine, SessionMaker=SessionMaker)
