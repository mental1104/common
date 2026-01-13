import asyncio

import pytest
import pytest_asyncio
from sqlalchemy import Integer, String, select
from sqlalchemy.orm import DeclarativeBase, mapped_column

from mental1104.db import (
    ConnParams,
    DBKind,
    DBRegistry,
    ctx_async_session,
    register_db_and_create_async,
    require_ctx_async_session,
)
from mental1104.db import AutoSessionDAO
from mental1104.db.scopes import async_session_scope, async_tx_scope

pytest.importorskip("aiosqlite")


class _Base(DeclarativeBase):
    pass


class _User(_Base):
    __tablename__ = "test_scope_user_async"
    id = mapped_column(Integer, primary_key=True, autoincrement=True)
    name = mapped_column(String(64), nullable=False, index=True)


class _AsyncUserDAO(AutoSessionDAO):
    async def create(self, name: str, *, db) -> _User:
        user = _User(name=name)
        db.add(user)
        await db.flush()
        return user

    async def list(self, *, db):
        result = await db.execute(select(_User).order_by(_User.id))
        return list(result.scalars().all())


@pytest_asyncio.fixture()
async def registry(tmp_path):
    db_path = tmp_path / "scope_async.db"
    reg = DBRegistry()
    await register_db_and_create_async(
        DBKind.SQLITE,
        params=ConnParams(ip=str(db_path)),
        db_name="scope_sqlite_async",
        registry=reg,
        base=_Base,
    )
    engine = reg.get_async_engine(DBKind.SQLITE, "scope_sqlite_async")
    yield reg
    async with engine.begin() as conn:
        await conn.run_sync(_Base.metadata.drop_all)
    await reg.get_async_client(DBKind.SQLITE, "scope_sqlite_async").close()


@pytest.mark.asyncio
async def test_async_ctx_injection_read(registry):
    dao = _AsyncUserDAO()
    async with async_session_scope(DBKind.SQLITE, "scope_sqlite_async", registry=registry):
        session = require_ctx_async_session()
        rows = await dao.list()
        assert session is require_ctx_async_session()
    assert rows == []
    assert ctx_async_session() is None


@pytest.mark.asyncio
async def test_async_tx_scope_commit_and_rollback(registry):
    dao = _AsyncUserDAO()

    async with async_tx_scope(DBKind.SQLITE, "scope_sqlite_async", registry=registry):
        require_ctx_async_session()
        await dao.create("alice")

    async with async_session_scope(DBKind.SQLITE, "scope_sqlite_async", registry=registry):
        rows = await dao.list()
    assert len(rows) == 1

    with pytest.raises(RuntimeError):
        async with async_tx_scope(DBKind.SQLITE, "scope_sqlite_async", registry=registry):
            require_ctx_async_session()
            await dao.create("bob")
            raise RuntimeError("boom")

    async with async_session_scope(DBKind.SQLITE, "scope_sqlite_async", registry=registry):
        rows = await dao.list()
    assert len(rows) == 1


@pytest.mark.asyncio
async def test_async_isolation(registry):
    dao = _AsyncUserDAO()
    session_ids = []

    async def worker(name: str):
        async with async_tx_scope(DBKind.SQLITE, "scope_sqlite_async", registry=registry):
            session = require_ctx_async_session()
            await dao.create(name)
            session_ids.append(id(session))

    await asyncio.gather(worker("t1"), worker("t2"))
    assert len(session_ids) == 2
    assert session_ids[0] != session_ids[1]
    assert ctx_async_session() is None


@pytest.mark.asyncio
async def test_async_ctx_missing_raises():
    with pytest.raises(RuntimeError):
        require_ctx_async_session()
