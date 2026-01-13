import os
import uuid

import pytest

from mental1104.db.nosql import (
    AutoMongoSessionDAO,
    async_mongo_session_scope,
    mongo_params_from_env,
    register_mongo,
)


class _AsyncUserDAO(AutoMongoSessionDAO):
    def __init__(self, collection: str) -> None:
        self._collection = collection

    async def insert_names(self, names, *, mongo):
        coll = mongo.db[self._collection]
        docs = [{"name": name} for name in names]
        await coll.insert_many(docs, session=mongo.session)

    async def list_names(self, *, mongo):
        coll = mongo.db[self._collection]
        cursor = coll.find({}, projection={"_id": 0, "name": 1}, session=mongo.session)
        return [doc["name"] async for doc in cursor]


@pytest.fixture(scope="module")
def mongo_async_enabled():
    if not os.getenv("MONGO_HOST") or not os.getenv("MONGO_PORT"):
        pytest.skip("MONGO_HOST or MONGO_PORT is not configured")
    try:
        import motor  # noqa: F401
    except Exception:
        pytest.skip("motor is not installed")


@pytest.mark.asyncio
async def test_async_mongo_session_scope_injects_context(mongo_async_enabled):
    params = mongo_params_from_env()
    register_mongo(params=params, options={"serverSelectionTimeoutMS": 2000}, allow_overwrite=True)

    collection = f"demo_users_async_{uuid.uuid4().hex}"
    dao = _AsyncUserDAO(collection)
    names = ["alice", "bob"]

    async with async_mongo_session_scope() as mongo:
        await mongo.client.admin.command("ping")
        await dao.insert_names(names)
        result = await dao.list_names()
        assert sorted(result) == sorted(names)
        await mongo.db.drop_collection(collection)


@pytest.mark.asyncio
async def test_async_mongo_requires_context(mongo_async_enabled):
    dao = _AsyncUserDAO("demo_users_async")
    with pytest.raises(RuntimeError):
        await dao.list_names()
