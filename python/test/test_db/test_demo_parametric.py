import itertools
import os
import uuid

import pytest
from sqlalchemy import Integer, String, select
from sqlalchemy.orm import DeclarativeBase, mapped_column

from mental1104.db import (
    AutoSessionDAO,
    ConnParams,
    DBKind,
    DBRegistry,
    async_session_scope,
    async_tx_scope,
    make_async_dao,
    register_db_and_create,
    register_db_and_create_async,
    session_scope,
    tx_scope,
)


def _make_base_and_model(table_name: str):
    class _Base(DeclarativeBase):
        pass

    class _User(_Base):
        __tablename__ = table_name
        id = mapped_column(Integer, primary_key=True, autoincrement=True)
        name = mapped_column(String(64), nullable=False, index=True)

    return _Base, _User


def _require_clickhouse_sqlalchemy():
    try:
        from clickhouse_sqlalchemy import engines, types
    except Exception as exc:
        raise RuntimeError("Missing dependency: clickhouse-sqlalchemy") from exc
    return engines, types


def _make_clickhouse_base_and_model(table_name: str):
    engines, types = _require_clickhouse_sqlalchemy()

    class _Base(DeclarativeBase):
        pass

    class _User(_Base):
        __tablename__ = table_name
        __table_args__ = (engines.MergeTree(order_by=("id",)),)
        id = mapped_column(types.UInt64, primary_key=True)
        name = mapped_column(types.String, nullable=False)

    return _Base, _User


def _make_user_dao(model, *, id_factory=None):
    class _UserDAO(AutoSessionDAO):
        _model = model

        def create(self, name: str, *, db):
            values = {"name": name}
            if id_factory is not None:
                values["id"] = id_factory()
            row = self._model(**values)
            db.add(row)
            db.flush()
            return row

        def list(self, *, db):
            result = db.execute(select(self._model).order_by(self._model.id))
            return list(result.scalars().all())

    return _UserDAO


def _pg_params(async_mode: bool) -> ConnParams:
    required = ["PGUSER", "PGPASSWORD", "PGHOST", "PGPORT", "PGDATABASE"]
    missing = [key for key in required if not os.getenv(key)]
    if missing:
        pytest.skip("PG* env vars are not fully configured")
    pytest.importorskip("psycopg")
    options = {}
    async_driver = os.getenv("PG_ASYNC_DRIVER")
    if async_mode and async_driver:
        options["async_driver"] = async_driver
    return ConnParams(
        ip=os.environ["PGHOST"],
        port=int(os.environ["PGPORT"]),
        database=os.environ["PGDATABASE"],
        user=os.environ["PGUSER"],
        password=os.environ["PGPASSWORD"],
        options=options,
    )


def _mysql_params(async_mode: bool) -> ConnParams:
    required = ["MYSQL_USER", "MYSQL_PASSWORD", "MYSQL_HOST", "MYSQL_PORT", "MYSQL_DATABASE"]
    missing = [key for key in required if not os.getenv(key)]
    if missing:
        pytest.skip("MYSQL_* env vars are not fully configured")
    if async_mode:
        async_driver = os.getenv("MYSQL_ASYNC_DRIVER")
        if async_driver:
            pytest.importorskip(async_driver)
            options = {"async_driver": async_driver}
        else:
            pytest.importorskip("asyncmy")
            options = {"async_driver": "asyncmy"}
    else:
        pytest.importorskip("pymysql")
        options = {}
    return ConnParams(
        ip=os.environ["MYSQL_HOST"],
        port=int(os.environ["MYSQL_PORT"]),
        database=os.environ["MYSQL_DATABASE"],
        user=os.environ["MYSQL_USER"],
        password=os.environ["MYSQL_PASSWORD"],
        options=options,
    )


