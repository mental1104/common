import threading

import pytest
from sqlalchemy import Integer, String, select
from sqlalchemy.orm import DeclarativeBase, mapped_column

from mental1104.db import (
    ConnParams,
    DBKind,
    DBRegistry,
    ctx_session,
    register_db_and_create,
    require_ctx_session,
)
from mental1104.db import AutoSessionDAO
from mental1104.db.scopes import session_scope, tx_scope


class _Base(DeclarativeBase):
    pass


class _User(_Base):
    __tablename__ = "test_scope_user"
    id = mapped_column(Integer, primary_key=True, autoincrement=True)
    name = mapped_column(String(64), nullable=False, index=True)


class _UserDAO(AutoSessionDAO):
    def create(self, name: str, *, db) -> _User:
        user = _User(name=name)
        db.add(user)
        db.flush()
        return user

    def list(self, *, db):
        result = db.execute(select(_User).order_by(_User.id))
        return list(result.scalars().all())


@pytest.fixture()
def registry(tmp_path):
    db_path = tmp_path / "scope.db"
    reg = DBRegistry()
    register_db_and_create(
        DBKind.SQLITE,
        params=ConnParams(ip=str(db_path)),
        db_name="scope_sqlite",
        registry=reg,
        base=_Base,
    )
    yield reg
    _Base.metadata.drop_all(bind=reg.get_engine(DBKind.SQLITE, "scope_sqlite"))
    reg.get_client(DBKind.SQLITE, "scope_sqlite").close()


def test_ctx_injection_read(registry):
    dao = _UserDAO()
    with session_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
        session = require_ctx_session()
        rows = dao.list()
        assert session is require_ctx_session()
    assert rows == []
    assert ctx_session() is None


def test_tx_scope_commit_and_rollback(registry):
    dao = _UserDAO()

    with tx_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
        require_ctx_session()
        dao.create("alice")

    with session_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
        rows = dao.list()
    assert len(rows) == 1

    with pytest.raises(RuntimeError):
        with tx_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
            require_ctx_session()
            dao.create("bob")
            raise RuntimeError("boom")

    with session_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
        rows = dao.list()
    assert len(rows) == 1


def test_thread_isolation(registry):
    dao = _UserDAO()
    session_ids = []
    lock = threading.Lock()

    def worker(name: str):
        with tx_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
            session = require_ctx_session()
            dao.create(name)
            with lock:
                session_ids.append(id(session))

    t1 = threading.Thread(target=worker, args=("t1",))
    t2 = threading.Thread(target=worker, args=("t2",))
    t1.start()
    t2.start()
    t1.join()
    t2.join()

    assert len(session_ids) == 2
    assert session_ids[0] != session_ids[1]
    assert ctx_session() is None


def test_chunk_read_releases_session(registry):
    dao = _UserDAO()
    with tx_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
        for i in range(50):
            dao.create(f"name-{i}")

    with session_scope(DBKind.SQLITE, "scope_sqlite", registry=registry):
        session = require_ctx_session()
        result = session.execute(
            select(_User).order_by(_User.id).execution_options(stream_results=True)
        )
        rows = result.scalars().yield_per(10)
        collected = [row.id for row in rows]
    assert len(collected) == 50
    assert ctx_session() is None


def test_ctx_session_missing_raises():
    with pytest.raises(RuntimeError):
        require_ctx_session()
