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
    """
    【场景背景】线程执行器分片数量必须为正。
    【步骤输入】n_shards=0 构造 ThreadExecutorCoroutinePool。
    【期望输出】构造抛 ValueError，防止非法配置。
    """
    with pytest.raises(ValueError):
        ThreadExecutorCoroutinePool(loop, n_shards=0)


def test_process_pool_requires_positive_shards(loop):
    """
    【场景背景】进程执行器同样需要正数分片。
    【步骤输入】n_shards=0 构造 ProcessExecutorCoroutinePool。
    【期望输出】抛 ValueError，提示配置错误。
    """
    with pytest.raises(ValueError):
        ProcessExecutorCoroutinePool(loop, n_shards=0)


def test_auto_executor_count_matches_ratio(loop):
    """
    【场景背景】未显式设置 n_shards 时，Pool 会基于任务上限自动推导。
    【步骤输入】max_concurrent_task=96 初始化线程池。
    【期望输出】实例的 n_shards 等于 recommended_executor_count(96)。
    """
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
    """
    【场景背景】recommended_executor_count 应根据 max_tasks 呈阶梯增长。
    【步骤输入】多组参数化 max_tasks。
    【期望输出】返回值与预期表一致，验证阈值逻辑。
    """
    assert ThreadExecutorCoroutinePool.recommended_executor_count(max_tasks) == expected


def test_load_based_shard_selection(loop):
    """
    【场景背景】_choose_shard 应选负载最小的分片。
    【步骤输入】手动设置 _shard_loads=[3,1,2]。
    【期望输出】返回索引 1，对应当前最轻负载。
    """
    pool = ThreadExecutorCoroutinePool(loop, max_concurrent_task=4, n_shards=3)
    pool._shard_loads = [3, 1, 2]
    idx = pool._choose_shard(make_async_partial("x"))
    assert idx == 1


def test_custom_shard_fn_respects_bounds(loop):
    """
    【场景背景】自定义 shard_fn 需支持任意整数并回退到取模结果。
    【步骤输入】定义返回 7 和 -3 的 shard_fn，并查看 _choose_shard。
    【期望输出】第一次 7%4=3；第二次负数回退为 0，同时记录调用 total_shards。
    """
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
    """
    【场景背景】线程执行器池需为每个分片惰性创建 ThreadPoolExecutor 并可安全回收。
    【步骤输入】使用 DummyExecutor 打桩，调用 _ensure_executors 和 _shutdown_executors。
    【期望输出】创建数与 n_shards 匹配，max_workers/前缀满足设计，shutdown 全部触发。
    """
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
    """
    【场景背景】进程池创建前需设置 multiprocessing start method 且能回收资源。
    【步骤输入】打桩 ProcessPoolExecutor 和 multiprocessing.set_start_method。
    【期望输出】set_start_method 收到配置的枚举值，executor 数量正确且都被 shutdown。
    """
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
    """
    【场景背景】任务批量执行结束后，线程池应释放执行器并重置 shard load。
    【步骤输入】提交 4 个短期协程、使用 GatherStrategy 收集结果。
    【期望输出】返回值是 [0,1,2,3]，执行器被清空，负载统计回到 0。
    """
    pool = ThreadExecutorCoroutinePool(loop, max_concurrent_task=4, n_shards=2)
    partials = [make_async_partial(i, delay=0.01) for i in range(4)]

    results = loop.run_until_complete(pool.run_task_batch(partials, GatherStrategy()))
    assert results == [0, 1, 2, 3]
    assert pool._executors is None
    assert pool._shard_loads == [0, 0]


def test_as_completed_strategy_orders_results(loop):
    """
    【场景背景】AsCompletedStrategy 应按完成顺序回调 on_result，并返回按完成
    顺序排序的结果列表。
    【步骤输入】提交 fast/mid/slow 三个协程，并收集 on_result 回调。
    【期望输出】results 顺序为 fast->mid->slow，且 observed 记录的索引与值对应。
    """
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
    """
    【场景背景】FirstSuccessfulStrategy 在第一个成功结果出现时应取消其余任务。
    【步骤输入】准备一个失败任务、一个慢任务、一个快任务，启用 cancel_pending。
    【期望输出】结果仅包含“fast”，并且慢任务的取消标志为 True。
    """
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
    """
    【场景背景】若所有任务都失败，策略应抛出聚合异常以便调用方处理。
    【步骤输入】提交两个抛异常的协程。
    【期望输出】run_task_batch 抛 ExceptionGroup，包含两个子异常。
    """
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
    """
    【场景背景】ThreadWorkerPool.run 应并行执行同步任务并按提交顺序返回结果。
    【步骤输入】提交 4 个 partial，每个睡眠 0.01 后返回索引。
    【期望输出】返回 [0,1,2,3]，并在 finally 中 shutdown 池子。
    """
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
    """
    【场景背景】ProcessWorkerPool.run 应在多进程下正常运算并返回结果。
    【步骤输入】提交平方函数 partial 列表。
    【期望输出】返回 [0,1,4,9]，验证多进程分发可行。
    """
    pool = ProcessWorkerPool(max_workers=2, mp_start_method=MPStartMethod.SPAWN)
    try:
        tasks = [functools.partial(_sync_square, i) for i in range(4)]
        assert pool.run(tasks) == [0, 1, 4, 9]
    finally:
        pool.shutdown()
