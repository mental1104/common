"""Baseline throughput and scaling benchmarks for coroutine pools."""
from __future__ import annotations

import pytest

from test_benchmark.test_concurrency.common import (
    POOL_VARIANTS,
    SCENARIO_CASES,
    THROUGHPUT_IDS,
    make_partial_tasks,
    run_pool_benchmark,
)


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,payload_len",
    SCENARIO_CASES,
    ids=THROUGHPUT_IDS,
)
def test_coroutine_pool_throughput(
    pool_variant,
    benchmark,
    scenario,
    case_no,
    n_tasks,
    max_concurrency,
    delay_ms,
    payload_len,
):
    async_partials = make_partial_tasks(n_tasks, delay_ms, payload_len)
    run_pool_benchmark(
        pool_variant,
        benchmark,
        scenario=scenario,
        case_no=case_no,
        n_tasks=n_tasks,
        max_concurrency=max_concurrency,
        async_partials=async_partials,
    )
