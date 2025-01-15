import pytest
import asyncio
import time
import logging
from mental1104.timed import async_timed, timed  # 替换为实际模块名

class TestTimedDecorators:
    """
    测试 timed 和 async_timed 装饰器的测试类
    """

    @pytest.mark.asyncio
    async def test_async_timed(self, caplog):
        """
        测试 async_timed 装饰器
        """
        @async_timed()
        async def sample_async_function(x, y):
            await asyncio.sleep(1)  # 模拟异步耗时操作
            return x + y
        # 确保日志配置正确
        logging.basicConfig(level=logging.DEBUG)
        # 设置日志等级
        with caplog.at_level(logging.DEBUG):
            # 执行被装饰的异步函数
            result = await sample_async_function(2, 3)

        # 验证函数返回值
        assert result == 5

        # 验证日志内容
        logs = [record.message for record in caplog.records]
        assert any("starting" in log and "sample_async_function" in log for log in logs)
        assert any("starting" in log and "sample_async_function" in log for log in logs)

    def test_timed(self, caplog):
        """
        测试 timed 装饰器
        """
        @timed()
        def sample_function(x, y):
            time.sleep(1)  # 模拟同步耗时操作
            return x * y

        # 确保日志配置正确
        logging.basicConfig(level=logging.DEBUG)
        # 设置日志等级
        with caplog.at_level(logging.DEBUG):
            # 执行被装饰的同步函数
            result = sample_function(2, 3)

        # 验证函数返回值
        assert result == 6

        # 验证日志内容
        logs = [record.message for record in caplog.records]

        assert any("starting" in log and "sample_function" in log for log in logs)
        assert any("starting" in log and "sample_function" in log for log in logs)