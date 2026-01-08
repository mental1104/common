import os
import uuid

import pytest
from sqlalchemy import Integer, String, inspect, select
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column

from mental1104.db import (
    ConnParams,
    DBKind,
    UnitOfWork,
    create_sqlalchemy_client,
    require_ctx_session,
    session_scope,
)
from mental1104.db import AutoSessionDAO

pytest.importorskip("pymysql")

REQUIRED_ENV = ["MYSQL_USER", "MYSQL_PASSWORD", "MYSQL_HOST", "MYSQL_PORT", "MYSQL_DATABASE"]
MISSING_ENV = [key for key in REQUIRED_ENV if not os.getenv(key)]

pytestmark = pytest.mark.skipif(
    bool(MISSING_ENV),
    reason="MYSQL_* env vars are not fully configured; skip MySQL db tests",
)


def _mysql_params() -> ConnParams:
    return ConnParams(
        ip=os.environ["MYSQL_HOST"],
        port=int(os.environ["MYSQL_PORT"]),
        database=os.environ["MYSQL_DATABASE"],
        user=os.environ["MYSQL_USER"],
        password=os.environ["MYSQL_PASSWORD"],
    )


def _table_name(prefix: str) -> str:
    return f"{prefix}{uuid.uuid4().hex}"


class _TestBase(DeclarativeBase):
    pass


def _make_user_model(table_name: str):
    class_name = f"MyUser_{uuid.uuid4().hex}"
    annotations = {"id": Mapped[int], "name": Mapped[str]}
    return type(
        class_name,
        (_TestBase,),
        {
            "__tablename__": table_name,
            "__annotations__": annotations,
            "id": mapped_column(Integer, primary_key=True, autoincrement=True),
            "name": mapped_column(String(64), nullable=False, index=True),
        },
    )


class _UserDAO(AutoSessionDAO):
    def __init__(self, model):
        self._model = model

    def create(self, name: str, *, db):
        row = self._model(name=name)
        db.add(row)
        db.flush()
        return row

    def list(self, *, db):
        result = db.execute(select(self._model).order_by(self._model.id))
        return list(result.scalars().all())


def test_mysql_uow_injects_session():
    client = create_sqlalchemy_client(DBKind.MYSQL, _mysql_params())
    table_name = _table_name("test_db_my_user_")
    Model = _make_user_model(table_name)
    dao = _UserDAO(Model)
    uow = UnitOfWork(client)

    try:
        _TestBase.metadata.create_all(bind=client.engine, tables=[Model.__table__])
        inspector = inspect(client.engine)
        assert table_name in inspector.get_table_names()

        with uow():
            session_a = require_ctx_session()
            dao.create("alice")
            session_b = require_ctx_session()
            assert session_a is session_b

        with session_scope(client=client):
            session = require_ctx_session()
            rows = session.execute(select(Model)).scalars().all()
        assert len(rows) == 1
    finally:
        with client.engine.begin() as conn:
            Model.__table__.drop(conn, checkfirst=True)
        client.close()


def test_mysql_dao_without_uow_raises():
    client = create_sqlalchemy_client(DBKind.MYSQL, _mysql_params())
    table_name = _table_name("test_db_my_user_")
    Model = _make_user_model(table_name)
    dao = _UserDAO(Model)

    try:
        with pytest.raises(RuntimeError):
            dao.create("bob")
    finally:
        client.close()


def test_mysql_explicit_session_overrides_context():
    client = create_sqlalchemy_client(DBKind.MYSQL, _mysql_params())
    table_name = _table_name("test_db_my_user_")
    Model = _make_user_model(table_name)
    dao = _UserDAO(Model)
    uow = UnitOfWork(client)

    try:
        _TestBase.metadata.create_all(bind=client.engine, tables=[Model.__table__])

        with uow():
            explicit = client.SessionMaker()
            try:
                dao.create("x", session=explicit)
                explicit.rollback()
            finally:
                explicit.close()

        with session_scope(client=client):
            session = require_ctx_session()
            rows = session.execute(select(Model).where(Model.name == "x")).scalars().all()
        assert rows == []
    finally:
        with client.engine.begin() as conn:
            Model.__table__.drop(conn, checkfirst=True)
        client.close()
