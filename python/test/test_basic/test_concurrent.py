import asyncio
import functools

import pytest

from mental1104 import CoroutinePool


class TestCoroutinePool:
    @pytest.fixture
    def coroutine_pool(self):
        """为测试提供 CoroutinePool 实例, 使用独立的事件循环。"""
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
        """
        【场景背景】基础能力：当只提交一个协程 partial 时应获取其返回值。
        【步骤输入】构造单个耗时 1s 的任务并调用 run。
        【期望输出】结果列表仅包含 "task1"。
        """
        partial_func = self.make_async_partial(1, "task1")
        result = coroutine_pool.run([partial_func])
        assert result == ["task1"]

    def test_multiple_tasks(self, coroutine_pool):
        """
        【场景背景】Pool 要能顺序返回多个任务的结果。
        【步骤输入】提交两个不同耗时的 partial。
        【期望输出】run 返回 ["task1","task2"], 保持提交顺序。
        """
        partial_funcs = [
            self.make_async_partial(1, "task1"),
            self.make_async_partial(2, "task2"),
        ]
        result = coroutine_pool.run(partial_funcs)
        assert result == ["task1", "task2"]

    def test_concurrent_task_limit(self, coroutine_pool):
        """
        【场景背景】即便 max_concurrent_task=2, 也能批量执行更多任务并收集全部结果。
        【步骤输入】提交 5 个任务。
        【期望输出】结果列表长度为 5, 顺序与提交一致。
        """
        partial_funcs = [self.make_async_partial(1, f"task{i}") for i in range(5)]
        result = coroutine_pool.run(partial_funcs)
        assert result == [f"task{i}" for i in range(5)]

    def test_empty_task_list(self, coroutine_pool):
        """
        【场景背景】空输入应返回空列表且不执行任何任务。
        【步骤输入】run([])。
        【期望输出】返回 []。
        """
        result = coroutine_pool.run([])
        assert result == []

    def test_worker_execution(self, coroutine_pool):
        """
        【场景背景】内部 worker 协程负责真正执行 partial。
        【步骤输入】直接调用 loop.run_until_complete(worker(partial))。
        【期望输出】返回 "worker_task", 说明 worker 行为与 run 一致。
        """
        partial_func = self.make_async_partial(1, "worker_task")
        result = coroutine_pool.loop.run_until_complete(coroutine_pool.worker(partial_func))
        assert result == "worker_task"
