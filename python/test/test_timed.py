import pytest
import asyncio
import time
from mental1104.timed import async_timed, timed  # 替换为实际模块名

class TestTimedDecorators:
    """
    测试 timed 和 async_timed 装饰器的测试类
    """

    def test_timed_decorator(self, capsys):
        """
        测试同步 timed 装饰器
        """
        @timed()
        def sync_function(x, y):
            time.sleep(1)  # 模拟耗时操作
            return x + y

        # 调用被装饰的函数
        result = sync_function(2, 3)

        # 验证函数返回值
        assert result == 5

        # 验证输出内容
        captured = capsys.readouterr()
        assert "starting" in captured.out
        assert "finished" in captured.out
        assert "second(s)" in captured.out

    @pytest.mark.asyncio
    async def test_async_timed_decorator(self, capsys):
        """
        测试异步 async_timed 装饰器
        """
        @async_timed()
        async def async_function(x, y):
            await asyncio.sleep(1)  # 模拟异步耗时操作
            return x + y

        # 调用被装饰的异步函数
        result = await async_function(2, 3)

        # 验证函数返回值
        assert result == 5

        # 验证输出内容
        captured = capsys.readouterr()
        assert "starting" in captured.out
        assert "finished" in captured.out
        assert "second(s)" in captured.out