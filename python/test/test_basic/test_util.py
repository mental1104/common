import pytest
from unittest.mock import patch, AsyncMock
from mental1104 import delay, async_delay

class TestDelayFunction:
    def test_delay_positive(self):
        """
        Test delay with a positive input.
        """
        delay_time = 3
        with patch("time.sleep", return_value=None) as mock_sleep:
            result = delay(delay_time)
            assert result == delay_time, "The delay function should return the input value"
            mock_sleep.assert_called_once_with(delay_time)

    def test_delay_zero(self):
        """
        Test delay with zero input.
        """
        delay_time = 0
        with patch("time.sleep", return_value=None) as mock_sleep:
            result = delay(delay_time)
            assert result == delay_time, "The delay function should return 0 when the input is 0"
            mock_sleep.assert_called_once_with(delay_time)

    def test_delay_negative(self):
        """
        Test delay with a negative input, expecting a ValueError.
        """
        with pytest.raises(ValueError, match="Delay time cannot be negative"):
            delay(-1)

    @pytest.mark.asyncio
    async def test_async_delay_positive(self):
        """
        Test async_delay with a positive input.
        """
        delay_time = 3
        with patch("asyncio.sleep", new_callable=AsyncMock) as mock_sleep:
            result = await async_delay(delay_time)
            assert result == delay_time, "The async_delay function should return the input value"
            mock_sleep.assert_awaited_once_with(delay_time)

    @pytest.mark.asyncio
    async def test_async_delay_zero(self):
        """
        Test async_delay with zero input.
        """
        delay_time = 0
        with patch("asyncio.sleep", new_callable=AsyncMock) as mock_sleep:
            result = await async_delay(delay_time)
            assert result == delay_time, "The async_delay function should return 0 when the input is 0"
            mock_sleep.assert_awaited_once_with(delay_time)

    @pytest.mark.asyncio
    async def test_async_delay_negative(self):
        """
        Test async_delay with a negative input, expecting a ValueError.
        """
        with pytest.raises(ValueError, match="Delay time cannot be negative"):
            await async_delay(-1)
