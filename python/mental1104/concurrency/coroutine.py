'''
Date: 2025-01-24 13:55:33
Author: mental1104 mental1104@gmail.com
LastEditors: mental1104 mental1104@gmail.com
LastEditTime: 2025-11-09 00:26:39
'''
from __future__ import annotations

import asyncio
import builtins
import functools
import math
import inspect
from abc import ABC, abstractmethod
from typing import List, Callable, Awaitable, TypeVar, Optional
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor, Executor

from mental1104 import async_timed
from mental1104.concurrency.types import MPStartMethod

# Python < 3.11 compatibility for ExceptionGroup.
if hasattr(builtins, "ExceptionGroup"):
    ExceptionGroup = builtins.ExceptionGroup  # type: ignore[attr-defined]
else:
    class ExceptionGroup(Exception):
        def __init__(self, message: str, exceptions: List[BaseException]) -> None:
            super().__init__(message)
            self.exceptions = list(exceptions)

# 私有类型变量：不作为公共 API 暴露
_T = TypeVar("_T")


# ================= 抽象策略 =================
class TaskExecutionStrategy(ABC):
    """协程策略-抽象基类：并发执行并在全部完成后一并返回"""
    @abstractmethod
    async def execute(self, loop, tasks: List[Awaitable[_T]]) -> List[_T]:
        """为兼容旧签名保留 loop 参数，实现内不依赖外部 loop。"""
        raise NotImplementedError


class GatherStrategy(TaskExecutionStrategy):
    async def execute(self, loop, tasks: List[Awaitable[_T]]) -> List[_T]:
        return await asyncio.gather(*tasks, return_exceptions=False)


class AsCompletedStrategy(TaskExecutionStrategy):
    """按完成顺序收集结果，可选回调供『流式消费』。"""

    def __init__(
        self,
        *,
        on_result: Optional[Callable[[int, _T], Awaitable[None] | None]] = None,
    ) -> None:
        self._on_result = on_result

    async def _maybe_call_callback(self, idx: int, value: _T) -> None:
        if self._on_result is None:
            return
        rv = self._on_result(idx, value)
        if inspect.isawaitable(rv):
            await rv

    async def execute(self, loop, tasks: List[Awaitable[_T]]) -> List[_T]:
        if not tasks:
            return []

        async def _wrap(index: int, aw: Awaitable[_T]) -> tuple[int, _T]:
            return index, await aw

        wrapped = [asyncio.ensure_future(_wrap(idx, task)) for idx, task in enumerate(tasks)]
        results: List[_T] = []
        for fut in asyncio.as_completed(wrapped):
            idx, value = await fut
            await self._maybe_call_callback(idx, value)
            results.append(value)
        return results


class FirstSuccessfulStrategy(TaskExecutionStrategy):
    """返回第一个成功结果，可配置是否取消剩余任务。"""

    def __init__(
        self,
        *,
        cancel_pending: bool = True,
        raise_if_all_fail: bool = True,
    ) -> None:
        self.cancel_pending = cancel_pending
        self.raise_if_all_fail = raise_if_all_fail

    async def execute(self, loop, tasks: List[Awaitable[_T]]) -> List[_T]:
        if not tasks:
            return []
        pending: set[asyncio.Task[_T]] = {asyncio.ensure_future(t) for t in tasks}
        failures: List[BaseException] = []
        try:
            while pending:
                done, pending = await asyncio.wait(pending, return_when=asyncio.FIRST_COMPLETED)
                for fut in done:
                    exc = fut.exception()
                    if exc is None:
                        result = fut.result()
                        if self.cancel_pending and pending:
                            for p in pending:
                                p.cancel()
                            await asyncio.gather(*pending, return_exceptions=True)
                        return [result]
                    failures.append(exc)
            if self.raise_if_all_fail and failures:
                raise ExceptionGroup("All tasks failed", failures)
            return []
        finally:
            if pending:
                await asyncio.gather(*pending, return_exceptions=True)


