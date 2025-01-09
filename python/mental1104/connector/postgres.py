import os
import logging
from sqlalchemy import create_engine, inspect
from sqlalchemy.orm import sessionmaker, declarative_base
from contextlib import contextmanager
import psycopg2
from psycopg2.extensions import ISOLATION_LEVEL_AUTOCOMMIT
from psycopg2 import OperationalError, DatabaseError

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# 全局Session和Base定义
_Session = sessionmaker()
Base = declarative_base()

def get_db_config():
    """从环境变量获取PostgreSQL配置信息"""
    required_env_vars = ['PGUSER', 'PGPASSWORD', 'PGHOST', 'PGPORT', 'PGDATABASE']
    missing_vars = [var for var in required_env_vars if var not in os.environ]
    
    if missing_vars:
        raise RuntimeError(f"缺少以下环境变量: {', '.join(missing_vars)}")

    return {
        "username": os.getenv('PGUSER'),
        "password": os.getenv('PGPASSWORD'),
        "host": os.getenv('PGHOST'),
        "port": os.getenv('PGPORT'),
        "database": os.getenv('PGDATABASE'),
    }

def ensure_database_exists(config):
    """确保数据库存在，不存在时尝试创建，失败则记录日志"""
    try:
        conn = psycopg2.connect(
            dbname="postgres",  # 连接到默认的 postgres 数据库
            user=config['username'],
            password=config['password'],
            host=config['host'],
            port=config['port']
        )
        conn.set_isolation_level(ISOLATION_LEVEL_AUTOCOMMIT)
        cursor = conn.cursor()

        cursor.execute("SELECT 1 FROM pg_catalog.pg_database WHERE datname = %s", (config['database'],))
        exists = cursor.fetchone()

        if not exists:
            logger.info(f"数据库 {config['database']} 不存在，尝试创建...")
            try:
                cursor.execute(f"CREATE DATABASE {config['database']}")
                logger.info(f"数据库 {config['database']} 创建完成")
            except DatabaseError as e:
                logger.error(f"创建数据库 {config['database']} 失败：{e}")
        else:
            logger.info(f"数据库 {config['database']} 已存在")

        cursor.close()
        conn.close()
    except OperationalError as e:
        logger.error(f"连接到默认数据库失败，无法检查或创建目标数据库：{e}")

def ensure_tables_exist(engine):
    """检查并按需创建数据库表"""
    inspector = inspect(engine)
    existing_tables = inspector.get_table_names()
    for table in Base.metadata.tables.keys():
        if table not in existing_tables:
            logger.info(f"表 {table} 不存在，正在创建...")
            Base.metadata.create_all(bind=engine)
            logger.info(f"表 {table} 创建完成")
        else:
            logger.info(f"表 {table} 已存在，跳过创建")
    return True

def startup():
    """初始化数据库连接并确保表和数据库存在"""
    config = get_db_config()
    ensure_database_exists(config)

    sqlalchemy_url = f"postgresql://{config['username']}:{config['password']}@{config['host']}:{config['port']}/{config['database']}"
    logger.info(f"数据库URL: {sqlalchemy_url}")

    engine = create_engine(
        sqlalchemy_url,
        pool_pre_ping=True,
        pool_size=20,
        pool_recycle=3600
    )
    _Session.configure(bind=engine)

    ensure_tables_exist(engine)
    logger.info("数据库初始化完成")
    return engine

@contextmanager
def open_session():
    """数据库会话的上下文管理器"""
    session = _Session()
    try:
        yield session
        session.commit()
    except Exception as e:
        session.rollback()
        logger.error(f"数据库会话发生异常: {e}")
        raise
    finally:
        session.close()
