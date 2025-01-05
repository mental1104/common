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
    async def run_task_batch(self, coros: List[functools.partial]):
        tasks = []
        for coro in coros:
            task = self.loop.create_task(self.worker(coro))
            tasks.append(task)
            
        result = await asyncio.gather(*task)
        return result

    def run(self, coros: List[functools.partial]):
        return self.loop.run_until_complete(self.run_task_batch(coros))
