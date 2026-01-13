from __future__ import annotations

import os
from typing import Any, Iterable, List, Mapping, Optional, Sequence, Tuple

import redis
from redis.cluster import ClusterNode, RedisCluster
from redis.sentinel import Sentinel

from .config import RedisConnParams, RedisMode


def _normalize_mode(mode: RedisMode | str) -> RedisMode:
    if isinstance(mode, RedisMode):
        return mode
    raw = str(mode).strip().lower()
    mapping = {
        "standalone": RedisMode.STANDALONE,
        "single": RedisMode.STANDALONE,
        "cluster": RedisMode.CLUSTER,
        "sentinel": RedisMode.SENTINEL,
    }
    try:
        return mapping[raw]
    except KeyError as exc:
        raise ValueError(f"unsupported redis mode: {mode}") from exc


def _parse_nodes(raw: object) -> List[Tuple[str, int]]:
    nodes: List[Tuple[str, int]] = []
    if raw is None:
        return nodes
    if isinstance(raw, str):
        items = [part.strip() for part in raw.split(",") if part.strip()]
        for item in items:
            if ":" not in item:
                raise ValueError(f"invalid redis node format: {item}")
            host, port = item.split(":", 1)
            nodes.append((host.strip(), int(port)))
        return nodes
    if isinstance(raw, dict):
        host = raw.get("host")
        port = raw.get("port")
        if host is None or port is None:
            raise ValueError(f"invalid redis node mapping: {raw}")
        return [(str(host), int(port))]
    if isinstance(raw, (list, tuple)):
        for item in raw:
            if isinstance(item, ClusterNode):
                nodes.append((item.host, int(item.port)))
            elif isinstance(item, dict):
                host = item.get("host")
                port = item.get("port")
                if host is None or port is None:
                    raise ValueError(f"invalid redis node mapping: {item}")
                nodes.append((str(host), int(port)))
            elif isinstance(item, (list, tuple)) and len(item) >= 2:
                nodes.append((str(item[0]), int(item[1])))
            elif isinstance(item, str):
                nodes.extend(_parse_nodes(item))
            else:
                raise ValueError(f"invalid redis node item: {item}")
        return nodes
    raise ValueError(f"invalid redis node spec: {raw}")


def _merge_options(params: Optional[RedisConnParams], options: Optional[Mapping[str, Any]]) -> dict:
    merged = dict(params.options or {}) if params else {}
    merged.update(dict(options or {}))
    return merged


def _resolve_startup_nodes(params: Optional[RedisConnParams], options: Mapping[str, Any]) -> List[ClusterNode]:
    raw = options.get("startup_nodes") or options.get("cluster_nodes")
    if raw is None:
        raw = os.environ.get("REDIS_CLUSTER_NODES")
    nodes = _parse_nodes(raw)
    if not nodes and params:
        nodes = [(params.host, params.port)]
    if not nodes:
        raise ValueError("redis cluster requires startup nodes")
    return [ClusterNode(host, port) for host, port in nodes]


def _resolve_sentinels(options: Mapping[str, Any]) -> List[Tuple[str, int]]:
    raw = options.get("sentinels") or options.get("sentinel_nodes")
    if raw is None:
        raw = os.environ.get("REDIS_SENTINELS")
    nodes = _parse_nodes(raw)
    if not nodes:
        raise ValueError("redis sentinel requires sentinel nodes")
    return nodes


def create_redis_client(
    *,
    params: Optional[RedisConnParams] = None,
    url: Optional[str] = None,
    mode: RedisMode | str = RedisMode.STANDALONE,
    options: Optional[Mapping[str, Any]] = None,
) -> redis.Redis:
    mode = _normalize_mode(mode)
    merged = _merge_options(params, options)
    decode_responses = bool(merged.pop("decode_responses", True))
    username = merged.pop("username", None) or (params.username if params else None)
    password = merged.pop("password", None) or (params.password if params else None)

    if mode == RedisMode.CLUSTER:
        if params and params.db not in (0, None):
            raise ValueError("redis cluster does not support db selection")
        if url:
            return RedisCluster.from_url(
                url,
                username=username,
                password=password,
                decode_responses=decode_responses,
                **merged,
            )
        startup_nodes = _resolve_startup_nodes(params, merged)
        merged.pop("db", None)
        return RedisCluster(
            startup_nodes=startup_nodes,
            username=username,
            password=password,
            decode_responses=decode_responses,
            **merged,
        )

    if mode == RedisMode.SENTINEL:
        sentinels = _resolve_sentinels(merged)
        service_name = merged.pop("service_name", None) or os.environ.get("REDIS_SENTINEL_SERVICE")
        if not service_name:
            raise ValueError("redis sentinel requires service_name")
        role = str(merged.pop("role", "master")).lower()
        sentinel_options = dict(merged.pop("sentinel_options", {}) or {})
        client_options = dict(merged.pop("client_options", {}) or {})
        if merged:
            client_options.update(merged)
        sentinel = Sentinel(
            sentinels,
            username=sentinel_options.pop("username", username),
            password=sentinel_options.pop("password", password),
            **sentinel_options,
        )
        db = params.db if params else 0
        if role == "replica":
            return sentinel.slave_for(
                service_name,
                db=db,
                username=username,
                password=password,
                decode_responses=decode_responses,
                **client_options,
            )
        return sentinel.master_for(
            service_name,
            db=db,
            username=username,
            password=password,
            decode_responses=decode_responses,
            **client_options,
        )

    if url:
        return redis.Redis.from_url(
            url,
            decode_responses=decode_responses,
            **merged,
        )
    if not params:
        raise ValueError("redis params is required when url is not provided")
    return redis.Redis(
        host=params.host,
        port=params.port,
        username=username,
        password=password,
        db=params.db,
        decode_responses=decode_responses,
        **merged,
    )
