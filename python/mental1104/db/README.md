# DB Framework

This package provides a unified database access layer (SQLAlchemy-first) with:

- Declarative ORM base (`Base`) and mixins (`TimestampMixin`, `SoftDeleteMixin`).
- Engine/session registry with per-process caching.
- ContextVar-driven session injection (no session params required by default).
- Explicit read/write scopes: `session_scope(DBKind.X)` for read, `tx_scope(DBKind.X)` for write.
- Async variants: `async_session_scope(DBKind.X)` / `async_tx_scope(DBKind.X)`.
- Convenience aliases: `pg_session_scope()`, `ck_tx_scope()`, etc.

## Scopes (hard rule)

- Pure read: `session_scope(DBKind.X)` or `pg_session_scope()`
- Write or mixed read/write: `tx_scope(DBKind.X)` or `pg_tx_scope()`
- ClickHouse (clickhouse-connect): `tx_scope()` is a no-op wrapper; no ACID.

## Registry

Register once per process, then call scopes by kind + db name.

```
register_db(DBKind.POSTGRES, dsn="postgresql+psycopg://...", db_name="main_pg")
```

Optional: register + create tables in one call (idempotent create_all):

```
register_db_and_create(DBKind.POSTGRES, dsn="postgresql+psycopg://...", db_name="main_pg")
```

ClickHouse (non-SQLAlchemy, clickhouse-connect):

```
register_db(DBKind.CLICKHOUSE, dsn="clickhouse://...", db_name="analytics_ch", options={"driver": "connect"})
with session_scope(DBKind.CLICKHOUSE, "analytics_ch") as ch:
    rows = ch.select("SELECT 1")
```

ClickHouse distributed defaults (auto inject global IN/JOIN settings):

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

Register once per process, then use redis scopes:

```
from mental1104.db import register_redis, redis_session_scope, redis_params_from_env

register_redis(name="cache", params=redis_params_from_env())
with redis_session_scope("cache") as client:
    client.set("k", "v")
```

Redis cluster:

```
from mental1104.db import RedisMode

register_redis(
    name="cache",
    params=redis_params_from_env(),
    mode=RedisMode.CLUSTER,
    options={"startup_nodes": "10.0.0.1:6379,10.0.0.2:6379"},
)
```

Redis sentinel:

```
register_redis(
    name="cache",
    params=redis_params_from_env(),
    mode=RedisMode.SENTINEL,
    options={"sentinels": "10.0.0.10:26379,10.0.0.11:26379", "service_name": "mymaster"},
)
```

## MongoDB (sync)

```
from mental1104.db import mongo_params_from_env, register_mongo, mongo_session_scope

register_mongo(params=mongo_params_from_env())
with mongo_session_scope() as mongo:
    coll = mongo.db["demo_users"]
    coll.insert_one({"name": "alice"})
    print(list(coll.find({"name": "alice"})))
```

## MongoDB (async)

```
from mental1104.db import mongo_params_from_env, register_mongo, async_mongo_session_scope

register_mongo(params=mongo_params_from_env())
async with async_mongo_session_scope() as mongo:
    coll = mongo.db["demo_users"]
    await coll.insert_one({"name": "alice"})
    rows = [doc async for doc in coll.find({"name": "alice"})]
    print(rows)
```

## Migration Hook

`set_migration_handler()` + `run_migrations()` define an integration point
for Alembic or other migration tools.
