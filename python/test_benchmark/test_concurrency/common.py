"""Shared helpers for coroutine pool benchmarks."""
from __future__ import annotations

import asyncio
import functools
from typing import Iterable, Sequence

import pytest

from mental1104.concurrency.coroutine import (
    CoroutinePool,
    GatherStrategy,
    ProcessExecutorCoroutinePool,
    ThreadExecutorCoroutinePool,
)
from mental1104.concurrency.sync_worker import (
    ProcessWorkerPool,
    ThreadWorkerPool,
)
from mental1104.utils.bench_tasks import CpuBoundTask, IoBoundTask


ASYNC_KIND = "async"
SYNC_KIND = "sync"


def _make_asyncio_pool(loop: asyncio.AbstractEventLoop, max_concurrency: int):
    return CoroutinePool(loop, max_concurrent_task=max_concurrency)


def _make_thread_pool(loop: asyncio.AbstractEventLoop, max_concurrency: int):
    return ThreadExecutorCoroutinePool(loop, max_concurrent_task=max_concurrency)


def _make_process_pool(loop: asyncio.AbstractEventLoop, max_concurrency: int):
    return ProcessExecutorCoroutinePool(loop, max_concurrent_task=max_concurrency)


def _make_plain_thread_pool(loop: asyncio.AbstractEventLoop, max_concurrency: int):
    del loop
    return ThreadWorkerPool(max_workers=max_concurrency)


def _make_plain_process_pool(loop: asyncio.AbstractEventLoop, max_concurrency: int):
    del loop
    return ProcessWorkerPool(max_workers=max_concurrency)


POOL_VARIANTS = [
    pytest.param(("asyncio", _make_asyncio_pool, ASYNC_KIND), id="asyncio"),
    pytest.param(("executor-thread", _make_thread_pool, ASYNC_KIND), id="executor-thread"),
    pytest.param(("executor-process", _make_process_pool, ASYNC_KIND), id="executor-process"),
    pytest.param(("plain-thread", _make_plain_thread_pool, SYNC_KIND), id="plain-thread"),
    pytest.param(("plain-process", _make_plain_process_pool, SYNC_KIND), id="plain-process"),
]


SCENARIO_CASES = [
    ("A) 并发缩放", 1, 1000, 1, 5, 8),
    ("A) 并发缩放", 2, 1000, 10, 5, 8),
    ("A) 并发缩放", 3, 1000, 50, 5, 8),
    ("A) 并发缩放", 4, 1000, 200, 5, 8),
    ("B) 任务规模缩放", 1, 100, 50, 1, 8),
    ("B) 任务规模缩放", 2, 1000, 50, 1, 8),
    ("B) 任务规模缩放", 3, 10000, 50, 1, 8),
    ("C) 零/微时延极限", 1, 5000, 1, 0, 8),
    ("C) 零/微时延极限", 2, 5000, 200, 0, 8),
    ("D) CPU 污染探测", 1, 2000, 1, 0, 8),
    ("D) CPU 污染探测", 2, 2000, 50, 0, 8),
    ("D) CPU 污染探测", 3, 2000, 1, 0, 100000),
    ("D) CPU 污染探测", 4, 2000, 50, 0, 100000),
    ("E) 超大并发压力", 1, 10000, 500, 1, 8),
    ("E) 超大并发压力", 2, 10000, 1000, 1, 8),
    ("F) 冷启动/微批", 1, 1, 1, 0, 8),
    ("F) 冷启动/微批", 2, 1, 50, 0, 8),
    ("F) 冷启动/微批", 3, 10, 1, 0, 8),
    ("F) 冷启动/微批", 4, 10, 50, 0, 8),
]

THROUGHPUT_IDS = [
    f"{grp.split(')')[0]}-{no:02d}|n{n}-c{c}-d{d}-p{payload}"
    for (grp, no, n, c, d, payload) in SCENARIO_CASES
]

MIXED_CASES = [
    ("G) I/O+CPU 混合", 1, 200, 10, 5, 200_000),
    ("G) I/O+CPU 混合", 2, 200, 10, 0, 1_000_000),
]

MIXED_IDS = [
    f"{grp.split(')')[0]}-{no:02d}|n{n}-c{c}-d{d}-cpu{cpu}"
    for (grp, no, n, c, d, cpu) in MIXED_CASES
]

