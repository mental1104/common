"""Benchmarks covering synchronous blocking I/O workloads."""
from __future__ import annotations

import pytest

from test_benchmark.test_concurrency.common import (
    BLOCKING_IDS,
    BLOCKING_IO_CASES,
    POOL_VARIANTS,
    make_blocking_io_async_tasks,
    make_blocking_io_sync_tasks,
    run_pool_benchmark,
)


@pytest.mark.parametrize("pool_variant", POOL_VARIANTS)
@pytest.mark.parametrize(
    "scenario,case_no,n_tasks,max_concurrency,delay_ms,payload_len",
    BLOCKING_IO_CASES,
    ids=BLOCKING_IDS,
)
def test_coroutine_pool_blocking_io(
    pool_variant,
    benchmark,
    scenario,
    case_no,
    n_tasks,
    max_concurrency,
    delay_ms,
    payload_len,
):
    async_partials = make_blocking_io_async_tasks(n_tasks, delay_ms, payload_len)
    sync_partials = make_blocking_io_sync_tasks(n_tasks, delay_ms, payload_len)
    run_pool_benchmark(
        pool_variant,
        benchmark,
        scenario=scenario,
        case_no=case_no,
        n_tasks=n_tasks,
        max_concurrency=max_concurrency,
        async_partials=async_partials,
        sync_partials=sync_partials,
    )
