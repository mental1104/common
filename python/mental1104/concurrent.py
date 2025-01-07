import functools
import asyncio
from typing import List
from mental1104.timed import async_timed


class CoroutinePool:
    def __init__(self, loop, max_concurrent_task=5):
        self.loop = loop
        self.semaphore = asyncio.Semaphore(max_concurrent_task)

    async def worker(self, coro):
        async with self.semaphore:
            result = await coro()
            return result

    @async_timed()
    async def run_task_batch(self, partial_funcs: List[functools.partial]):
        """
        接收 partial 函数对象列表，依次执行。
        """
        tasks = []
        for partial_func in partial_funcs:
            tasks.append(self.worker(partial_func))  # 将 worker 的协程加入任务列表

        if tasks:
            result = await asyncio.gather(*tasks)
            return result
        return []

    def run(self, coros: List[functools.partial]):
        return self.loop.run_until_complete(self.run_task_batch(coros))
