import os
import pytest
from sqlalchemy import create_engine, text, inspect, Column, Integer, String
from contextlib import contextmanager
from mental1104.connector.postgres import startup, Base, get_db_config, open_session

@contextmanager
def connect_to_postgres():
    """连接到默认的postgres数据库"""
    config = get_db_config()
    connection_url = f"postgresql://{config['username']}:{config['password']}@{config['host']}:{config['port']}/postgres"
    engine = create_engine(connection_url)
    conn = engine.connect()
    try:
        yield conn
    finally:
        conn.close()

def delete_test_database(config):
    """删除测试数据库（跳过postgres数据库）"""
    if config["database"] == "postgres":
        print("跳过删除默认数据库: postgres")
        return

    with connect_to_postgres() as conn:
        # 切换到管理数据库（如 postgres）
        conn = conn.execution_options(isolation_level="AUTOCOMMIT")
        db_name = config["database"]

        try:
            # 强制终止与目标数据库的所有连接
            conn.execute(text(f"""
                SELECT pg_terminate_backend(pg_stat_activity.pid)
                FROM pg_stat_activity
                WHERE pg_stat_activity.datname = '{db_name}'
                  AND pid <> pg_backend_pid();
            """))
            
            # 删除数据库
            conn.execute(text(f"DROP DATABASE IF EXISTS {db_name}"))
            print(f"测试数据库 {db_name} 删除完成")
        except Exception as e:
            print(f"删除数据库 {db_name} 时出错: {e}")


# 定义测试 ORM 类
class TempTable(Base):
    __tablename__ = 'test_table'
    id = Column(Integer, primary_key=True, autoincrement=True)
    name = Column(String(50), nullable=False)


@pytest.mark.skipif(
    not all([os.getenv('PGUSER'), os.getenv('PGPASSWORD'), os.getenv('PGHOST'), os.getenv('PGPORT'), os.getenv('PGDATABASE')]),
    reason="环境变量未配置完整，跳过测试"
)
class TestDatabase:
    def setup_class(self):
        """在测试开始时初始化数据库"""
        self.config = {
            "username": os.getenv('PGUSER'),
            "password": os.getenv('PGPASSWORD'),
            "host": os.getenv('PGHOST'),
            "port": os.getenv('PGPORT'),
            "database": "test_database"
        }
        delete_test_database(self.config)  # 确保测试开始前数据库被清理
        startup()

    def teardown_class(self):
        """在测试结束时删除数据库"""
        delete_test_database(self.config)

    def test_orm_table_creation_and_verification(self):
        """验证根据 ORM 类构建表并校验表是否存在的逻辑"""
        # 1. 启动数据库并创建表
        startup()  # 初始化数据库引擎和会话
        Base.metadata.create_all(bind=startup().engine)

        # 2. 校验表是否存在
        engine = startup().engine
        inspector = inspect(engine)
        tables = inspector.get_table_names()
        assert "test_table" in tables, "表 'test_table' 应该存在"

        # 3. 插入数据并验证
        with open_session() as session:
            new_entry = TempTable(name="Test Entry")
            session.add(new_entry)
            session.commit()

            # 验证数据是否插入成功
            result = session.query(TempTable).filter_by(name="Test Entry").one_or_none()
            assert result is not None, "表 'test_table' 应该包含插入的数据"

        # 4. 删除表
        # 强制使用 AUTOCOMMIT 执行 DROP 语句
        with engine.connect() as conn:
            conn.execution_options(isolation_level="AUTOCOMMIT").execute(text("DROP TABLE IF EXISTS test_table"))

        # 5. 使用原生 SQL 校验表是否删除
        with engine.connect() as conn:
            result = conn.execute(text("SELECT 1 FROM pg_tables WHERE tablename = 'test_table'"))
            assert result.fetchone() is None, "表 'test_table' 应该已被删除"