from unittest.mock import AsyncMock, patch

import pytest

from mental1104 import async_delay, delay


class TestDelayFunction:
    def test_delay_positive(self):
        """
        【场景背景】同步 delay 应在输入为正数时 sleep 相应时长并返回原值。
        【步骤输入】mock time.sleep, 传 delay_time=3。
        【期望输出】函数返回 3 且 sleep 被调用一次, 验证封装正确。
        """
        delay_time = 3
        with patch("time.sleep", return_value=None) as mock_sleep:
            result = delay(delay_time)
            assert result == delay_time, "The delay function should return the input value"
            mock_sleep.assert_called_once_with(delay_time)

    def test_delay_zero(self):
        """
        【场景背景】延迟 0 时仍会调用 sleep(0), 但应立即返回。
        【步骤输入】delay_time=0。
        【期望输出】返回 0 且 mock_sleep 被调用一次, 确认零输入合法。
        """
        delay_time = 0
        with patch("time.sleep", return_value=None) as mock_sleep:
            result = delay(delay_time)
            assert result == delay_time, "The delay function should return 0 when the input is 0"
            mock_sleep.assert_called_once_with(delay_time)

    def test_delay_negative(self):
        """
        【场景背景】负延迟没有意义, api 应拒绝。
        【步骤输入】delay(-1)。
        【期望输出】抛 ValueError 并提示不允许负数。
        """
        with pytest.raises(ValueError, match="Delay time cannot be negative"):
            delay(-1)

    @pytest.mark.asyncio
    async def test_async_delay_positive(self):
        """
        【场景背景】async_delay 与同步版本类似, 只是 await asyncio.sleep。
        【步骤输入】mock asyncio.sleep 并 await async_delay(3)。
        【期望输出】函数返回 3, sleep 被 await 一次。
        """
        delay_time = 3
        with patch("asyncio.sleep", new_callable=AsyncMock) as mock_sleep:
            result = await async_delay(delay_time)
            assert result == delay_time, "The async_delay function should return the input value"
            mock_sleep.assert_awaited_once_with(delay_time)

    @pytest.mark.asyncio
    async def test_async_delay_zero(self):
        """
        【场景背景】协程版在 0 延迟时也应立即返回。
        【步骤输入】delay_time=0。
        【期望输出】返回 0 且 sleep 获得 0 参数。
        """
        delay_time = 0
        with patch("asyncio.sleep", new_callable=AsyncMock) as mock_sleep:
            result = await async_delay(delay_time)
            assert result == delay_time, (
                "The async_delay function should return 0 when the input is 0"
            )
            mock_sleep.assert_awaited_once_with(delay_time)

    @pytest.mark.asyncio
    async def test_async_delay_negative(self):
        """
        【场景背景】异步接口同样要拒绝负数。
        【步骤输入】await async_delay(-1)。
        【期望输出】抛 ValueError, 提示非法输入。
        """
        with pytest.raises(ValueError, match="Delay time cannot be negative"):
            await async_delay(-1)
