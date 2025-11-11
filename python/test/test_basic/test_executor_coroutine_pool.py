import asyncio
import functools
import time
import sys

import pytest

from mental1104.concurrency.coroutine import (
    AsCompletedStrategy,
    CoroutinePool,
    FirstSuccessfulStrategy,
    GatherStrategy,
    ProcessExecutorCoroutinePool,
    ThreadExecutorCoroutinePool,
)
from mental1104.concurrency.sync_worker import (
    ProcessWorkerPool,
    ThreadWorkerPool,
)
from mental1104.concurrency.types import MPStartMethod


def make_async_partial(result, delay=0.0):
    async def task():
        if delay:
            await asyncio.sleep(delay)
        return result
    return functools.partial(task)


def make_failing_partial(exc: BaseException, delay: float = 0.0):
    async def task():
        if delay:
            await asyncio.sleep(delay)
        raise exc
    return functools.partial(task)


def make_cancellable_partial(result, delay=0.0):
    state = {"cancelled": False}

    async def task():
        try:
            if delay:
                await asyncio.sleep(delay)
            return result
        except asyncio.CancelledError:
            state["cancelled"] = True
            raise

    return functools.partial(task), state


@pytest.fixture
def loop():
    loop = asyncio.new_event_loop()
    yield loop
    loop.close()


def test_thread_pool_requires_positive_shards(loop):
    with pytest.raises(ValueError):
        ThreadExecutorCoroutinePool(loop, n_shards=0)


def test_process_pool_requires_positive_shards(loop):
    with pytest.raises(ValueError):
        ProcessExecutorCoroutinePool(loop, n_shards=0)


def test_auto_executor_count_matches_ratio(loop):
    pool = ThreadExecutorCoroutinePool(loop, max_concurrent_task=96)
    expected = ThreadExecutorCoroutinePool.recommended_executor_count(96)
    assert pool.n_shards == expected


@pytest.mark.parametrize(
    "max_tasks,expected",
    [
        (0, 1),
        (1, 1),
        (32, 1),
        (33, 2),
        (96, 3),
        (9999, ThreadExecutorCoroutinePool._MAX_AUTO_EXECUTORS),
    ],
)
def test_recommended_executor_count_scaling(max_tasks, expected):
    assert ThreadExecutorCoroutinePool.recommended_executor_count(max_tasks) == expected


def test_load_based_shard_selection(loop):
    pool = ThreadExecutorCoroutinePool(loop, max_concurrent_task=4, n_shards=3)
    pool._shard_loads = [3, 1, 2]
    idx = pool._choose_shard(make_async_partial("x"))
    assert idx == 1


def test_custom_shard_fn_respects_bounds(loop):
    called = []

    def shard_fn(partial_func, total_shards):
        called.append(total_shards)
        return 7

    pool = ThreadExecutorCoroutinePool(loop, n_shards=4, shard_fn=shard_fn)
    assert pool._choose_shard(make_async_partial("x")) == 7 % 4
    assert called == [4]

    def negative_shard_fn(*_):
        return -3

    pool = ThreadExecutorCoroutinePool(loop, n_shards=4, shard_fn=negative_shard_fn)
    assert pool._choose_shard(make_async_partial("y")) == 0


def test_thread_executor_creation(monkeypatch, loop):
    created = []

    class DummyExecutor:
        def __init__(self, max_workers=None, thread_name_prefix=None):
            self.max_workers = max_workers
            self.thread_name_prefix = thread_name_prefix
            self.shutdown_called = False
            created.append(self)

        def shutdown(self, wait=True, cancel_futures=False):
            self.shutdown_called = True

    monkeypatch.setattr(
        "mental1104.concurrency.coroutine.ThreadPoolExecutor",
        DummyExecutor,
    )

    pool = ThreadExecutorCoroutinePool(loop, max_concurrent_task=7, n_shards=3)
    executors = pool._ensure_executors()
    assert len(executors) == 3
    assert all(ex.max_workers == 3 for ex in executors)
    assert all(ex.thread_name_prefix.startswith("coro-shard-") for ex in executors)

    pool._shutdown_executors()
    assert all(ex.shutdown_called for ex in executors)


