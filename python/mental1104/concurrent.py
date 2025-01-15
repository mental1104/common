import functools
import asyncio
from typing import List, Callable, Generator
from mental1104.timed import async_timed

# 策略基类
class TaskExecutionStrategy:
    def execute(self, loop, tasks: List[Callable[[], asyncio.Future]]):
        raise NotImplementedError("Each strategy must implement the 'execute' method.")

# 策略1：所有任务一起执行，等待所有完成
class GatherStrategy(TaskExecutionStrategy):
    async def execute(self, loop, tasks: List[Callable[[], asyncio.Future]]) -> List:
        return await asyncio.gather(*tasks, return_exceptions=True)

class CoroutinePool:
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
