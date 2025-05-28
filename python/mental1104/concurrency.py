'''
Date: 2025-01-24 13:55:33
Author: mental1104 mental1104@gmail.com
LastEditors: mental1104 mental1104@gmail.com
LastEditTime: 2025-01-24 22:51:17
'''
import functools
import asyncio
from asyncio import Future
from abc import ABC, abstractmethod
from typing import List, Callable
from mental1104.timed import async_timed

# 策略基类
class TaskExecutionStrategy(ABC):
    """协程策略-抽象基类

    这种场景下，会并发执行所有协程，并保证所有协程执行完后一并返回
    """
    @abstractmethod
    def execute(self, loop, tasks: List[Callable[[], Future]]):
        raise NotImplementedError("Each strategy must implement the 'execute' method.")

# 策略1：所有任务一起执行，等待所有完成
class GatherStrategy(TaskExecutionStrategy):
    """协程策略-并发执行

    这种场景下，会并发执行所有协程，并保证所有协程执行完后一并返回
    """
    async def execute(self, loop, tasks: List[Callable[[], Future]]) -> List:
        return await asyncio.gather(*tasks, return_exceptions=True)

class CoroutinePool:
    """协程池类

    loop是传入的事件循环
    max_concurrent_task是最大并发任务数，默认为5。
    使用async_timed装饰器来记录任务执行时间。
    run_task_batch方法接收一个函数对象列表，依次执行。
    run方法接收一个函数对象列表和一个执行策略，默认使用GatherStrategy。
    """
    def __init__(self, loop, max_concurrent_task=5):
        self.loop = loop
        self.semaphore = asyncio.Semaphore(max_concurrent_task)

    async def worker(self, coro):
        try:
            async with self.semaphore:
                result = await coro()
                return result
        except Exception as e:
            # 处理任务内的异常
            return f"Task failed with exception: {str(e)}"

    @async_timed
    async def run_task_batch(self, partial_funcs: List[functools.partial], strategy: TaskExecutionStrategy):
        """
        接收 partial 函数对象列表，依次执行。
        """
        tasks = [self.worker(partial_func) for partial_func in partial_funcs]
        # 使用策略来执行任务
        return await strategy.execute(self.loop, tasks)

    def run(self, coros: List[functools.partial], strategy: TaskExecutionStrategy = GatherStrategy()):
        """
        运行协程任务，支持选择策略来执行。
        :param coros: 函数对象列表
        :param strategy: 执行策略，决定任务如何执行
        :return: 根据策略返回的结果
        """
        return self.loop.run_until_complete(self.run_task_batch(coros, strategy))