# ================= 默认实现：单进程/单线程 asyncio 协程池 =================
class CoroutinePool:
    """协程池（默认行为：单进程单线程 + asyncio 并发，Semaphore 限流）

    loop: 传入的事件循环（保持签名兼容）
    max_concurrent_task: 最大并发
    """

    def __init__(self, loop, max_concurrent_task: int = 5):
        self.loop = loop
        self.max_concurrent_task = max_concurrent_task
        self._semaphore: asyncio.Semaphore | None = None  # 懒创建

    async def _ensure_sem(self) -> asyncio.Semaphore:
        if self._semaphore is None:
            self._semaphore = asyncio.Semaphore(self.max_concurrent_task)
        return self._semaphore

    async def worker(self, coro_factory: Callable[[], Awaitable[_T]]) -> _T:
        sem = await self._ensure_sem()
        async with sem:
            return await coro_factory()

    @async_timed
    async def run_task_batch(
        self,
        partial_funcs: List[functools.partial],
        strategy: TaskExecutionStrategy
    ) -> List[_T]:
        tasks: List[Awaitable[_T]] = [self.worker(pf) for pf in partial_funcs]
        return await strategy.execute(self.loop, tasks)

    def run(
        self,
        coros: List[functools.partial],
        strategy: TaskExecutionStrategy = GatherStrategy()
    ) -> List[_T]:
        if self.loop is None:
            return asyncio.run(self.run_task_batch(coros, strategy))
        try:
            asyncio.set_event_loop(self.loop)
        except RuntimeError:
            pass
        return self.loop.run_until_complete(self.run_task_batch(coros, strategy))


# ================= 扩展实现：m(协程) → n(线程/进程) =================
def _run_coro_factory_sync(coro_factory: Callable[[], Awaitable[_T]]) -> _T:
    """顶层同步桥：在工作线程/进程里开本地事件循环跑协程（进程池可 picklable）"""
    return asyncio.run(coro_factory())


class _BaseExecutorCoroutinePool(CoroutinePool, ABC):
    """公共基类：负责 m 个协程任务向 n 个执行器的有序映射（m→n）。"""

    _TASKS_PER_EXECUTOR = 32  # 每个执行器期望承担的最大协程数
    _MAX_AUTO_EXECUTORS = 4   # 自动推导时的执行器上限（保持 n 较小）

    def __init__(
        self,
        loop,
        max_concurrent_task: int = 5,
        *,
        n_shards: Optional[int] = None,
        shard_fn: Optional[Callable[[functools.partial, int], int]] = None,
    ):
        super().__init__(loop, max_concurrent_task=max_concurrent_task)
        if n_shards is None:
            n_shards = self.recommended_executor_count(max_concurrent_task)
        if n_shards < 1:
            raise ValueError("n_shards must be >= 1")

        self.n_shards = n_shards
        self.shard_fn = shard_fn
        self._executors: List[Executor] | None = None
        self._shard_loads: List[int] = [0 for _ in range(self.n_shards)]

    @abstractmethod
    def _create_executor(self, shard_idx: int, max_workers: int) -> Executor:
        """由子类创建真实执行器。"""
        raise NotImplementedError

    def _ensure_executors(self) -> List[Executor]:
        if self._executors is not None:
            return self._executors

        per_shard = max(1, math.ceil(self.max_concurrent_task / self.n_shards))
        self._executors = [
            self._create_executor(i, per_shard)
            for i in range(self.n_shards)
        ]
        self._shard_loads = [0 for _ in range(self.n_shards)]
        return self._executors

    @classmethod
    def recommended_executor_count(cls, max_concurrent_task: int) -> int:
        """根据协程池的最大并发自动估算所需的执行器数量。"""
        if max_concurrent_task <= 0:
            return 1
        estimate = math.ceil(max_concurrent_task / cls._TASKS_PER_EXECUTOR)
        return max(1, min(cls._MAX_AUTO_EXECUTORS, estimate))

    def _choose_shard(self, pf: functools.partial) -> int:
        if self.shard_fn is not None:
            idx = int(self.shard_fn(pf, self.n_shards))
            return 0 if idx < 0 else (idx % self.n_shards)
        # 默认策略：选择当前负载最小的执行器
        min_load = min(self._shard_loads)
        for idx, load in enumerate(self._shard_loads):
            if load == min_load:
                return idx
        return 0  # 理论上不会触发

    def _shutdown_executors(self) -> None:
        if self._executors is not None:
            for ex in self._executors:
                ex.shutdown(wait=True, cancel_futures=False)
            self._executors = None
        # 负载表保留结构，供下一批次复用

    @async_timed
    async def run_task_batch(
        self,
        partial_funcs: List[functools.partial],
        strategy: TaskExecutionStrategy
    ) -> List[_T]:
        loop = asyncio.get_running_loop()
        executors = self._ensure_executors()

        try:
            tasks: List[Awaitable[_T]] = []
            for pf in partial_funcs:
                shard = self._choose_shard(pf)
                ex = executors[shard]
                future = loop.run_in_executor(ex, _run_coro_factory_sync, pf)
                self._shard_loads[shard] += 1

                def _release(_fut, shard_idx=shard, self_ref=self):
                    self_ref._shard_loads[shard_idx] = max(
                        0, self_ref._shard_loads[shard_idx] - 1
                    )

                future.add_done_callback(_release)
                tasks.append(future)
            return await strategy.execute(self.loop, tasks)
        finally:
            self._shutdown_executors()


