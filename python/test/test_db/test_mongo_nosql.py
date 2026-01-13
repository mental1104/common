import os
import uuid

import pytest

from mental1104.db.nosql import (
    AutoMongoSessionDAO,
    mongo_params_from_env,
    mongo_session_scope,
    register_mongo,
)


class _UserDAO(AutoMongoSessionDAO):
    def __init__(self, collection: str) -> None:
        self._collection = collection

    def insert_names(self, names, *, mongo):
        coll = mongo.db[self._collection]
        docs = [{"name": name} for name in names]
        coll.insert_many(docs, session=mongo.session)
        return coll

    def list_names(self, *, mongo):
        coll = mongo.db[self._collection]
        cursor = coll.find({}, projection={"_id": 0, "name": 1}, session=mongo.session)
        return [doc["name"] for doc in cursor]


@pytest.fixture(scope="module")
def mongo_enabled():
    if not os.getenv("MONGO_HOST") or not os.getenv("MONGO_PORT"):
        pytest.skip("MONGO_HOST or MONGO_PORT is not configured")
    try:
        import pymongo  # noqa: F401
    except Exception:
        pytest.skip("pymongo is not installed")


def test_mongo_session_scope_injects_context(mongo_enabled):
    params = mongo_params_from_env()
    register_mongo(params=params, options={"serverSelectionTimeoutMS": 2000}, allow_overwrite=True)

    collection = f"demo_users_{uuid.uuid4().hex}"
    dao = _UserDAO(collection)
    names = ["alice", "bob"]

    with mongo_session_scope() as mongo:
        mongo.client.admin.command("ping")
        dao.insert_names(names)
        result = dao.list_names()
        assert sorted(result) == sorted(names)
        mongo.db.drop_collection(collection)


def test_mongo_requires_context(mongo_enabled):
    dao = _UserDAO("demo_users")
    with pytest.raises(RuntimeError):
        dao.list_names()
