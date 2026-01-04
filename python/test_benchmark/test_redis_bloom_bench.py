#!/usr/bin/env python3

import concurrent.futures
import os
import random
import time

import pytest

from mental1104.connector.redis_client import RedisConnection
from mental1104.connector.redis_client.redis_bloom_kv import RedisBloom

PREFIX = "test:bloom:kv"
FILTER_KEY = "test:bf:kv"
BLOOM_ERROR_RATE = float(
    os.getenv("BLOOM_BENCH_ERROR_RATE", "1e-6")
)  # 低误判率,便于观察大规模 miss 的过滤效果
BLOOM_CAPACITY = int(os.getenv("BLOOM_BENCH_CAPACITY", "1000000"))
BATCH_SIZE = int(os.getenv("BLOOM_BENCH_BATCH_SIZE", "500"))
WORKERS = int(os.getenv("BLOOM_BENCH_WORKERS", "20"))


def _clear_prefix(client, prefix: str) -> None:
    for key in client.scan_iter(f"{prefix}*"):
        client.delete(key)


def _seed_existing(client, prefix: str, count: int):
    keys = [f"{prefix}:existing:{i}" for i in range(count)]
    pipe = client.pipeline()
    for key in keys:
        pipe.set(key, "value")
    pipe.execute()
    return keys


def _build_workload(existing_keys, query_count: int, miss_ratio: float, prefix: str):
    miss_count = int(query_count * miss_ratio)
    miss_keys = [f"{prefix}:missing:{i}" for i in range(miss_count)]
    hit_count = max(query_count - miss_count, 0)
    hits = random.choices(existing_keys, k=hit_count) if hit_count else []
    workload = hits + miss_keys
    random.shuffle(workload)
    return workload, miss_count


def _benchmark_plain(client, workload):
    start = time.perf_counter()
    for key in workload:
        client.get(key)
    return time.perf_counter() - start


def _benchmark_bloom(client, bloom: RedisBloom, workload):
    skipped = 0
    start = time.perf_counter()
    for key in workload:
        if bloom.exists(key):
            client.get(key)
        else:
            skipped += 1
    return time.perf_counter() - start, skipped


def _benchmark_pipeline_plain(client, workload, batch_size: int):
    start = time.perf_counter()
    for i in range(0, len(workload), batch_size):
        batch = workload[i : i + batch_size]
        pipe = client.pipeline()
        for key in batch:
            pipe.get(key)
        pipe.execute()
    return time.perf_counter() - start


def _benchmark_pipeline_bloom(client, bloom: RedisBloom, workload, batch_size: int):
    skipped = 0
    start = time.perf_counter()
    for i in range(0, len(workload), batch_size):
        batch = workload[i : i + batch_size]
        # 1) pipeline BF.EXISTS
        pipe = client.pipeline()
        for key in batch:
            pipe.execute_command("BF.EXISTS", bloom.filter_key, key)
        flags = pipe.execute()

        # 2) pipeline GET for positives
        pipe = client.pipeline()
        for key, exists_flag in zip(batch, flags):
            if exists_flag:
                pipe.get(key)
            else:
                skipped += 1
        if pipe.command_stack:
            pipe.execute()

    return time.perf_counter() - start, skipped


def _chunk_work(workload, chunks: int):
    size = (len(workload) + chunks - 1) // chunks
    for i in range(0, len(workload), size):
        yield workload[i : i + size]


def _benchmark_concurrent_plain(client, workload, workers: int):
    start = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        list(
            executor.map(
                lambda batch: [client.get(k) for k in batch],
                _chunk_work(workload, workers),
            )
        )
    return time.perf_counter() - start


def _benchmark_concurrent_bloom(client, bloom: RedisBloom, workload, workers: int):
    skipped = 0

    def _worker(batch):
        nonlocal skipped
        local_skipped = 0
        for key in batch:
            if bloom.exists(key):
                client.get(key)
            else:
                local_skipped += 1
        return local_skipped

    start = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        for local_skipped in executor.map(_worker, _chunk_work(workload, workers)):
            skipped += local_skipped
    return time.perf_counter() - start, skipped