class ThreadExecutorCoroutinePool(_BaseExecutorCoroutinePool):
    """把协程工厂交给 ThreadPoolExecutor 运行，适合 I/O 密集或轻量 CPU 场景。"""

    def __init__(
        self,
        loop,
        max_concurrent_task: int = 5,
        *,
        n_shards: Optional[int] = None,
        shard_fn: Optional[Callable[[functools.partial, int], int]] = None,
        thread_name_prefix: str = "coro-shard",
    ):
        super().__init__(
            loop,
            max_concurrent_task=max_concurrent_task,
            n_shards=n_shards,
            shard_fn=shard_fn,
        )
        # 为了利于定位线程，允许自定义线程名前缀
        self._thread_name_prefix = thread_name_prefix

    def _create_executor(self, shard_idx: int, max_workers: int) -> Executor:
        # 每个分片单独起一个线程池，方便调试线程归属
        prefix = f"{self._thread_name_prefix}-{shard_idx}"
        return ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix=prefix)


class ProcessExecutorCoroutinePool(_BaseExecutorCoroutinePool):
    """把协程工厂交给 ProcessPoolExecutor 运行，适合 CPU 密集或 GIL 受限场景。"""

    def __init__(
        self,
        loop,
        max_concurrent_task: int = 5,
        *,
        n_shards: Optional[int] = None,
        shard_fn: Optional[Callable[[functools.partial, int], int]] = None,
        mp_start_method: Optional[MPStartMethod] = None,
    ):
        super().__init__(
            loop,
            max_concurrent_task=max_concurrent_task,
            n_shards=n_shards,
            shard_fn=shard_fn,
        )
        # multiprocessing 的启动方式默认沿用当前解释器设置，这里保留覆写入口
        self._mp_start_method = mp_start_method

    def _ensure_executors(self) -> List[Executor]:
        if self._mp_start_method:
            import multiprocessing as mp

            try:
                mp.set_start_method(self._mp_start_method.value, force=False)
            except RuntimeError:
                pass
        return super()._ensure_executors()

    def _create_executor(self, shard_idx: int, max_workers: int) -> Executor:  # noqa: ARG002
        # 进程池无需线程名前缀，一个分片对应一个独立的进程池实例
        return ProcessPoolExecutor(max_workers=max_workers)