def _clickhouse_params() -> ConnParams:
    required = ["CLICKHOUSE_HOST"]
    missing = [key for key in required if not os.getenv(key)]
    if missing:
        pytest.skip("CLICKHOUSE_* env vars are not fully configured")
    scheme = os.getenv("CLICKHOUSE_SCHEME", "clickhouse+http")
    if scheme.startswith("clickhousedb"):
        raise RuntimeError(
            "clickhousedb scheme uses clickhouse-connect; "
            "this ORM demo requires clickhouse-sqlalchemy (clickhouse+http/native)."
        )
    _require_clickhouse_sqlalchemy()
    port_raw = os.getenv("CLICKHOUSE_HTTP_PORT")
    port = int(port_raw) if port_raw else 8123
    database = os.getenv("CLICKHOUSE_DATABASE") or "default"
    user = os.getenv("CLICKHOUSE_USER") or None
    password = os.getenv("CLICKHOUSE_PASSWORD") or None
    return ConnParams(
        ip=os.environ["CLICKHOUSE_HOST"],
        port=port,
        database=database,
        user=user,
        password=password,
        options={"scheme": scheme},
    )


def _build_params(kind: DBKind, tmp_path, async_mode: bool) -> ConnParams:
    if kind == DBKind.SQLITE:
        if async_mode:
            pytest.importorskip("aiosqlite")
        db_path = tmp_path / ("demo_async.db" if async_mode else "demo.db")
        return ConnParams(ip=str(db_path))
    if kind == DBKind.POSTGRES:
        return _pg_params(async_mode)
    if kind == DBKind.MYSQL:
        return _mysql_params(async_mode)
    if kind == DBKind.CLICKHOUSE:
        if async_mode:
            pytest.skip("ClickHouse async is not supported")
        return _clickhouse_params()
    raise ValueError(f"unsupported kind: {kind}")


@pytest.mark.parametrize("kind", [DBKind.SQLITE, DBKind.POSTGRES, DBKind.MYSQL, DBKind.CLICKHOUSE])
def test_demo_sync(kind, tmp_path):
    params = _build_params(kind, tmp_path, async_mode=False)
    registry = DBRegistry()
    model = None

    try:
        table_name = f"demo_user_{uuid.uuid4().hex}"
        if kind == DBKind.CLICKHOUSE:
            base, model = _make_clickhouse_base_and_model(table_name)
            counter = itertools.count(1)
            dao_cls = _make_user_dao(model, id_factory=counter.__next__)
        else:
            base, model = _make_base_and_model(table_name)
            dao_cls = _make_user_dao(model)
        dao = dao_cls()
        register_db_and_create(
            kind,
            params=params,
            registry=registry,
            base=base,
            tables=[model.__table__],
        )

        with tx_scope(kind, registry=registry):
            dao.create("alice")
            dao.create("bob")

        with session_scope(kind, registry=registry):
            rows = dao.list()
        assert [r.name for r in rows] == ["alice", "bob"]
    finally:
        try:
            if model is not None:
                engine = registry.get_engine(kind)
                model.__table__.drop(engine, checkfirst=True)
        except KeyError:
            pass
        finally:
            try:
                registry.get_client(kind).close()
            except KeyError:
                pass


@pytest.mark.asyncio
@pytest.mark.parametrize("kind", [DBKind.SQLITE, DBKind.POSTGRES, DBKind.MYSQL, DBKind.CLICKHOUSE])
async def test_demo_async(kind, tmp_path):
    params = _build_params(kind, tmp_path, async_mode=True)
    if kind == DBKind.CLICKHOUSE:
        pytest.skip("ClickHouse async is not supported")
    table_name = f"demo_user_async_{uuid.uuid4().hex}"
    base, model = _make_base_and_model(table_name)
    dao_cls = _make_user_dao(model)
    async_dao_cls = make_async_dao(dao_cls, name="AsyncDemoDAO")
    dao = async_dao_cls()
    registry = DBRegistry()

    try:
        await register_db_and_create_async(
            kind,
            params=params,
            registry=registry,
            base=base,
            tables=[model.__table__],
        )

        async with async_tx_scope(kind, registry=registry):
            await dao.create("alice")
            await dao.create("bob")

        async with async_session_scope(kind, registry=registry):
            rows = await dao.list()
        assert [r.name for r in rows] == ["alice", "bob"]
    finally:
        try:
            engine = registry.get_async_engine(kind)
            async with engine.begin() as conn:
                await conn.run_sync(model.__table__.drop, checkfirst=True)
        finally:
            await registry.get_async_client(kind).close()
