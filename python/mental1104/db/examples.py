from __future__ import annotations

import threading
from typing import List

from sqlalchemy import String, select
from sqlalchemy.orm import Mapped, mapped_column

from . import (
    AutoSessionDAO,
    Base,
    DBKind,
    async_session_scope,
    async_tx_scope,
    ctx_session,
    register_db,
    session_scope,
    tx_scope,
)
from .schema import register_db_and_create


class User(Base):
    __tablename__ = "example_user"
    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String(64), nullable=False, index=True)


class UserDAO(AutoSessionDAO):
    def create(self, name: str, *, db):
        user = User(name=name)
        db.add(user)
        db.flush()
        return user

    def list(self, *, db) -> List[User]:
        result = db.execute(select(User).order_by(User.id))
        return list(result.scalars().all())


class AsyncUserDAO(AutoSessionDAO):
    async def create(self, name: str, *, db):
        user = User(name=name)
        db.add(user)
        await db.flush()
        return user

    async def list(self, *, db) -> List[User]:
        result = await db.execute(select(User).order_by(User.id))
        return list(result.scalars().all())


def bootstrap(dsn: str) -> None:
    register_db_and_create(DBKind.POSTGRES, dsn=dsn, db_name="main_pg", base=Base)


def example_read() -> List[User]:
    dao = UserDAO()
    with session_scope(DBKind.POSTGRES, "main_pg"):
        return dao.list()


def example_write(name: str) -> None:
    dao = UserDAO()
    with tx_scope(DBKind.POSTGRES, "main_pg"):
        dao.create(name)


def example_read_then_write(name: str) -> None:
    dao = UserDAO()
    with tx_scope(DBKind.POSTGRES, "main_pg"):
        db = ctx_session()
        result = db.execute(select(User).where(User.name == name).with_for_update())
        row = result.scalar_one_or_none()
        if row is None:
            dao.create(name)


def example_threads(names: List[str]) -> None:
    dao = UserDAO()

    def worker(value: str):
        with tx_scope(DBKind.POSTGRES, "main_pg"):
            dao.create(value)

    threads = [threading.Thread(target=worker, args=(n,)) for n in names]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def example_chunk_read() -> List[int]:
    with session_scope(DBKind.POSTGRES, "main_pg") as session:
        result = session.execute(
            select(User.id).order_by(User.id).execution_options(stream_results=True)
        )
        rows = result.scalars().yield_per(200)
        return list(rows)


async def example_async_read() -> List[User]:
    dao = AsyncUserDAO()
    async with async_session_scope(DBKind.POSTGRES, "main_pg"):
        return await dao.list()


async def example_async_write(name: str) -> None:
    dao = AsyncUserDAO()
    async with async_tx_scope(DBKind.POSTGRES, "main_pg"):
        await dao.create(name)
