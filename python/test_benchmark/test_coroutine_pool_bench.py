# 文件：python/test_benchmark/test_coroutine_pool_bench.py
import asyncio
import functools
import random
import string

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


# ------------------------------------------------------------
# 基础任务与工具
# ------------------------------------------------------------
def _rand_payload(n: int) -> str:
    return "".join(random.choices(string.ascii_lowercase, k=n))


async def io_task(delay_ms: int, payload_len: int) -> int:
    await asyncio.sleep(delay_ms / 1000.0)
    return len(_rand_payload(payload_len))


def make_partial_tasks(n_tasks: int, delay_ms: int, payload_len: int):
    return [functools.partial(io_task, delay_ms, payload_len) for _ in range(n_tasks)]


def _cpu_spin(iterations: int) -> int:
    acc = 0
    for i in range(iterations):
        acc = (acc + i * i) % 1_000_000_007
    return acc


async def mixed_task(delay_ms: int, cpu_iters: int) -> int:
    if delay_ms:
        await asyncio.sleep(delay_ms / 1000.0)
    return _cpu_spin(cpu_iters)


def make_mixed_tasks(n_tasks: int, delay_ms: int, cpu_iters: int):
    return [functools.partial(mixed_task, delay_ms, cpu_iters) for _ in range(n_tasks)]


async def mixed_io_cpu_task(delay_ms: int, cpu_iters: int, inner_io_ms: int) -> int:
    if delay_ms:
        await asyncio.sleep(delay_ms / 1000.0)
    # 模拟在重 CPU 阶段前后的额外异步 I/O
    await asyncio.sleep(inner_io_ms / 1000.0)
    value = _cpu_spin(cpu_iters)
    await asyncio.sleep(inner_io_ms / 1000.0)
    return value


def make_mixed_io_cpu_tasks(n_tasks: int, delay_ms: int, cpu_iters: int, inner_io_ms: int):
    return [
        functools.partial(mixed_io_cpu_task, delay_ms, cpu_iters, inner_io_ms)
        for _ in range(n_tasks)
    ]


# ------------------------------------------------------------
# 用例矩阵（带“场景分组”与“序号”）
# 建议运行参数：
#   pytest -q --benchmark-group-by=param:scenario --benchmark-sort=name --benchmark-name=short
# ------------------------------------------------------------
SCENARIO_CASES = [
    # A) 并发缩放（固定总工作量与I/O延迟）：看吞吐随并发的收益曲线与拐点
    ("A) 并发缩放", 1, 1000,   1, 5, 8),
    ("A) 并发缩放", 2, 1000,  10, 5, 8),
    ("A) 并发缩放", 3, 1000,  50, 5, 8),
    ("A) 并发缩放", 4, 1000, 200, 5, 8),

    # B) 任务规模缩放（固定并发）：看调度开销如何随 n 增长
    ("B) 任务规模缩放", 1,   100, 50, 1, 8),
    ("B) 任务规模缩放", 2,  1000, 50, 1, 8),
    ("B) 任务规模缩放", 3, 10000, 50, 1, 8),

    # C) 零/微时延极限（纯调度）：看事件循环与池本身开销上限
    ("C) 零/微时延极限", 1, 5000,   1, 0, 8),
    ("C) 零/微时延极限", 2, 5000, 200, 0, 8),

    # D) CPU 污染探测：payload_len 放大生成开销，验证 CPU-bound 下并发无收益
    ("D) CPU 污染探测", 1, 2000,   1, 0,       8),
    ("D) CPU 污染探测", 2, 2000,  50, 0,       8),
    ("D) CPU 污染探测", 3, 2000,   1, 0,  100000),
    ("D) CPU 污染探测", 4, 2000,  50, 0,  100000),

    # E) 超大并发压力（内存占用与调度抖动）：看高并发下的稳定性
    ("E) 超大并发压力", 1, 10000,  500, 1, 8),
    ("E) 超大并发压力", 2, 10000, 1000, 1, 8),

    # F) 冷启动/微批（小 n）：看单次开销与批量门槛
    ("F) 冷启动/微批", 1,    1,   1, 0, 8),
    ("F) 冷启动/微批", 2,    1,  50, 0, 8),
    ("F) 冷启动/微批", 3,   10,   1, 0, 8),
    ("F) 冷启动/微批", 4,   10,  50, 0, 8),
]

IDS = [
    # 例：A-01|n1000-c1-d5-p8  —— 便于按 name 排序即等同输入顺序
    f"{grp.split(')')[0]}-{no:02d}|n{n}-c{c}-d{d}-p{p}"
    for (grp, no, n, c, d, p) in SCENARIO_CASES
]

# I/O + CPU 复合场景：先等 I/O，再执行 CPU 计算
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


def _make_asyncio_pool(loop, max_concurrency):
    return CoroutinePool(loop, max_concurrent_task=max_concurrency)


