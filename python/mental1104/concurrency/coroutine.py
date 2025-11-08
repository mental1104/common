'''
Date: 2025-01-24 13:55:33
Author: mental1104 mental1104@gmail.com
LastEditors: mental1104 mental1104@gmail.com
LastEditTime: 2025-11-09 00:26:39
'''
from __future__ import annotations

import functools
import asyncio
from abc import ABC, abstractmethod
from typing import List, Callable, Awaitable, TypeVar
from mental1104 import async_timed

T = TypeVar("T")

# 策略基类
class TaskExecutionStrategy(ABC):
    """协程策略-抽象基类：并发执行并在全部完成后一并返回"""
    @abstractmethod
    async def execute(self, loop, tasks: List[Awaitable[T]]) -> List[T]:
        """为兼容旧签名保留 loop 参数，实现内不依赖外部 loop。"""
        raise NotImplementedError

# 策略1：所有任务一起执行，等待所有完成
class GatherStrategy(TaskExecutionStrategy):
    async def execute(self, loop, tasks: List[Awaitable[T]]) -> List[T]:
        # 直接 gather 协程对象；在当前 running loop 中调度
        return await asyncio.gather(*tasks, return_exceptions=False)

class CoroutinePool:
    """协程池

    loop: 传入的事件循环（保持签名兼容）
    max_concurrent_task: 最大并发
    """

    def __init__(self, loop, max_concurrent_task: int = 5):
        self.loop = loop
        self.max_concurrent_task = max_concurrent_task
        self._semaphore: asyncio.Semaphore | None = None  # 懒创建，避免在无 current loop 的线程里初始化

    async def _ensure_sem(self) -> asyncio.Semaphore:
        if self._semaphore is None:
            # 在协程上下文中创建，自动绑定当前 running loop
            self._semaphore = asyncio.Semaphore(self.max_concurrent_task)
        return self._semaphore

    async def worker(self, coro_factory: Callable[[], Awaitable[T]]) -> T:
        sem = await self._ensure_sem()
        async with sem:
            return await coro_factory()

    @async_timed
    async def run_task_batch(self, partial_funcs: List[functools.partial], strategy: TaskExecutionStrategy) -> List[T]:
        tasks: List[Awaitable[T]] = [self.worker(partial_func) for partial_func in partial_funcs]
        return await strategy.execute(self.loop, tasks)

    def run(self, coros: List[functools.partial], strategy: TaskExecutionStrategy = GatherStrategy()) -> List[T]:
        # 优先使用传入的 loop，保持与测试夹具一致；必要时临时设置为当前 loop
        if self.loop is None:
            return asyncio.run(self.run_task_batch(coros, strategy))
        try:
            asyncio.set_event_loop(self.loop)
        except RuntimeError:
            pass
        return self.loop.run_until_complete(self.run_task_batch(coros, strategy))