ASYNC_HEAVY_CASES = [
    ("H) CPU + 额外异步", 1, 200, 10, 5, 100_000, 5),
    ("H) CPU + 额外异步", 2, 200, 10, 0, 200_000, 10),
]

ASYNC_HEAVY_IDS = [
    f"{grp.split(')')[0]}-{no:02d}|n{n}-c{c}-d{d}-cpu{cpu}-inner{inner}"
    for (grp, no, n, c, d, cpu, inner) in ASYNC_HEAVY_CASES
]

BLOCKING_IO_CASES = [
    ("I) 同步阻塞IO", 1, 500, 10, 5, 32),
    ("I) 同步阻塞IO", 2, 200, 50, 5, 128),
]

BLOCKING_IDS = [
    f"{grp.split(')')[0]}-{no:02d}|n{n}-c{c}-d{d}-p{payload}"
    for (grp, no, n, c, d, payload) in BLOCKING_IO_CASES
]


async def io_task(delay_ms: int, payload_len: int) -> int:
    return await IoBoundTask.io_task(delay_ms, payload_len)


def make_partial_tasks(n_tasks: int, delay_ms: int, payload_len: int):
    return [functools.partial(io_task, delay_ms, payload_len) for _ in range(n_tasks)]


async def mixed_task(delay_ms: int, cpu_iters: int) -> int:
    if delay_ms:
        await asyncio.sleep(delay_ms / 1000.0)
    return CpuBoundTask.spin(cpu_iters)


def make_mixed_tasks(n_tasks: int, delay_ms: int, cpu_iters: int):
    return [functools.partial(mixed_task, delay_ms, cpu_iters) for _ in range(n_tasks)]


async def mixed_io_cpu_task(delay_ms: int, cpu_iters: int, inner_io_ms: int) -> int:
    if delay_ms:
        await asyncio.sleep(delay_ms / 1000.0)
    await asyncio.sleep(inner_io_ms / 1000.0)
    value = CpuBoundTask.spin(cpu_iters)
    await asyncio.sleep(inner_io_ms / 1000.0)
    return value


def make_mixed_io_cpu_tasks(n_tasks: int, delay_ms: int, cpu_iters: int, inner_io_ms: int):
    return [
        functools.partial(mixed_io_cpu_task, delay_ms, cpu_iters, inner_io_ms)
        for _ in range(n_tasks)
    ]


async def blocking_io_coro(delay_ms: int, payload_len: int) -> int:
    return await asyncio.to_thread(IoBoundTask.blocking_io_task, delay_ms, payload_len)


def make_blocking_io_async_tasks(n_tasks: int, delay_ms: int, payload_len: int):
    return [functools.partial(blocking_io_coro, delay_ms, payload_len) for _ in range(n_tasks)]


def make_blocking_io_sync_tasks(n_tasks: int, delay_ms: int, payload_len: int):
    return [functools.partial(IoBoundTask.blocking_io_task, delay_ms, payload_len) for _ in range(n_tasks)]


def _run_coro_partial_sync(coro_factory: functools.partial) -> int:
    return asyncio.run(coro_factory())


def run_pool_benchmark(
    pool_variant,
    benchmark,
    *,
    scenario: str,
    case_no: int,
    n_tasks: int,
    max_concurrency: int,
    async_partials: Sequence[functools.partial] | None,
    rounds: int = 10,
    sync_partials: Sequence[functools.partial] | None = None,
):
    pool_name, pool_factory, pool_kind = pool_variant
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        pool = pool_factory(loop, max_concurrency)
        strategy = GatherStrategy()

        if case_no == 1:
            print(f"\n### {scenario} [{pool_name}] ###")

        def run_once():
            if pool_kind == ASYNC_KIND:
                assert async_partials is not None, "async pools require coroutine tasks"
                return pool.run(async_partials, strategy)

            prepared = sync_partials
            if prepared is None:
                assert async_partials is not None, "need async tasks to wrap for sync pools"
                prepared = [functools.partial(_run_coro_partial_sync, pf) for pf in async_partials]
            return pool.run(prepared)

        result = benchmark.pedantic(run_once, iterations=1, rounds=rounds)
        assert isinstance(result, list)
        assert len(result) == n_tasks
    finally:
        if hasattr(pool, "shutdown"):
            pool.shutdown()
        loop.close()
