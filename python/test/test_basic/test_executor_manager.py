from __future__ import annotations

import asyncio
import time

import pytest

from mental1104.concurrency.executors import (
    ExecutorConfig,
    ExecutorError,
    ExecutorManager,
    ExecutorRejectedError,
    ExecutorTaskError,
)


def blocking_echo(value: str, delay: float = 0.0) -> str:
    if delay:
        time.sleep(delay)
    return value


def failing_task() -> None:
    raise ValueError("boom")


def short_cpu_sum(limit: int) -> int:
    return sum(i * i for i in range(limit))


@pytest.mark.asyncio
async def test_executor_manager_runs_blocking_io_and_short_cpu_task():
    """
    【场景背景】ExecutorManager 需要分别托管阻塞 I/O 和短 CPU 任务。
    【步骤输入】提交一个线程池 I/O 函数和一个进程池 CPU 函数。
    【期望输出】两个任务都能返回业务结果, metrics 记录成功数。
    """
    manager = ExecutorManager(
        ExecutorConfig(
            io_workers=2,
            cpu_workers=1,
            io_max_inflight=2,
            cpu_max_inflight=1,
            default_io_timeout=2.0,
            default_cpu_timeout=2.0,
            default_queue_timeout=0.5,
        )
    )
    try:
        assert await manager.run_blocking_io("test.echo", blocking_echo, "ok") == "ok"
        assert await manager.run_short_cpu_task("test.sum", short_cpu_sum, 1000) == short_cpu_sum(
            1000
        )

        metrics = manager.snapshot_metrics()
        assert metrics["blocking_io"]["success_total"] == 1
        assert metrics["short_cpu_task"]["success_total"] == 1
    finally:
        await manager.shutdown()


@pytest.mark.asyncio
async def test_executor_manager_normalizes_task_errors():
    """
    【场景背景】worker 内部异常不能把底层异常类型直接泄漏成不可控状态。
    【步骤输入】提交会抛 ValueError 的阻塞 I/O 函数。
    【期望输出】调用方收到 ExecutorTaskError, metrics 记录 error_total。
    """
    manager = ExecutorManager(
        ExecutorConfig(
            io_workers=1,
            cpu_workers=1,
            io_max_inflight=1,
            cpu_max_inflight=1,
            default_io_timeout=2.0,
            default_cpu_timeout=2.0,
            default_queue_timeout=0.5,
        )
    )
    try:
        with pytest.raises(ExecutorTaskError, match="ValueError"):
            await manager.run_blocking_io("test.fail", failing_task)

        metrics = manager.snapshot_metrics()
        assert metrics["blocking_io"]["error_total"] == 1
        assert metrics["blocking_io"]["active_waiters"] == 0
    finally:
        await manager.shutdown()


@pytest.mark.asyncio
async def test_executor_manager_rejects_when_inflight_capacity_is_full():
    """
    【场景背景】执行器需要有队列等待上限, 避免请求无限堆积。
    【步骤输入】io_max_inflight=1, 先占住唯一容量, 再提交第二个任务。
    【期望输出】第二个任务在 queue_timeout 后抛 ExecutorRejectedError。
    """
    manager = ExecutorManager(
        ExecutorConfig(
            io_workers=1,
            cpu_workers=1,
            io_max_inflight=1,
            cpu_max_inflight=1,
            default_io_timeout=2.0,
            default_cpu_timeout=2.0,
            default_queue_timeout=0.02,
        )
    )
    try:
        first_task = asyncio.create_task(
            manager.run_blocking_io("test.hold", blocking_echo, "held", delay=0.15)
        )
        await asyncio.sleep(0.03)

        with pytest.raises(ExecutorRejectedError):
            await manager.run_blocking_io("test.reject", blocking_echo, "rejected")

        assert await first_task == "held"
        metrics = manager.snapshot_metrics()
        assert metrics["blocking_io"]["success_total"] == 1
        assert metrics["blocking_io"]["rejected_total"] == 1
        assert metrics["blocking_io"]["active_waiters"] == 0
    finally:
        await manager.shutdown()


@pytest.mark.asyncio
async def test_executor_manager_shutdown_prevents_new_work():
    """
    【场景背景】FastAPI lifespan 关闭后不应继续接收新任务。
    【步骤输入】先 shutdown, 再提交 blocking I/O。
    【期望输出】抛 ExecutorError。
    """
    manager = ExecutorManager(
        ExecutorConfig(
            io_workers=1,
            cpu_workers=1,
            io_max_inflight=1,
            cpu_max_inflight=1,
        )
    )
    await manager.shutdown()

    with pytest.raises(ExecutorError):
        await manager.run_blocking_io("test.after_shutdown", blocking_echo, "nope")