def test_process_executor_creation_sets_start_method(monkeypatch, loop):
    calls = []

    class DummyProcessExecutor:
        def __init__(self, max_workers=None):
            self.max_workers = max_workers
            self.shutdown_called = False

        def shutdown(self, wait=True, cancel_futures=False):
            self.shutdown_called = True

    def fake_set_start_method(method, force=False):
        calls.append((method, force))

    monkeypatch.setattr(
        "mental1104.concurrency.coroutine.ProcessPoolExecutor",
        DummyProcessExecutor,
    )
    monkeypatch.setattr("multiprocessing.set_start_method", fake_set_start_method)

    pool = ProcessExecutorCoroutinePool(
        loop,
        max_concurrent_task=4,
        n_shards=2,
        mp_start_method=MPStartMethod.SPAWN,
    )
    executors = pool._ensure_executors()
    assert len(executors) == 2
    assert all(ex.max_workers == 2 for ex in executors)
    assert calls == [("spawn", False)]

    pool._shutdown_executors()
    assert all(ex.shutdown_called for ex in executors)


def test_thread_run_task_batch_executes_and_releases(loop):
    pool = ThreadExecutorCoroutinePool(loop, max_concurrent_task=4, n_shards=2)
    partials = [make_async_partial(i, delay=0.01) for i in range(4)]

    results = loop.run_until_complete(pool.run_task_batch(partials, GatherStrategy()))
    assert results == [0, 1, 2, 3]
    assert pool._executors is None
    assert pool._shard_loads == [0, 0]


def test_as_completed_strategy_orders_results(loop):
    pool = CoroutinePool(loop, max_concurrent_task=3)
    partials = [
        make_async_partial("slow", delay=0.05),
        make_async_partial("fast", delay=0.01),
        make_async_partial("mid", delay=0.03),
    ]
    observed: list[tuple[int, str]] = []

    async def on_result(idx, value):
        observed.append((idx, value))

    strategy = AsCompletedStrategy(on_result=on_result)
    results = loop.run_until_complete(pool.run_task_batch(partials, strategy))
    assert results == ["fast", "mid", "slow"]
    assert observed == [(1, "fast"), (2, "mid"), (0, "slow")]


def test_first_success_strategy_returns_fastest(loop):
    pool = CoroutinePool(loop, max_concurrent_task=3)
    slow_partial, slow_state = make_cancellable_partial("slow", delay=0.2)
    partials = [
        make_failing_partial(RuntimeError("boom"), delay=0.01),
        slow_partial,
        make_async_partial("fast", delay=0.02),
    ]
    strategy = FirstSuccessfulStrategy(cancel_pending=True)
    results = loop.run_until_complete(pool.run_task_batch(partials, strategy))
    assert results == ["fast"]
    assert slow_state["cancelled"] is True


def test_first_success_strategy_raises_when_all_fail(loop):
    pool = CoroutinePool(loop, max_concurrent_task=2)
    partials = [
        make_failing_partial(RuntimeError("a")),
        make_failing_partial(ValueError("b")),
    ]
    strategy = FirstSuccessfulStrategy()
    with pytest.raises(ExceptionGroup) as excinfo:
        loop.run_until_complete(pool.run_task_batch(partials, strategy))
    assert len(excinfo.value.exceptions) == 2


def _sync_sleep_then_value(delay, value):
    time.sleep(delay)
    return value


def _sync_square(value):
    return value * value


def test_thread_worker_pool_executes_sync_tasks():
    pool = ThreadWorkerPool(max_workers=2)
    try:
        tasks = [
            functools.partial(_sync_sleep_then_value, 0.01, i)
            for i in range(4)
        ]
        assert pool.run(tasks) == [0, 1, 2, 3]
    finally:
        pool.shutdown()


@pytest.mark.skipif(sys.platform == "win32", reason="spawn semantics differ on Windows")
def test_process_worker_pool_executes_sync_tasks():
    pool = ProcessWorkerPool(max_workers=2, mp_start_method=MPStartMethod.SPAWN)
    try:
        tasks = [functools.partial(_sync_square, i) for i in range(4)]
        assert pool.run(tasks) == [0, 1, 4, 9]
    finally:
        pool.shutdown()
