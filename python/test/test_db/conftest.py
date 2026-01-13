import pathlib
import sys

import pytest
import pytest_asyncio

TEST_DB_ROOT = pathlib.Path(__file__).resolve().parent
if str(TEST_DB_ROOT) not in sys.path:
    sys.path.insert(0, str(TEST_DB_ROOT))

from app.dao.async_user_dao import AsyncUserDAO
from app.dao.user_dao import UserDAO
from app.models.user import User  # noqa: F401
from mental1104.db import (
    AsyncUnitOfWork,
    Base,
    ConnParams,
    DBKind,
    UnitOfWork,
    create_async_sqlalchemy_client,
    create_sqlalchemy_client,
)


@pytest.fixture
def sync_client(tmp_path):
    db_path = tmp_path / "sync.db"
    client = create_sqlalchemy_client(DBKind.SQLITE, ConnParams(ip=str(db_path)))
    Base.metadata.create_all(client.engine)
    yield client
    client.close()


@pytest.fixture
def uow(sync_client):
    return UnitOfWork(sync_client)


@pytest.fixture
def user_dao():
    return UserDAO()


@pytest_asyncio.fixture
async def async_client(tmp_path):
    db_path = tmp_path / "async.db"
    client = create_async_sqlalchemy_client(DBKind.SQLITE, ConnParams(ip=str(db_path)))
    async with client.engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield client
    await client.close()


@pytest_asyncio.fixture
async def async_uow(async_client):
    return AsyncUnitOfWork(async_client)


@pytest.fixture
def async_user_dao():
    return AsyncUserDAO()
