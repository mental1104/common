from __future__ import annotations

from mental1104.db import (
    AsyncUnitOfWork,
    Base,
    ConnParams,
    DBKind,
    UnitOfWork,
    create_async_sqlalchemy_client,
    create_sqlalchemy_client,
)

# Ensure models are imported so metadata is populated.
from app.models.user import User  # noqa: F401


def build_app_sqlite(path: str):
    client = create_sqlalchemy_client(DBKind.SQLITE, ConnParams(ip=path))
    Base.metadata.create_all(client.engine)
    uow = UnitOfWork(client)
    return client, uow


async def build_app_sqlite_async(path: str):
    client = create_async_sqlalchemy_client(DBKind.SQLITE, ConnParams(ip=path))
    async with client.engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    uow = AsyncUnitOfWork(client)
    return client, uow
