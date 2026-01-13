import os
import sys
import uuid

import pytest
from sqlalchemy import text

from mental1104.db import (
    ConnParams,
    DBKind,
    UnitOfWork,
    create_sqlalchemy_client,
    require_ctx_session,
    session_scope,
)
from mental1104.db import AutoSessionDAO

if sys.version_info < (3, 9):
    pytest.skip(
        "clickhouse_connect requires Python >= 3.9",
        allow_module_level=True,
    )

pytest.importorskip("clickhouse_connect")

REQUIRED_ENV = [
    "CLICKHOUSE_HOST",
    "CLICKHOUSE_HTTP_PORT",
    "CLICKHOUSE_DATABASE",
    "CLICKHOUSE_USER",
]
MISSING_ENV = [key for key in REQUIRED_ENV if not os.getenv(key)]

pytestmark = pytest.mark.skipif(
    bool(MISSING_ENV),
    reason="CLICKHOUSE_* env vars are not fully configured; skip ClickHouse db tests",
)


def _clickhouse_params() -> ConnParams:
    host = os.environ["CLICKHOUSE_HOST"]
    port_raw = os.getenv("CLICKHOUSE_HTTP_PORT")
    port = int(port_raw) if port_raw else 8123
    database = os.getenv("CLICKHOUSE_DATABASE") or "default"
    user = os.getenv("CLICKHOUSE_USER") or None
    password = os.getenv("CLICKHOUSE_PASSWORD") or None
    return ConnParams(
        ip=host,
        port=port,
        database=database,
        user=user,
        password=password,
    )


def _table_name(prefix: str) -> str:
    return f"{prefix}{uuid.uuid4().hex}"


class _ClickHouseDAO(AutoSessionDAO):
    def create_table(self, table: str, *, db) -> None:
        db.execute(
            text(
                f"CREATE TABLE IF NOT EXISTS {table} (id UInt64, name String) "
                "ENGINE = MergeTree ORDER BY id"
            )
        )

    def drop_table(self, table: str, *, db) -> None:
        db.execute(text(f"DROP TABLE IF EXISTS {table}"))

    def insert_row(self, table: str, row_id: int, name: str, *, db) -> None:
        db.execute(
            text(f"INSERT INTO {table} (id, name) VALUES (:id, :name)"),
            {"id": row_id, "name": name},
        )

    def count_rows(self, table: str, *, db) -> int:
        return db.execute(text(f"SELECT count() FROM {table}")).scalar_one()

    def session_id(self, *, db) -> int:
        return id(db)


def test_clickhouse_uow_injects_session():
    client = create_sqlalchemy_client(DBKind.CLICKHOUSE, _clickhouse_params())
    table_name = _table_name("test_db_ch_")
    dao = _ClickHouseDAO()
    uow = UnitOfWork(client)

    try:
        with uow():
            dao.create_table(table_name)
            dao.insert_row(table_name, 1, "alice")
            assert dao.count_rows(table_name) == 1
    finally:
        try:
            with session_scope(client=client):
                session = require_ctx_session()
                session.execute(text(f"DROP TABLE IF EXISTS {table_name}"))
        finally:
            client.close()


def test_clickhouse_dao_without_uow_raises():
    dao = _ClickHouseDAO()
    with pytest.raises(RuntimeError):
        dao.count_rows("any_table")


def test_clickhouse_explicit_session_overrides_context():
    client = create_sqlalchemy_client(DBKind.CLICKHOUSE, _clickhouse_params())
    dao = _ClickHouseDAO()
    uow = UnitOfWork(client)

    try:
        with uow():
            current = require_ctx_session()
            explicit = client.SessionMaker()
            try:
                assert current is not None
                assert dao.session_id() == id(current)
                assert dao.session_id(session=explicit) == id(explicit)
            finally:
                explicit.close()
    finally:
        client.close()