def _parse_cmd_calls(entry):
    if isinstance(entry, dict):
        return entry.get("calls")
    if isinstance(entry, str):
        for part in entry.split(","):
            if part.startswith("calls="):
                try:
                    return int(part.split("=", 1)[1])
                except Exception:
                    return None
    return None


def _snapshot_stats(client):
    info = client.info(section="all")
    cmdstats = info.get("commandstats") or {}
    return {
        "hits": info.get("keyspace_hits"),
        "misses": info.get("keyspace_misses"),
        "total_cmd": info.get("total_commands_processed"),
        "net_in": info.get("total_net_input_bytes"),
        "net_out": info.get("total_net_output_bytes"),
        "mem_used": info.get("used_memory"),
        "cmd_get": _parse_cmd_calls(cmdstats.get("cmdstat_get")),
        "cmd_exists": _parse_cmd_calls(cmdstats.get("cmdstat_exists")),
        "cmd_bf_exists": _parse_cmd_calls(cmdstats.get("cmdstat_bf.exists")),
        "cmd_bf_add": _parse_cmd_calls(cmdstats.get("cmdstat_bf.add")),
    }


def _stats_delta(after: dict, before: dict):
    delta = {}
    for k, v in after.items():
        b = before.get(k)
        if isinstance(v, (int, float)) and isinstance(b, (int, float)):
            delta[k] = v - b
    return delta


@pytest.fixture(scope="module")
def redis_client():
    redis_host = os.environ.get("REDIS_HOST")
    redis_port = os.environ.get("REDIS_PORT")
    if not redis_host or not redis_port:
        pytest.skip("REDIS_HOST and REDIS_PORT environment variables are not set, skipping tests")
    try:
        with RedisConnection() as client:
            _clear_prefix(client, PREFIX)
            client.delete(FILTER_KEY)
            yield client
    except Exception as e:
        pytest.skip("Cannot connect to Redis: " + str(e))


@pytest.fixture(scope="module")
def bloom_bench_data(redis_client):
    """
    Prepare shared workload and bloom filter; reused by both plain/bloom benchmarks.
    """
    existing_count = int(os.getenv("BLOOM_BENCH_EXISTING_COUNT", "100000"))
    query_count = int(os.getenv("BLOOM_BENCH_QUERY_COUNT", "500000"))
    miss_ratio = float(
        os.getenv("BLOOM_BENCH_MISS_RATIO", "0.999")
    )  # 更偏向 miss,放大 Bloom 的作用
    seed = int(os.getenv("BLOOM_BENCH_SEED", "42"))
    random.seed(seed)

    existing_keys = _seed_existing(redis_client, PREFIX, existing_count)
    bloom = RedisBloom(
        redis_client,
        filter_key=FILTER_KEY,
        error_rate=BLOOM_ERROR_RATE,
        capacity=BLOOM_CAPACITY,
    )
    if bloom.enabled:
        for key in existing_keys:
            bloom.add(key)

    workload, miss_count = _build_workload(existing_keys, query_count, miss_ratio, PREFIX)

    return {
        "workload": workload,
        "miss_count": miss_count,
        "total": len(workload),
        "bloom": bloom,
        "bloom_enabled": bloom.enabled,
        "miss_ratio": miss_ratio,
        "existing_count": existing_count,
        "query_count": query_count,
        "seed": seed,
    }


