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

## Migration Hook

`set_migration_handler()` + `run_migrations()` define an integration point
for Alembic or other migration tools.
