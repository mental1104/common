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
        【场景背景】async_timed 装饰器应在异步函数执行前后打印日志并保持返回值。
        【步骤输入】装饰一个 sleep 1 秒、返回 x+y 的协程，开启 caplog 捕获日志。
        【期望输出】函数返回 5，日志中含有“starting sample_async_function”等关键字。
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
        【场景背景】同步 timed 装饰器应与异步版本等价，确保日志与输出一致。
        【步骤输入】装饰一个 sleep 1 秒、返回乘积的函数。
        【期望输出】结果为 6，日志包含 starting/ending sample_function 信息。
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
        """
        【场景背景】无参调用 get_current_time() 应输出上海时区当前时间。
        【步骤输入】调用函数并取当前 datetime.now(ZoneInfo("Asia/Shanghai"))。
        【期望输出】字符串格式 YYYY-MM-DD HH:MM:SS 且与期望一致。
        """
        current_time = get_current_time()
        expected_time = datetime.now(ZoneInfo("Asia/Shanghai")).strftime("%Y-%m-%d %H:%M:%S")
        assert current_time == expected_time, f"Expected {expected_time}, but got {current_time}"

    @pytest.mark.parametrize("format, zone, expected_format", [
        ("%Y/%m/%d", "Asia/Shanghai", "%Y/%m/%d"),
        ("%H:%M:%S", "Asia/Shanghai", "%H:%M:%S"),
        ("%Y-%m-%d %H:%M:%S", "UTC", "%Y-%m-%d %H:%M:%S"),
    ])
    def test_get_current_time_with_parameters(self, format, zone, expected_format):
        """
        【场景背景】调用方可自定义格式和时区。
        【步骤输入】使用参数化组合多种 format/zone。
        【期望输出】返回值与 datetime.now 对应格式完全一致。
        """
        current_time = get_current_time(format=format, zone=zone)
        expected_time = datetime.now(ZoneInfo(zone)).strftime(expected_format)
        assert current_time == expected_time, f"Expected {expected_time}, but got {current_time}"

    def test_get_current_time_invalid_zone(self):
        """
        【场景背景】传入不存在的时区标识时应抛异常。
        【步骤输入】zone="Invalid/Zone"。
        【期望输出】抛 KeyError，提醒调用方传入可解析的 IANA 名称。
        """
        invalid_zone = "Invalid/Zone"
        with pytest.raises(KeyError):
            get_current_time(zone=invalid_zone)

    def test_get_current_time_format(self):
        """
        【场景背景】默认返回格式必须始终符合 YYYY-MM-DD HH:MM:SS。
        【步骤输入】获取字符串后用正则匹配。
        【期望输出】正则命中，说明格式稳定。
        """
        time_str = get_current_time()
        # 检查格式是否为 YYYY-MM-DD HH:MM:SS
        assert re.match(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}", time_str)
