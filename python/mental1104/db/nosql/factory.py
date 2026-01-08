from __future__ import annotations

from typing import Any, Mapping, Optional

from pymongo import MongoClient
from pymongo.uri_parser import parse_uri

from .config import MongoConnParams


def _resolve_database(params: Optional[MongoConnParams], options: Mapping[str, Any], url: Optional[str]) -> str:
    if "database" in options:
        return str(options["database"])
    if params and params.database:
        return params.database
    if url:
        parsed = parse_uri(url)
        if parsed.get("database"):
            return str(parsed["database"])
    return "test"


def create_mongo_client(
    *,
    params: Optional[MongoConnParams] = None,
    url: Optional[str] = None,
    options: Optional[Mapping[str, Any]] = None,
) -> tuple[MongoClient, str]:
    merged = dict(params.options or {}) if params else {}
    merged.update(dict(options or {}))
    database = _resolve_database(params, merged, url)
    merged.pop("database", None)

    if url:
        return MongoClient(url, **merged), database

    if params is None:
        raise ValueError("mongo params is required when url is not provided")

    return (
        MongoClient(
            host=params.host,
            port=params.port,
            username=params.user,
            password=params.password,
            **merged,
        ),
        database,
    )


def create_async_mongo_client(
    *,
    params: Optional[MongoConnParams] = None,
    url: Optional[str] = None,
    options: Optional[Mapping[str, Any]] = None,
) -> tuple[Any, str]:
    try:
        from motor.motor_asyncio import AsyncIOMotorClient
    except Exception as exc:
        raise RuntimeError("Missing dependency: motor") from exc

    merged = dict(params.options or {}) if params else {}
    merged.update(dict(options or {}))
    database = _resolve_database(params, merged, url)
    merged.pop("database", None)

    if url:
        return AsyncIOMotorClient(url, **merged), database

    if params is None:
        raise ValueError("mongo params is required when url is not provided")

    return (
        AsyncIOMotorClient(
            host=params.host,
            port=params.port,
            username=params.user,
            password=params.password,
            **merged,
        ),
        database,
    )
