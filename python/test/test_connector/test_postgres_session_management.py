import logging
import os
import time
import uuid

from functools import lru_cache

import psycopg2
from psycopg2 import errorcodes
import pytest
from sqlalchemy import Column, Integer, String, select, text

from mental1104.connector.postgres import (
    Base,
    SessionAwareMixin,
    close_session,
    get_db_config,
    get_session,
    open_session,
    startup,
    with_session,
)


logging.basicConfig()
logging.getLogger("sqlalchemy.pool").setLevel(logging.DEBUG)

REQUIRED_ENV_VARS = ["PGUSER", "PGPASSWORD", "PGHOST", "PGPORT", "PGDATABASE"]
MISSING_ENV = [var for var in REQUIRED_ENV_VARS if not os.getenv(var)]

pytestmark = pytest.mark.skipif(
    bool(MISSING_ENV),
    reason="PG* 环境变量未配置完整，跳过 PostgreSQL 连接器集成测试",
)


class SessionProbe(Base):
    __tablename__ = "test_connector_session_probe"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String(100), unique=True, nullable=False)


class AutoSessionEntity(SessionAwareMixin, Base):
    __tablename__ = "test_connector_auto_session"

    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String(100), unique=True, nullable=False)
    """用于验证 SessionAwareMixin 自动注入能力的临时 ORM 实体。"""

    @classmethod
    def create(cls, name: str, session=None):
        """
        插入新记录并返回实例（用于检测 classmethod 自动注入）。
        """
        row = cls(name=name)
        session.add(row)
        session.flush()
        return row

    @classmethod
    def list_names(cls, session=None):
        """
        以创建顺序返回所有 name 字段，验证查询类方法的注入效果。
        """
        query = session.query(cls).order_by(cls.id)
        return [row.name for row in query.all()]

    @staticmethod
    def count_rows(session=None):
        """
        统计表内记录数量，覆盖 staticmethod 注入路径。
        """
        return session.query(AutoSessionEntity).count()


@with_session
def insert_probe(name: str, session=None):
    row = SessionProbe(name=name)
    session.add(row)
    session.flush()
    return row.id


@with_session
def load_probe_by_name(name: str, session=None):
    stmt = select(SessionProbe).where(SessionProbe.name == name)
    return session.execute(stmt).scalar_one_or_none()


@pytest.fixture(scope="module")
def db_config():
    return get_db_config()


@pytest.fixture(scope="module")
def engine(db_config):
    engine = startup()
    Base.metadata.create_all(bind=engine)
    yield engine
    with engine.connect() as conn:
        conn.execute(text("DROP TABLE IF EXISTS test_connector_auto_session"))
        conn.execute(text("DROP TABLE IF EXISTS test_connector_session_probe"))
        conn.commit()
    engine.dispose()


@pytest.fixture(autouse=True)
def cleanup_probe_rows(engine):
    yield
    with engine.connect() as conn:
        conn.execute(text("DELETE FROM test_connector_auto_session"))
        conn.execute(text("DELETE FROM test_connector_session_probe"))
        conn.commit()


def _connect_raw(config):
    return psycopg2.connect(
        dbname=config["database"],
        user=config["username"],
        password=config["password"],
        host=config["host"],
        port=config["port"],
    )


def _lookup_pg_terminate_backend(config):
    return _lookup_pg_terminate_backend_cached(
        config["host"],
        config["port"],
        config["database"],
        config["username"],
        config["password"],
    )


