import pytest
import asyncio
import functools
from mental1104 import CoroutinePool


class TestCoroutinePool:
    @pytest.fixture
    def coroutine_pool(self):
        """为测试提供 CoroutinePool 实例，使用独立的事件循环。"""
        loop = asyncio.new_event_loop()
        pool = CoroutinePool(loop, max_concurrent_task=2)
        yield pool
        loop.close()

    def make_async_partial(self, duration, result):
        """创建一个返回协程的 functools.partial 对象。"""
        async def task():
            await asyncio.sleep(duration)
            return result
        return functools.partial(task)

    def test_single_task(self, coroutine_pool):
        """测试单个任务是否正确执行。"""
        partial_func = self.make_async_partial(1, "task1")
        result = coroutine_pool.run([partial_func])
        assert result == ["task1"]

    def test_multiple_tasks(self, coroutine_pool):
        """测试多个任务是否正确执行。"""
        partial_funcs = [
            self.make_async_partial(1, "task1"),
            self.make_async_partial(2, "task2"),
        ]
        result = coroutine_pool.run(partial_funcs)
        assert result == ["task1", "task2"]

    def test_concurrent_task_limit(self, coroutine_pool):
        """测试任务的并发限制。"""
        partial_funcs = [self.make_async_partial(1, f"task{i}") for i in range(5)]
        result = coroutine_pool.run(partial_funcs)
        assert result == [f"task{i}" for i in range(5)]

    def test_empty_task_list(self, coroutine_pool):
        """测试空任务列表是否返回空结果。"""
        result = coroutine_pool.run([])
        assert result == []

    def test_worker_execution(self, coroutine_pool):
        """测试 worker 方法是否正确处理单个任务。"""
        partial_func = self.make_async_partial(1, "worker_task")
        result = coroutine_pool.loop.run_until_complete(coroutine_pool.worker(partial_func))
        assert result == "worker_task"
