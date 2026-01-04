"""Benchmarks combining I/O waits with CPU work."""

from __future__ import annotations

import pytest
from test_benchmark.test_concurrency.common import (
    MIXED_CASES,
    MIXED_IDS,
    POOL_VARIANTS,
    make_mixed_tasks,
    run_pool_benchmark,
)


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,cpu_iters",
    MIXED_CASES,
    ids=MIXED_IDS,
)
def test_coroutine_pool_mixed(
    pool_variant,
    benchmark,
    scenario,
    case_no,
    n_tasks,
    max_concurrency,
    delay_ms,
    cpu_iters,
):
    async_partials = make_mixed_tasks(n_tasks, delay_ms, cpu_iters)
    run_pool_benchmark(
        pool_variant,
        benchmark,
        scenario=scenario,
        case_no=case_no,
        n_tasks=n_tasks,
        max_concurrency=max_concurrency,
        async_partials=async_partials,
    )
