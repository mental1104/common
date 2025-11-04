# 文件：python/test_benchmark/test_coroutine_pool_bench.py
import asyncio
import functools
import random
import string

import pytest

from mental1104.concurrency.coroutine import CoroutinePool, GatherStrategy


def _rand_payload(n: int) -> str:
    return "".join(random.choices(string.ascii_lowercase, k=8))


async def io_task(delay_ms: int, payload_len: int) -> int:
    await asyncio.sleep(delay_ms / 1000.0)
    return len(_rand_payload(payload_len))


def make_partial_tasks(n_tasks: int, delay_ms: int, payload_len: int):
    return [functools.partial(io_task, delay_ms, payload_len) for _ in range(n_tasks)]


@pytest.mark.parametrize("n_tasks,max_concurrency,delay_ms,payload_len", [
    (100,   1,  1, 8),
    (100,   5,  1, 8),
    (100,  50,  1, 8),
    (1000,  1,  0, 4),
    (1000, 10,  0, 4),
    (1000, 50,  0, 4),
])
def test_coroutine_pool_bench(benchmark, n_tasks, max_concurrency, delay_ms, payload_len):
    loop = asyncio.new_event_loop()
    try:
        pool = CoroutinePool(loop, max_concurrent_task=max_concurrency)
        partials = make_partial_tasks(n_tasks, delay_ms, payload_len)
        strategy = GatherStrategy()

        def run_once():
            return pool.run(partials, strategy)

        result = benchmark.pedantic(run_once, iterations=1, rounds=10)
        assert isinstance(result, list)
        assert len(result) == n_tasks
    finally:
        loop.close()
