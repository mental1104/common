import pytest
import asyncio
import time
import logging
import re
from datetime import datetime
from zoneinfo import ZoneInfo
from mental1104 import async_timed, timed, get_current_time  # 替换为实际模块名


class TestTimedDecorators:
    """
    测试 timed 和 async_timed 装饰器的测试类
    """

    @pytest.mark.asyncio
    async def test_async_timed(self, caplog):
        """
        测试 async_timed 装饰器
        """
        @async_timed
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
        @timed
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


class TestTimeHelper:

    def test_get_current_time_default(self):
        """测试 get_current_time 方法的默认参数是否工作正常"""
        current_time = get_current_time()
        expected_time = datetime.now(ZoneInfo("Asia/Shanghai")).strftime("%Y-%m-%d %H:%M:%S")
        assert current_time == expected_time, f"Expected {expected_time}, but got {current_time}"

    @pytest.mark.parametrize("format, zone, expected_format", [
        ("%Y/%m/%d", "Asia/Shanghai", "%Y/%m/%d"),
        ("%H:%M:%S", "Asia/Shanghai", "%H:%M:%S"),
        ("%Y-%m-%d %H:%M:%S", "UTC", "%Y-%m-%d %H:%M:%S"),
    ])
    def test_get_current_time_with_parameters(self, format, zone, expected_format):
        """测试 get_current_time 方法带有不同的格式和时区"""
        current_time = get_current_time(format=format, zone=zone)
        expected_time = datetime.now(ZoneInfo(zone)).strftime(expected_format)
        assert current_time == expected_time, f"Expected {expected_time}, but got {current_time}"

    def test_get_current_time_invalid_zone(self):
        """测试 get_current_time 方法传入无效时区时是否正确抛出异常"""
        invalid_zone = "Invalid/Zone"
        with pytest.raises(KeyError):
            get_current_time(zone=invalid_zone)

    def test_get_current_time_format(self):
        time_str = get_current_time()
        # 检查格式是否为 YYYY-MM-DD HH:MM:SS
        assert re.match(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}", time_str)