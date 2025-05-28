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


"""
从数据库中获取markdown文件
"""

import psycopg2
from functools import wraps

def db_connection(db_type, db_params):
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            conn = None
            cursor = None
            try:
                # 连接数据库
                if db_type == 'postgresql':
                    conn = psycopg2.connect(**db_params)
                    cursor = conn.cursor()
                else:
                    raise ValueError("Unsupported database type.")
                
                offset = 0
                limit = 1000
                
                while True:
                    # 执行传入的 SQL 语句
                    sql, params = kwargs.get('sql'), kwargs.get('params', {})
                    if not sql:
                        raise ValueError("SQL query must be provided.")

                    # 处理 SQL 语句的 offset 和 limit
                    params.update({"offset": offset, "limit": limit})
                    if db_type == 'postgresql':
                        cursor.execute(sql, params)
                        result = cursor.fetchall()

                    if len(result) == 0:
                        break  # 当记录数为 0 时退出
                        
                    # 针对每一条记录执行传入的处理函数
                    for item in result:
                        func(item)
                    
                    offset += limit

            except Exception as e:
                if conn:
                    if db_type == 'postgresql':
                        conn.rollback()
                print(f"错误: {str(e)}")
            finally:
                # 关闭连接
                if cursor:
                    cursor.close()
                if conn and db_type == 'postgresql':
                    conn.close()
    
        return wrapper
    
    return decorator

# 使用示例
db_params_postgres = {
    "database": "xdlp",
    "user": "xdlp",
    "password": "xdlpllm2024",
    "host": "10.74.113.42",
    "port": "5432"
}

db_params_clickhouse = {
    "host": "clickhouse_host",
    "user": "default",
    "password": "password"
}

@db_connection('postgresql', db_params_postgres)
def process_record(item):
    # 处理每条记录的逻辑
    markdown, file_name, label = item
    print(f"Processing file: {file_name}, with label: {label}, content")

# 调用示例
sql_query = """
SELECT markdown, file_name, label FROM dataset
WHERE markdown != '' AND markdown != 'XDLP ERROR'
ORDER BY sha_256 ASC
LIMIT %(limit)s OFFSET %(offset)s
"""

params = {
    "version": "yzf-v00"
}

# 调用装饰器时传入 SQL 和参数
# process_record(sql=sql_query, params=params)