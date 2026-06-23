# 数据库框架

此包提供统一的数据库访问层，优先面向 SQLAlchemy，并包含：

- 声明式 ORM 基类 (`Base`) 和 mixin (`TimestampMixin`, `SoftDeleteMixin`)。
- 带进程内缓存的引擎 / 会话注册表。
- 基于 ContextVar 的会话注入，默认无需显式传入 session 参数。
- 明确的读 / 写作用域：读操作使用 `session_scope(DBKind.X)`，写操作使用 `tx_scope(DBKind.X)`。
- 异步变体：`async_session_scope(DBKind.X)` / `async_tx_scope(DBKind.X)`。
- 便捷别名：`pg_session_scope()`、`ck_tx_scope()` 等。

## 作用域规则

- 纯读取：使用 `session_scope(DBKind.X)` 或 `pg_session_scope()`。
- 写入或读写混合：使用 `tx_scope(DBKind.X)` 或 `pg_tx_scope()`。
- ClickHouse（clickhouse-connect）：`tx_scope()` 是无操作包装，不提供 ACID。

## 注册表

每个进程注册一次，然后按数据库类型和数据库名称调用作用域。

```
register_db(DBKind.POSTGRES, dsn="postgresql+psycopg://...", db_name="main_pg")
```

可选：一次调用完成注册和建表（`create_all` 幂等）：

```
register_db_and_create(DBKind.POSTGRES, dsn="postgresql+psycopg://...", db_name="main_pg")
```

ClickHouse（非 SQLAlchemy，使用 clickhouse-connect）：

```
register_db(DBKind.CLICKHOUSE, dsn="clickhouse://...", db_name="analytics_ch", options={"driver": "connect"})
with session_scope(DBKind.CLICKHOUSE, "analytics_ch") as ch:
    rows = ch.select("SELECT 1")
```

ClickHouse 分布式默认配置（自动注入 global IN/JOIN 设置）：

```
from mental1104.db import ClickHouseProfile

register_db(
    DBKind.CLICKHOUSE,
    dsn="clickhouse+http://...",
    profile=ClickHouseProfile.DISTRIBUTED,
    options={"cluster": "my_cluster"},
)
```

## Redis

每个进程注册一次，然后使用 Redis 作用域：

```
from mental1104.db import register_redis, redis_session_scope, redis_params_from_env

register_redis(name="cache", params=redis_params_from_env())
with redis_session_scope("cache") as client:
    client.set("k", "v")
```

Redis 集群：

```
from mental1104.db import RedisMode

register_redis(
    name="cache",
    params=redis_params_from_env(),
    mode=RedisMode.CLUSTER,
    options={"startup_nodes": "10.0.0.1:6379,10.0.0.2:6379"},
)
```

Redis sentinel：

```
register_redis(
    name="cache",
    params=redis_params_from_env(),
    mode=RedisMode.SENTINEL,
    options={"sentinels": "10.0.0.10:26379,10.0.0.11:26379", "service_name": "mymaster"},
)
```

## MongoDB（同步）

```
from mental1104.db import mongo_params_from_env, register_mongo, mongo_session_scope

register_mongo(params=mongo_params_from_env())
with mongo_session_scope() as mongo:
    coll = mongo.db["demo_users"]
    coll.insert_one({"name": "alice"})
    print(list(coll.find({"name": "alice"})))
```

## MongoDB（异步）

```
from mental1104.db import mongo_params_from_env, register_mongo, async_mongo_session_scope

register_mongo(params=mongo_params_from_env())
async with async_mongo_session_scope() as mongo:
    coll = mongo.db["demo_users"]
    await coll.insert_one({"name": "alice"})
    rows = [doc async for doc in coll.find({"name": "alice"})]
    print(rows)
```

## 迁移钩子

`set_migration_handler()` + `run_migrations()` 定义了 Alembic 或其他迁移工具的集成点。
