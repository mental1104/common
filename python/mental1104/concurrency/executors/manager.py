from __future__ import annotations

import asyncio
import json
import logging
import time
from collections.abc import Callable
from concurrent.futures import Executor, ProcessPoolExecutor, ThreadPoolExecutor
from dataclasses import dataclass
from typing import Any, TypeVar

from mental1104.concurrency.executors.config import ExecutorConfig
from mental1104.concurrency.executors.errors import (
    ExecutorError,
    ExecutorRejectedError,
    ExecutorTaskError,
    ExecutorTimeoutError,
)
from mental1104.concurrency.executors.metrics import ExecutorMetrics


_T = TypeVar("_T")

_TIMEOUT_NOTE = (
    "Timeout only means caller stopped waiting; it does not guarantee the underlying "
    "worker stopped."
)

_logger = logging.getLogger(__name__)


@dataclass(frozen=True)
class _TimedResult:
    ok: bool
    value: Any
    executor_queue_wait_ms: float
    execution_ms: float
    error_type: str | None = None
    error_message: str | None = None
    error_repr: str | None = None

    def error_text(self) -> str:
        if not self.error_type:
            return ""
        if self.error_message:
            return f"{self.error_type}: {self.error_message}"
        return self.error_repr or self.error_type


def _run_timed(
    func: Callable[..., Any],
    args: tuple[Any, ...],
    kwargs: dict[str, Any],
    submitted_ns: int,
) -> _TimedResult:
    started_ns = time.time_ns()
    start = time.perf_counter()
    try:
        result = func(*args, **kwargs)
    except Exception as exc:  # noqa: BLE001 - task failures are normalized for callers.
        execution_ms = (time.perf_counter() - start) * 1000
        executor_queue_wait_ms = (started_ns - submitted_ns) / 1_000_000
        return _TimedResult(
            ok=False,
            value=None,
            executor_queue_wait_ms=executor_queue_wait_ms,
            execution_ms=execution_ms,
            error_type=exc.__class__.__name__,
            error_message=str(exc),
            error_repr=repr(exc),
        )

    execution_ms = (time.perf_counter() - start) * 1000
    executor_queue_wait_ms = (started_ns - submitted_ns) / 1_000_000
    return _TimedResult(
        ok=True,
        value=result,
        executor_queue_wait_ms=executor_queue_wait_ms,
        execution_ms=execution_ms,
    )


