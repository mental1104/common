# Connector Tests

This directory holds connector-specific tests (e.g. Redis/Pulsar).

PostgreSQL connector tests were retired in favor of the new common/db integration
tests under `python/test/test_db/`, which cover Postgres/MySQL/SQLite/ClickHouse
using the shared SQLAlchemy client + UoW layer. Configure `PG*`, `MYSQL*`, and
`CLICKHOUSE*` variables in `.env` and run with:

```
./dev test python --filter "test_db and postgres"
```
