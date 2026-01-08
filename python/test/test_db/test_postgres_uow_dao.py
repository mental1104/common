import os

import pytest
from sqlalchemy import Integer, String, inspect, select, text
from sqlalchemy.orm import DeclarativeBase, mapped_column

from mental1104.db import (
    ConnParams,
    DBKind,
    create_sqlalchemy_client,
    require_ctx_session,
    session_scope,
    tx_scope,
)
from mental1104.db import AutoSessionDAO

pytest.importorskip("psycopg")

# 必须配置 PG* 环境变量，否则跳过这些集成测试。
REQUIRED_ENV = ["PGUSER", "PGPASSWORD", "PGHOST", "PGPORT", "PGDATABASE"]
MISSING_ENV = [key for key in REQUIRED_ENV if not os.getenv(key)]

pytestmark = pytest.mark.skipif(
    bool(MISSING_ENV),
    reason="PG* env vars are not fully configured; skip PostgreSQL db tests",
)


def _pg_params() -> ConnParams:
    # 从环境变量拼出连接参数。
    app_name = os.getenv("PGAPPNAME") or "mental1104-test-db"  # 设置连接的 application_name，方便在 PG 侧审计/定位该测试连接
    return ConnParams(
        ip=os.environ["PGHOST"],
        port=int(os.environ["PGPORT"]),
        database=os.environ["PGDATABASE"],
        user=os.environ["PGUSER"],
        password=os.environ["PGPASSWORD"],
        options={"connect_args": {"application_name": app_name}},
    )


TABLE_NAME = "mental1104_test_db_pg_user_uow_dao_static"  # 固定且足够独特的表名，便于业务参考


class _TestBase(DeclarativeBase):
    # 测试用的 DeclarativeBase，避免污染全局 Base。
    __allow_unmapped__ = True  # 业务中若不想写 Mapped[...] 注解，可开启允许无注解映射
    pass


class _UserModel(_TestBase):
    # 静态模型示例，业务中通常也这样写固定模型类
    __tablename__ = TABLE_NAME
    id = mapped_column(Integer, primary_key=True, autoincrement=True)
    name = mapped_column(String(64), nullable=False, index=True)


class _UserDAO(AutoSessionDAO):
    # 最小 DAO：依赖 AutoSessionDAO 自动注入会话。
    _model = _UserModel  # 静态绑定模型类，业务中通常一个 DAO 绑定一个模型

    def create(self, name: str, *, db):
        row = self._model(name=name)
        db.add(row)
        db.flush()
        return row

    def list(self, *, db):
        result = db.execute(select(self._model).order_by(self._model.id))
        return list(result.scalars().all())


def _connect_raw(params: ConnParams):
    # 直接用 psycopg 建立原生连接，执行管理类 SQL。
    import psycopg

    return psycopg.connect(
        dbname=params.database,
        user=params.user,
        password=params.password,
        host=params.ip,
        port=params.port,
    )


def _lookup_pg_terminate_backend(params: ConnParams):
    # 查 pg_terminate_backend 的签名（参数个数在不同版本可能不同）。
    with _connect_raw(params) as conn:
        conn.autocommit = True
        with conn.cursor() as cur:
            cur.execute(
                """
                select pronargs, p.oid
                from pg_proc p
                join pg_namespace n on p.pronamespace = n.oid
                where n.nspname = 'pg_catalog'
                  and proname = 'pg_terminate_backend'
                order by pronargs asc
                limit 1
                """
            )
            row = cur.fetchone()
            if not row:
                return None
            return {"arg_count": row[0], "oid": row[1]}


def _can_terminate_backends(params: ConnParams) -> bool:
    # 当前用户是否有执行 pg_terminate_backend 的权限。
    meta = _lookup_pg_terminate_backend(params)
    if meta is None:
        return False
    with _connect_raw(params) as conn:
        conn.autocommit = True
        with conn.cursor() as cur:
            cur.execute(
                "select has_function_privilege(%s::regprocedure, 'EXECUTE')",
                (meta["oid"],),
            )
            return bool(cur.fetchone()[0])