class TestRedisBloomPerformance:
    """
    纯性能基准:对比 miss-heavy 场景下裸 GET 与 Bloom 过滤的表现。
    """

    @pytest.mark.benchmark(group="redis_bloom_miss")
    @pytest.mark.parametrize("path", ["plain_get", "bloom_guard"])
    def test_bloom_vs_plain(self, path, redis_client, benchmark, bloom_bench_data):
        workload = bloom_bench_data["workload"]
        miss_ratio = bloom_bench_data["miss_ratio"]
        total = bloom_bench_data["total"]

        if path == "bloom_guard" and not bloom_bench_data["bloom_enabled"]:
            pytest.skip("Redis Bloom module is not loaded; skipping Bloom benchmark")

        def run():
            if path == "plain_get":
                return _benchmark_plain(redis_client, workload)
            elapsed, skipped = _benchmark_bloom(redis_client, bloom_bench_data["bloom"], workload)
            return elapsed, skipped

        before = _snapshot_stats(redis_client)
        result = benchmark.pedantic(run, iterations=1, rounds=3)
        after = _snapshot_stats(redis_client)
        delta = _stats_delta(after, before)

        if path == "plain_get":
            elapsed = result
            qps = int(total / elapsed)
            benchmark.extra_info.update(
                {
                    "workload_total": total,
                    "miss_ratio": miss_ratio,
                    "error_rate": BLOOM_ERROR_RATE,
                    "capacity": BLOOM_CAPACITY,
                    "existing_count": bloom_bench_data["existing_count"],
                    "query_count": bloom_bench_data["query_count"],
                    "seed": bloom_bench_data["seed"],
                    "path": path,
                    "elapsed_s": elapsed,
                    "qps": qps,
                    "stats_delta": delta,
                }
            )
            print(
                f"[plain_get] total={total:,}, miss_ratio={miss_ratio:.2f}, time={elapsed:.3f}s, qps≈{qps:,}, "
                f"cmd_delta={delta}"
            )
        else:
            elapsed, skipped = result
            qps = int(total / elapsed)
            benchmark.extra_info.update(
                {
                    "workload_total": total,
                    "miss_ratio": miss_ratio,
                    "error_rate": BLOOM_ERROR_RATE,
                    "capacity": BLOOM_CAPACITY,
                    "existing_count": bloom_bench_data["existing_count"],
                    "query_count": bloom_bench_data["query_count"],
                    "seed": bloom_bench_data["seed"],
                    "path": path,
                    "elapsed_s": elapsed,
                    "qps": qps,
                    "skipped_gets": skipped,
                    "skip_pct": skipped / total if total else 0,
                    "stats_delta": delta,
                }
            )
            print(
                f"[bloom_guard] total={total:,}, miss_ratio={miss_ratio:.2f}, time={elapsed:.3f}s, "
                f"qps≈{qps:,}, skipped={skipped:,} ({skipped / total:.1%}), cmd_delta={delta}"
            )


class TestRedisBloomPipeline:
    """
    Pipeline 场景:将 BF.EXISTS / GET 批量发送,观察往返减少后的效果。
    """

    @pytest.mark.benchmark(group="redis_bloom_pipeline")
    @pytest.mark.parametrize("path", ["plain_get_pipeline", "bloom_guard_pipeline"])
    def test_bloom_pipeline(self, path, redis_client, benchmark, bloom_bench_data):
        workload = bloom_bench_data["workload"]
        miss_ratio = bloom_bench_data["miss_ratio"]
        total = bloom_bench_data["total"]
        batch_size = BATCH_SIZE

        if path == "bloom_guard_pipeline" and not bloom_bench_data["bloom_enabled"]:
            pytest.skip("Redis Bloom module is not loaded; skipping Bloom benchmark")

        def run():
            if path == "plain_get_pipeline":
                return _benchmark_pipeline_plain(redis_client, workload, batch_size)
            elapsed, skipped = _benchmark_pipeline_bloom(
                redis_client, bloom_bench_data["bloom"], workload, batch_size
            )
            return elapsed, skipped

        before = _snapshot_stats(redis_client)
        result = benchmark.pedantic(run, iterations=1, rounds=3)
        after = _snapshot_stats(redis_client)
        delta = _stats_delta(after, before)

        if path == "plain_get_pipeline":
            elapsed = result
            qps = int(total / elapsed)
            benchmark.extra_info.update(
                {
                    "workload_total": total,
                    "miss_ratio": miss_ratio,
                    "error_rate": BLOOM_ERROR_RATE,
                    "capacity": BLOOM_CAPACITY,
                    "existing_count": bloom_bench_data["existing_count"],
                    "query_count": bloom_bench_data["query_count"],
                    "seed": bloom_bench_data["seed"],
                    "batch_size": batch_size,
                    "path": path,
                    "elapsed_s": elapsed,
                    "qps": qps,
                    "stats_delta": delta,
                }
            )
            print(
                f"[plain_get_pipeline] total={total:,}, miss_ratio={miss_ratio:.2f}, time={elapsed:.3f}s, "
                f"qps≈{qps:,}, batch={batch_size}, cmd_delta={delta}"
            )
        else:
            elapsed, skipped = result
            qps = int(total / elapsed)
            benchmark.extra_info.update(
                {
                    "workload_total": total,
                    "miss_ratio": miss_ratio,
                    "error_rate": BLOOM_ERROR_RATE,
                    "capacity": BLOOM_CAPACITY,
                    "existing_count": bloom_bench_data["existing_count"],
                    "query_count": bloom_bench_data["query_count"],
                    "seed": bloom_bench_data["seed"],
                    "batch_size": batch_size,
                    "path": path,
                    "elapsed_s": elapsed,
                    "qps": qps,
                    "skipped_gets": skipped,
                    "skip_pct": skipped / total if total else 0,
                    "stats_delta": delta,
                }
            )
            print(
                f"[bloom_guard_pipeline] total={total:,}, miss_ratio={miss_ratio:.2f}, time={elapsed:.3f}s, "
                f"qps≈{qps:,}, skipped={skipped:,} ({skipped / total:.1%}), batch={batch_size}, cmd_delta={delta}"
            )