@lru_cache(maxsize=8)
def _lookup_pg_terminate_backend_cached(host, port, database, username, password):
    conn = psycopg2.connect(
        dbname=database,
        user=username,
        password=password,
        host=host,
        port=port,
    )
    try:
        conn.autocommit = True
        with conn.cursor() as cur:
            cur.execute(
                """
                select
                    pronargs,
                    p.oid
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
    finally:
        conn.close()


def _count_active_app_connections(config):
    with _connect_raw(config) as conn:
        conn.autocommit = True
        with conn.cursor() as cur:
            cur.execute(
                """
                select count(*)
                from pg_stat_activity
                where datname = %s
                  and application_name = %s
                  and state <> 'idle'
                """,
                (config["database"], config["application_name"]),
            )
            return cur.fetchone()[0]


def _kill_backend(config, pid):
    meta = _lookup_pg_terminate_backend(config)
    if meta is None:
        raise RuntimeError("pg_terminate_backend not available in current database")
    with _connect_raw(config) as conn:
        conn.autocommit = True
        with conn.cursor() as cur:
            if meta["arg_count"] == 1:
                cur.execute("select pg_catalog.pg_terminate_backend(%s)", (pid,))
            elif meta["arg_count"] == 2:
                cur.execute("select pg_catalog.pg_terminate_backend(%s, %s)", (pid, 0))
            else:
                raise RuntimeError(f"Unsupported pg_terminate_backend signature: {meta}")


def _can_terminate_backends(config):
    meta = _lookup_pg_terminate_backend(config)
    if meta is None:
        return False
    try:
        with _connect_raw(config) as conn:
            conn.autocommit = True
            with conn.cursor() as cur:
                cur.execute(
                    """
                    select has_function_privilege(
                        %s::regprocedure,
                        'EXECUTE'
                    )
                    """,
                    (meta["oid"],),
                )
                return cur.fetchone()[0]
    except psycopg2.Error as exc:
        if getattr(exc, "pgcode", None) == errorcodes.UNDEFINED_FUNCTION:
            return False
        raise


def _wait_for(predicate, expected, timeout=5.0, interval=0.05):
    deadline = time.time() + timeout
    last_value = None
    while time.time() < deadline:
        last_value = predicate()
        if last_value == expected:
            return last_value
        time.sleep(interval)
    raise AssertionError(f"等待超时：期望 {expected}，实际 {last_value}")


def _reset_pool(engine, config):
    close_session()
    engine.dispose()
    _wait_for(lambda: engine.pool.checkedout(), 0)
    _wait_for(lambda: _count_active_app_connections(config), 0)


def test_open_session_reuses_single_connection(engine, db_config):
    """
    【场景背景】验证 open_session() 作为上层事务边界时，整个调用栈应复用同一
    个 Session/连接，以免在 DAO 间来回穿 session。
    【步骤输入】在 with open_session(): 块内多次调用 insert_probe / load_probe，
    并重复调用 get_session() 获取当前 Session。
    【期望输出】连接池 checkedout 上升 1 表示只借出一个连接，两个 get_session()
    返回完全相同的对象且底层 DBAPI connection 也一致，退出上下文后连接数恢复。
    """
    _reset_pool(engine, db_config)

    names = [f"probe-{uuid.uuid4()}" for _ in range(3)]

    with open_session():
        for name in names:
            insert_probe(name)
            assert load_probe_by_name(name) is not None

        _wait_for(lambda: engine.pool.checkedout(), 1)
        _wait_for(lambda: _count_active_app_connections(db_config), 1)

        session_a = get_session()
        session_b = get_session()
        assert session_a is session_b

        conn_a = session_a.connection()
        conn_b = session_b.connection()
        assert conn_a.connection is conn_b.connection

    _wait_for(lambda: engine.pool.checkedout(), 0)
    _wait_for(lambda: _count_active_app_connections(db_config), 0)


def test_pool_pre_ping_recovers_after_backend_killed(engine, db_config):
    """
    【场景背景】pool_pre_ping=True 时应在连接被服务器杀掉后自动检测并置换，
    这是保障长连接稳定性的关键特性。
    【步骤输入】第一次 open_session() 查询 pg_backend_pid() 记住连接，随后
    通过 pg_terminate_backend 主动杀死该 PID，再次进入 open_session()。
    【期望输出】第二次获取的会话 PID 应与旧值不同，说明连接被重建；并且
    insert_probe/load_probe 还能成功执行，证明业务层无需额外重试即可恢复。
    """
    if not _can_terminate_backends(db_config):
        pytest.skip("当前数据库用户没有 pg_terminate_backend 权限")

    _reset_pool(engine, db_config)

    with open_session():
        session = get_session()
        pid = session.execute(text("select pg_backend_pid()")).scalar_one()

    _kill_backend(db_config, pid)

    with open_session():
        session = get_session()
        new_pid = session.execute(text("select pg_backend_pid()")).scalar_one()
        assert new_pid != pid

        inserted_name = f"probe-{uuid.uuid4()}"
        insert_probe(inserted_name)
        assert load_probe_by_name(inserted_name) is not None

    _wait_for(lambda: engine.pool.checkedout(), 0)
    _wait_for(lambda: _count_active_app_connections(db_config), 0)


def test_lazy_session_requires_manual_close(engine, db_config):
    """
    【场景背景】当调用方绕过 open_session() 而直接依赖 get_session() 懒加载时，
    需要手动管理事务提交与连接释放，否则连接池可能被耗尽。
    【步骤输入】直接调用 get_session() 执行查询与插入，但不 commit；另开
    open_session() 检查数据库视图，并在最后显示 close_session()。
    【期望输出】连接池 checkedout 增加 1 且未经提交的新行对其他 Session 不可见，
    调用 close_session() 后连接数降回基线，证明资源释放仍需调用方负责。
    """
    _reset_pool(engine, db_config)

    lazy_session = get_session()
    lazy_session.execute(text("select 1"))

    _wait_for(lambda: engine.pool.checkedout(), 1)
    _wait_for(lambda: _count_active_app_connections(db_config), 1)

    pending_name = f"lazy-{uuid.uuid4()}"
    lazy_session.add(SessionProbe(name=pending_name))
    lazy_session.flush()

    with open_session():
        isolated_session = get_session()
        stmt = select(SessionProbe).where(SessionProbe.name == pending_name)
        result = isolated_session.execute(stmt).first()
        assert result is None

    lazy_session.rollback()
    close_session()

    _wait_for(lambda: engine.pool.checkedout(), 0)
    _wait_for(lambda: _count_active_app_connections(db_config), 0)


def test_session_aware_mixin_auto_wrap(engine, db_config):
    """
    【场景背景】SessionAwareMixin 应自动为声明 session 参数的 classmethod/staticmethod
    注入 with_session，消除 DAO 层逐个装饰的需求。
    【步骤输入】定义 AutoSessionEntity 继承 mixin，在 open_session() 中调用 create、
    list_names、count_rows，整个过程不显式传 session。
    【期望输出】成功插入两条记录，list_names 返回插入顺序，count_rows 返回数量，
    证明 session 被自动注入。
    """
    _reset_pool(engine, db_config)

    created = [f"auto-{i}" for i in range(2)]

    with open_session():
        for name in created:
            AutoSessionEntity.create(name)

        assert AutoSessionEntity.list_names() == created
        assert AutoSessionEntity.count_rows() == len(created)

    _wait_for(lambda: engine.pool.checkedout(), 0)
    _wait_for(lambda: _count_active_app_connections(db_config), 0)
