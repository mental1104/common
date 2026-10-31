"""Bounded executors for blocking I/O and short CPU tasks.

FastAPI integration example:

    from contextlib import asynccontextmanager

    from fastapi import FastAPI, Request

    from mental1104.concurrency.executors import ExecutorManager

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.executors = ExecutorManager()
        try:
            yield
        finally:
            await app.state.executors.shutdown()

    def get_executors(request: Request) -> ExecutorManager:
        return request.app.state.executors

Business usage example:

    result = await executors.run_blocking_io(
        "legacy.fetch_user",
        legacy_fetch_user,
        user_id,
        timeout=3,
    )

    result = await executors.run_short_cpu_task(
        "risk.calculate_score",
        calculate_score,
        payload,
        timeout=2,
    )

Boundaries:

    run_blocking_io fits blocking SDKs, blocking file I/O, blocking network I/O,
    and legacy synchronous libraries. It does not fit pure Python CPU loops,
    long batch jobs, or dangerous network calls without their own low-level
    timeout.

    run_short_cpu_task fits short pure functions, small data transforms, rule
    matching, scoring, and serializable inputs/outputs. It does not fit long or
    unpredictable loops, tasks that may hang, tasks needing forced cancellation,
    DB/Redis/HTTP clients, request/session/logger/connection objects, large
    object transfers, exports, reports, or bulk scans.
"""

from __future__ import annotations

from mental1104.concurrency.executors.config import ExecutorConfig
from mental1104.concurrency.executors.errors import (
    ExecutorError,
    ExecutorRejectedError,
    ExecutorTaskError,
    ExecutorTimeoutError,
)
from mental1104.concurrency.executors.manager import ExecutorManager
from mental1104.concurrency.executors.metrics import ExecutorMetrics

__all__ = [
    "ExecutorConfig",
    "ExecutorError",
    "ExecutorManager",
    "ExecutorMetrics",
    "ExecutorRejectedError",
    "ExecutorTaskError",
    "ExecutorTimeoutError",
]