class TestRedisBloomConcurrent:
    """
    多线程并发场景:模拟高并发访问,观察 Bloom 对 GET 数量/吞吐的影响。
    """

    @pytest.mark.benchmark(group="redis_bloom_concurrent")
    @pytest.mark.parametrize("path", ["plain_get_concurrent", "bloom_guard_concurrent"])
    def test_bloom_concurrent(self, path, redis_client, benchmark, bloom_bench_data):
        workload = bloom_bench_data["workload"]
        miss_ratio = bloom_bench_data["miss_ratio"]
        total = bloom_bench_data["total"]
        workers = WORKERS

        if path == "bloom_guard_concurrent" and not bloom_bench_data["bloom_enabled"]:
            pytest.skip("Redis Bloom module is not loaded; skipping Bloom benchmark")

        def run():
            if path == "plain_get_concurrent":
                return _benchmark_concurrent_plain(redis_client, workload, workers)
            elapsed, skipped = _benchmark_concurrent_bloom(
                redis_client, bloom_bench_data["bloom"], workload, workers
            )
            return elapsed, skipped

        before = _snapshot_stats(redis_client)
        result = benchmark.pedantic(run, iterations=1, rounds=3)
        after = _snapshot_stats(redis_client)
        delta = _stats_delta(after, before)

        if path == "plain_get_concurrent":
            elapsed = result
            qps = int(total / elapsed)
            benchmark.extra_info.update(
                {
                    "workload_total": total,
                    "miss_ratio": miss_ratio,
                    "error_rate": BLOOM_ERROR_RATE,
                    "capacity": BLOOM_CAPACITY,
                    "existing_count": bloom_bench_data["existing_count"],
                    "query_count": bloom_bench_data["query_count"],
                    "seed": bloom_bench_data["seed"],
                    "workers": workers,
                    "path": path,
                    "elapsed_s": elapsed,
                    "qps": qps,
                    "stats_delta": delta,
                }
            )
            print(
                f"[plain_get_concurrent] total={total:,}, miss_ratio={miss_ratio:.2f}, time={elapsed:.3f}s, "
                f"qps≈{qps:,}, workers={workers}, cmd_delta={delta}"
            )
        else:
            elapsed, skipped = result
            qps = int(total / elapsed)
            benchmark.extra_info.update(
                {
                    "workload_total": total,
                    "miss_ratio": miss_ratio,
                    "error_rate": BLOOM_ERROR_RATE,
                    "capacity": BLOOM_CAPACITY,
                    "existing_count": bloom_bench_data["existing_count"],
                    "query_count": bloom_bench_data["query_count"],
                    "seed": bloom_bench_data["seed"],
                    "workers": workers,
                    "path": path,
                    "elapsed_s": elapsed,
                    "qps": qps,
                    "skipped_gets": skipped,
                    "skip_pct": skipped / total if total else 0,
                    "stats_delta": delta,
                }
            )
            print(
                f"[bloom_guard_concurrent] total={total:,}, miss_ratio={miss_ratio:.2f}, time={elapsed:.3f}s, "
                f"qps≈{qps:,}, skipped={skipped:,} ({skipped / total:.1%}), workers={workers}, cmd_delta={delta}"
            )