def _kill_backend(params: ConnParams, pid: int) -> None:
    # 主动杀掉指定连接，用于验证 pre_ping 是否能恢复连接。
    meta = _lookup_pg_terminate_backend(params)
    if meta is None:
        raise RuntimeError("pg_terminate_backend is not available")
    with _connect_raw(params) as conn:
        conn.autocommit = True
        with conn.cursor() as cur:
            if meta["arg_count"] == 1:
                cur.execute("select pg_catalog.pg_terminate_backend(%s)", (pid,))
            elif meta["arg_count"] == 2:
                cur.execute("select pg_catalog.pg_terminate_backend(%s, %s)", (pid, 0))
            else:
                raise RuntimeError(f"unsupported pg_terminate_backend signature: {meta}")



@pytest.fixture(scope="class")
def pg_client(request):
    # 类级别复用 client，避免每个用例重复创建连接池。
    client = create_sqlalchemy_client(DBKind.POSTGRES, _pg_params())
    if request.cls is not None:
        request.cls.client = client
    yield client
    client.close()

@pytest.mark.usefixtures("pg_client")
class TestPostgresUowDao:
    # 该类内所有用例复用同一个 client（连接池）。
    def test_postgres_scope_injects_session(self):
        client = self.client  # 业务中通常在应用启动时创建并注入 client/SessionMaker
        dao = _UserDAO()  # 静态 DAO（绑定固定模型），更接近业务代码写法

        try:
            _TestBase.metadata.create_all(bind=client.engine, tables=[_UserModel.__table__])  # 先建表，准备后续 DAO 操作
            inspector = inspect(client.engine)  # 读取元数据验证表确实创建成功
            assert TABLE_NAME in inspector.get_table_names()

            with tx_scope(client=client):  # 进入 tx_scope，开启事务并把 session 放进上下文变量
                # 目的：验证 scope 能注入 session，DAO 在同一上下文拿到同一个 session/连接
                session_a = require_ctx_session()  # 第一次取上下文 session
                dao.create("alice")  # AutoSessionDAO 会取当前上下文 session（未显式传参），因此必然是 session_a/session_b
                session_b = require_ctx_session()  # 再次取 session，应该是同一个对象
                assert session_a is session_b
                conn_a = session_a.connection()  # 验证底层连接也一致
                conn_b = session_b.connection()
                assert conn_a.connection is conn_b.connection

            with session_scope(client=client):
                session = require_ctx_session()
                rows = session.execute(select(_UserModel)).scalars().all()
            assert len(rows) == 1
        finally:
            with client.engine.begin() as conn:
                _UserModel.__table__.drop(conn, checkfirst=True)

    def test_postgres_dao_without_uow_raises(self):
        # 未进入 session_scope/tx_scope 时，AutoSessionDAO 不应允许隐式访问 session。
        dao = _UserDAO()

        with pytest.raises(RuntimeError):
            dao.create("bob")

    def test_postgres_explicit_session_overrides_context(self):
        # 显式传 session 时，不应被 scope 上下文里的 session 覆盖。
        client = self.client
        dao = _UserDAO()

        try:
            _TestBase.metadata.create_all(bind=client.engine, tables=[_UserModel.__table__])

            with tx_scope(client=client):
                explicit = client.SessionMaker()
                try:
                    dao.create("x", session=explicit)
                    explicit.rollback()
                finally:
                    explicit.close()

            with session_scope(client=client):
                # 显式 session rollback 后，数据不应落库。
                session = require_ctx_session()
                rows = session.execute(select(_UserModel).where(_UserModel.name == "x")).scalars().all()
            assert rows == []
        finally:
            with client.engine.begin() as conn:
                _UserModel.__table__.drop(conn, checkfirst=True)

    def test_postgres_pre_ping_recovers_after_backend_killed(self):
        # 验证 SQLAlchemy 的 pre_ping 能在连接被杀后恢复。
        params = _pg_params()
        if not _can_terminate_backends(params):
            pytest.skip("current postgres user cannot execute pg_terminate_backend")

        client = self.client
        with session_scope(client=client):
            session = require_ctx_session()
            pid = session.execute(text("select pg_backend_pid()")).scalar_one()

        _kill_backend(params, pid)

        with session_scope(client=client):
            session = require_ctx_session()
            new_pid = session.execute(text("select pg_backend_pid()")).scalar_one()
        assert new_pid != pid