def _make_thread_pool(loop, max_concurrency):
    return ThreadExecutorCoroutinePool(loop, max_concurrent_task=max_concurrency)


def _make_process_pool(loop, max_concurrency):
    return ProcessExecutorCoroutinePool(loop, max_concurrent_task=max_concurrency)


def _make_plain_thread_pool(loop, max_concurrency):
    return ThreadWorkerPool(max_workers=max_concurrency)


def _make_plain_process_pool(loop, max_concurrency):
    return ProcessWorkerPool(max_workers=max_concurrency)


ASYNC_KIND = "async"
SYNC_KIND = "sync"

POOL_VARIANTS = [
    pytest.param(("asyncio", _make_asyncio_pool, ASYNC_KIND), id="asyncio"),
    pytest.param(("executor-thread", _make_thread_pool, ASYNC_KIND), id="executor-thread"),
    pytest.param(("executor-process", _make_process_pool, ASYNC_KIND), id="executor-process"),
    pytest.param(("plain-thread", _make_plain_thread_pool, SYNC_KIND), id="plain-thread"),
    pytest.param(("plain-process", _make_plain_process_pool, SYNC_KIND), id="plain-process"),
]


def _run_coro_partial_sync(coro_factory: functools.partial) -> int:
    return asyncio.run(coro_factory())


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,payload_len",
    SCENARIO_CASES,
    ids=IDS,
)
def test_coroutine_pool_bench(
    pool_variant,
    benchmark,
    scenario,
    case_no,
    n_tasks,
    max_concurrency,
    delay_ms,
    payload_len,
):
    pool_name, pool_factory, pool_kind = pool_variant
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        pool = pool_factory(loop, max_concurrency)
        partials = make_partial_tasks(n_tasks, delay_ms, payload_len)
        strategy = GatherStrategy()

        # 仅在每个场景的第一个用例打印分组标题，便于阅读测试输出日志
        if case_no == 1:
            print(f"\n### {scenario} [{pool_name}] ###")

        def run_once():
            if pool_kind == ASYNC_KIND:
                return pool.run(partials, strategy)
            sync_partials = [functools.partial(_run_coro_partial_sync, pf) for pf in partials]
            return pool.run(sync_partials)

        result = benchmark.pedantic(run_once, iterations=1, rounds=10)
        assert isinstance(result, list)
        assert len(result) == n_tasks
    finally:
        if hasattr(pool, "shutdown"):
            pool.shutdown()
        loop.close()


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,cpu_iters",
    MIXED_CASES,
    ids=MIXED_IDS,
)
def test_coroutine_pool_mixed_bench(
    pool_variant,
    benchmark,
    scenario,
    case_no,
    n_tasks,
    max_concurrency,
    delay_ms,
    cpu_iters,
):
    pool_name, pool_factory, pool_kind = pool_variant
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        pool = pool_factory(loop, max_concurrency)
        partials = make_mixed_tasks(n_tasks, delay_ms, cpu_iters)
        strategy = GatherStrategy()

        if case_no == 1:
            print(f"\n### {scenario} [{pool_name}] ###")

        def run_once():
            if pool_kind == ASYNC_KIND:
                return pool.run(partials, strategy)
            sync_partials = [functools.partial(_run_coro_partial_sync, pf) for pf in partials]
            return pool.run(sync_partials)

        result = benchmark.pedantic(run_once, iterations=1, rounds=10)
        assert isinstance(result, list)
        assert len(result) == n_tasks
    finally:
        if hasattr(pool, "shutdown"):
            pool.shutdown()
        loop.close()


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,cpu_iters,inner_io",
    ASYNC_HEAVY_CASES,
    ids=ASYNC_HEAVY_IDS,
)
def test_coroutine_pool_async_heavy_bench(
    pool_variant,
    benchmark,
    scenario,
    case_no,
    n_tasks,
    max_concurrency,
    delay_ms,
    cpu_iters,
    inner_io,
):
    pool_name, pool_factory, pool_kind = pool_variant
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        pool = pool_factory(loop, max_concurrency)
        partials = make_mixed_io_cpu_tasks(n_tasks, delay_ms, cpu_iters, inner_io)
        strategy = GatherStrategy()

        if case_no == 1:
            print(f"\n### {scenario} [{pool_name}] ###")

        def run_once():
            if pool_kind == ASYNC_KIND:
                return pool.run(partials, strategy)
            sync_partials = [functools.partial(_run_coro_partial_sync, pf) for pf in partials]
            return pool.run(sync_partials)

        result = benchmark.pedantic(run_once, iterations=1, rounds=10)
        assert isinstance(result, list)
        assert len(result) == n_tasks
    finally:
        if hasattr(pool, "shutdown"):
            pool.shutdown()
        loop.close()
