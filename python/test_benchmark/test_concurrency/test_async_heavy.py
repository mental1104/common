"""Benchmarks with heavy CPU work wrapped by additional async I/O phases."""
from __future__ import annotations

import pytest

from test_benchmark.test_concurrency.common import (
    ASYNC_HEAVY_CASES,
    ASYNC_HEAVY_IDS,
    POOL_VARIANTS,
    make_mixed_io_cpu_tasks,
    run_pool_benchmark,
)


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,cpu_iters,inner_io",
    ASYNC_HEAVY_CASES,
    ids=ASYNC_HEAVY_IDS,
)
def test_coroutine_pool_async_heavy(
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
    async_partials = make_mixed_io_cpu_tasks(n_tasks, delay_ms, cpu_iters, inner_io)
    run_pool_benchmark(
        pool_variant,
        benchmark,
        scenario=scenario,
        case_no=case_no,
        n_tasks=n_tasks,
        max_concurrency=max_concurrency,
        async_partials=async_partials,
    )