class ExecutorManager:
    def __init__(self, config: ExecutorConfig | None = None) -> None:
        self.config = config or ExecutorConfig()
        self._io_executor = ThreadPoolExecutor(
            max_workers=self.config.io_workers,
            thread_name_prefix="blocking-io",
        )
        self._cpu_executor = ProcessPoolExecutor(max_workers=self.config.cpu_workers)
        self._io_semaphore = asyncio.Semaphore(self.config.io_max_inflight)
        self._cpu_semaphore = asyncio.Semaphore(self.config.cpu_max_inflight)
        self._io_metrics = ExecutorMetrics()
        self._cpu_metrics = ExecutorMetrics()
        self._closed = False

    async def run_blocking_io(
        self,
        name: str,
        func: Callable[..., _T],
        *args: Any,
        timeout: float | None = None,
        queue_timeout: float | None = None,
        **kwargs: Any,
    ) -> _T:
        return await self._run(
            kind="blocking_io",
            name=name,
            func=func,
            args=args,
            kwargs=kwargs,
            executor=self._io_executor,
            semaphore=self._io_semaphore,
            metrics=self._io_metrics,
            timeout=_resolve_timeout(timeout, self.config.default_io_timeout, "timeout"),
            queue_timeout=_resolve_timeout(
                queue_timeout,
                self.config.default_queue_timeout,
                "queue_timeout",
            ),
        )

    async def run_short_cpu_task(
        self,
        name: str,
        func: Callable[..., _T],
        *args: Any,
        timeout: float | None = None,
        queue_timeout: float | None = None,
        **kwargs: Any,
    ) -> _T:
        return await self._run(
            kind="short_cpu_task",
            name=name,
            func=func,
            args=args,
            kwargs=kwargs,
            executor=self._cpu_executor,
            semaphore=self._cpu_semaphore,
            metrics=self._cpu_metrics,
            timeout=_resolve_timeout(timeout, self.config.default_cpu_timeout, "timeout"),
            queue_timeout=_resolve_timeout(
                queue_timeout,
                self.config.default_queue_timeout,
                "queue_timeout",
            ),
        )

    def snapshot_metrics(self) -> dict[str, dict[str, int]]:
        return {
            "blocking_io": self._io_metrics.snapshot(),
            "short_cpu_task": self._cpu_metrics.snapshot(),
        }

    async def shutdown(self) -> None:
        if self._closed:
            return
        self._closed = True
        await asyncio.gather(
            asyncio.to_thread(self._io_executor.shutdown, wait=True, cancel_futures=True),
            asyncio.to_thread(self._cpu_executor.shutdown, wait=True, cancel_futures=True),
        )

    async def _run(
        self,
        *,
        kind: str,
        name: str,
        func: Callable[..., _T],
        args: tuple[Any, ...],
        kwargs: dict[str, Any],
        executor: Executor,
        semaphore: asyncio.Semaphore,
        metrics: ExecutorMetrics,
        timeout: float,
        queue_timeout: float,
    ) -> _T:
        if self._closed:
            raise ExecutorError("ExecutorManager has been shut down")

        acquired = False
        metrics.active_waiters += 1
        gate_start = time.perf_counter()
        try:
            try:
                await asyncio.wait_for(semaphore.acquire(), timeout=queue_timeout)
            except (asyncio.TimeoutError, TimeoutError) as exc:
                gate_wait_ms = (time.perf_counter() - gate_start) * 1000
                metrics.rejected_total += 1
                message = (
                    f"{kind} task {name!r} rejected after waiting {queue_timeout:.3f}s "
                    "for executor capacity"
                )
                _log_execution(
                    kind=kind,
                    name=name,
                    gate_wait_ms=gate_wait_ms,
                    executor_queue_wait_ms=None,
                    execution_ms=None,
                    timeout=False,
                    error=f"ExecutorRejectedError: {message}",
                )
                raise ExecutorRejectedError(message) from exc

            acquired = True
            gate_wait_ms = (time.perf_counter() - gate_start) * 1000
            metrics.submitted_total += 1

            loop = asyncio.get_running_loop()
            submitted_ns = time.time_ns()
            future = loop.run_in_executor(executor, _run_timed, func, args, kwargs, submitted_ns)

            try:
                timed_result = await asyncio.wait_for(future, timeout=timeout)
            except (asyncio.TimeoutError, TimeoutError) as exc:
                metrics.timeout_total += 1
                message = f"{kind} task {name!r} timed out after {timeout:.3f}s. {_TIMEOUT_NOTE}"
                _log_execution(
                    kind=kind,
                    name=name,
                    gate_wait_ms=gate_wait_ms,
                    executor_queue_wait_ms=None,
                    execution_ms=None,
                    timeout=True,
                    error=f"ExecutorTimeoutError: {message}",
                )
                raise ExecutorTimeoutError(message) from exc
            except Exception as exc:  # noqa: BLE001 - executor transport failures are normalized.
                metrics.error_total += 1
                message = f"{kind} task {name!r} failed in executor transport: {exc}"
                _log_execution(
                    kind=kind,
                    name=name,
                    gate_wait_ms=gate_wait_ms,
                    executor_queue_wait_ms=None,
                    execution_ms=None,
                    timeout=False,
                    error=f"{exc.__class__.__name__}: {exc}",
                )
                raise ExecutorTaskError(message) from exc

            if timed_result.ok:
                metrics.success_total += 1
                _log_execution(
                    kind=kind,
                    name=name,
                    gate_wait_ms=gate_wait_ms,
                    executor_queue_wait_ms=timed_result.executor_queue_wait_ms,
                    execution_ms=timed_result.execution_ms,
                    timeout=False,
                    error=None,
                )
                return timed_result.value

            metrics.error_total += 1
            error_text = timed_result.error_text()
            message = f"{kind} task {name!r} failed: {error_text}"
            _log_execution(
                kind=kind,
                name=name,
                gate_wait_ms=gate_wait_ms,
                executor_queue_wait_ms=timed_result.executor_queue_wait_ms,
                execution_ms=timed_result.execution_ms,
                timeout=False,
                error=f"ExecutorTaskError: {message}",
            )
            raise ExecutorTaskError(message)
        finally:
            if acquired:
                semaphore.release()
            metrics.active_waiters -= 1


def _resolve_timeout(value: float | None, default: float, label: str) -> float:
    resolved = default if value is None else value
    if resolved <= 0:
        raise ValueError(f"{label} must be > 0")
    return resolved


def _log_execution(
    *,
    kind: str,
    name: str,
    gate_wait_ms: float,
    executor_queue_wait_ms: float | None,
    execution_ms: float | None,
    timeout: bool,
    error: str | None,
) -> None:
    event = {
        "event_type": "executor_task",
        "kind": kind,
        "name": name,
        "gate_wait_ms": _round_ms(gate_wait_ms),
        "executor_queue_wait_ms": _round_ms(executor_queue_wait_ms),
        "execution_ms": _round_ms(execution_ms),
        "timeout": timeout,
        "error": error,
    }
    _logger.info(json.dumps(event, ensure_ascii=False, separators=(",", ":")))


def _round_ms(value: float | None) -> float | None:
    if value is None:
        return None
    return round(value, 2)
