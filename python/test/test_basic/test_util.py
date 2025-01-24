import pytest
import os
import sys
import string
from datetime import datetime
from zoneinfo import ZoneInfo
from unittest.mock import patch, AsyncMock
from mental1104.util import delay, async_delay, StringHelper, Environment, Encryption, TimeHelper, MissingEnvVarError  

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


class TestStringHelper:
    def test_replace_space_with_default_separator(self):
        input_string = "This is\n   a test\t string"
        expected_output = "This|is|a|test|string"
        assert StringHelper.replace_space_with(input_string) == expected_output

    def test_replace_space_with_custom_separator(self):
        input_string = "Another\n  example \t string"
        custom_separator = ","
        expected_output = "Another,example,string"
        assert StringHelper.replace_space_with(input_string, custom_separator) == expected_output

    def test_replace_space_with_empty_string(self):
        input_string = ""
        expected_output = ""
        assert StringHelper.replace_space_with(input_string) == expected_output

    def test_replace_space_with_no_whitespace(self):
        input_string = "NoWhitespaceHere"
        expected_output = "NoWhitespaceHere"
        assert StringHelper.replace_space_with(input_string) == expected_output

    def test_replace_space_with_only_whitespace(self):
        input_string = "\n  \t  "
        expected_output = ""
        assert StringHelper.replace_space_with(input_string) == expected_output

    def test_replace_space_with_special_characters(self):
        input_string = "Special@#Characters\nHere"
        expected_output = "Special@#Characters|Here"
        assert StringHelper.replace_space_with(input_string) == expected_output


class TestEnvironment:
    @pytest.fixture
    def mock_env(self, mocker):
        """模拟 os.environ 的 Fixture"""
        return mocker.patch.dict(os.environ, clear=True)

    def test_check_required_env_vars_all_present(self, mock_env):
        # 准备测试数据：所有环境变量存在
        required_env_vars = ["ENV_VAR_1", "ENV_VAR_2"]
        mock_env.update({var: "value" for var in required_env_vars})

        # 调用测试方法，验证没有抛出异常
        try:
            Environment.check_required_env_vars(required_env_vars)
        except MissingEnvVarError:
            pytest.fail("MissingEnvVarError raised unexpectedly")

    def test_check_required_env_vars_missing_var(self, mock_env):
        # 准备测试数据：部分环境变量缺失
        required_env_vars = ["ENV_VAR_1", "ENV_VAR_2"]
        mock_env.update({"ENV_VAR_1": "value"})  # 只设置了一个变量

        # 调用测试方法，验证是否抛出 MissingEnvVarError 异常
        with pytest.raises(MissingEnvVarError, match="Missing required environment variables: ENV_VAR_2"):
            Environment.check_required_env_vars(required_env_vars)

class TestEncryption:

    def test_encrypt_decrypt(self):
        plaintext = "This is a test message."
        key = "c1JJbLFKStaTraGF"
        salt = "c1JJbLFKStaTraGF"

        # 调用加密函数
        encrypted = Encryption.encrypt(plaintext, key=key, salt=salt)
        assert isinstance(encrypted, bytes), "加密结果应为字节串"

        # 调用解密函数
        decrypted = Encryption.decrypt(encrypted, key=key, salt=salt)
        assert decrypted == plaintext, "解密后的文本应与原始明文一致"

    def test_encrypt_with_different_salt(self):
        plaintext = "Another message"
        key = "9NpdMqxt5cI244sh"
        salt = "9NpdMqxt5cI244sh"

        # 加密解密验证
        encrypted = Encryption.encrypt(plaintext, key=key, salt=salt)
        decrypted = Encryption.decrypt(encrypted, key=key, salt=salt)
        assert decrypted == plaintext, "解密后的文本应与原始明文一致"

    def test_decrypt_invalid_key(self):
        plaintext = "Sensitive data"
        key = "Zw034vaFmNDjDMhy"
        salt = "Zw034vaFmNDjDMhy"
        wrong_key = "wrongkey"

        # 加密
        encrypted = Encryption.encrypt(plaintext, key=key, salt=salt)

        # 解密应失败
        with pytest.raises(ValueError):
            Encryption.decrypt(encrypted, key=wrong_key, salt=salt)

    def test_decrypt_invalid_salt(self):
        plaintext = "Test for invalid salt"
        key = "coO243UXvjaCgevy"
        salt = "coO243UXvjaCgevy"
        wrong_salt = "wrong_salt"

        # 加密
        encrypted = Encryption.encrypt(plaintext, key=key, salt=salt)

        # 解密应失败
        with pytest.raises(ValueError):
            Encryption.decrypt(encrypted, key=key, salt=wrong_salt)

    def test_encrypt_with_default_parameters(self):
        plaintext = "Default test"
        
        # 使用默认参数加密
        encrypted = Encryption.encrypt(plaintext)
        decrypted = Encryption.decrypt(encrypted)
        
        assert decrypted == plaintext, "使用默认参数加密解密后应与原文一致"
        
    @staticmethod
    def is_valid_salt(salt, length):
        """
        验证生成的盐是否满足以下条件：
        1. 长度正确
        2. 只包含字母和数字
        """
        valid_characters = set(string.ascii_letters + string.digits)
        return all(char in valid_characters for char in salt)

    def test_default_length_salt(self):
        length = 16
        salt = Encryption.generate_salt()
        assert self.is_valid_salt(salt, length), f"Salt '{salt}' is invalid for default length {length}"

    def test_custom_length_salt(self):
        length = 32
        salt = Encryption.generate_salt(length=length)
        assert self.is_valid_salt(salt, length), f"Salt '{salt}' is invalid for custom length {length}"

    def test_zero_length_salt(self):
        length = 0
        salt = Encryption.generate_salt(length=length)
        assert salt == "", f"Salt should be empty for length {length}, but got '{salt}'"

    def test_negative_length_salt(self):
        with pytest.raises(ValueError):
            Encryption.generate_salt(length=-1)


class TestTimeHelper:

    def test_get_current_time_default(self):
        """测试 get_current_time 方法的默认参数是否工作正常"""
        current_time = TimeHelper.get_current_time()
        expected_time = datetime.now(ZoneInfo("Asia/Shanghai")).strftime("%Y-%m-%d %H:%M:%S")
        assert current_time == expected_time, f"Expected {expected_time}, but got {current_time}"

    @pytest.mark.parametrize("format, zone, expected_format", [
        ("%Y/%m/%d", "Asia/Shanghai", "%Y/%m/%d"),
        ("%H:%M:%S", "Asia/Shanghai", "%H:%M:%S"),
        ("%Y-%m-%d %H:%M:%S", "UTC", "%Y-%m-%d %H:%M:%S"),
    ])
    def test_get_current_time_with_parameters(self, format, zone, expected_format):
        """测试 get_current_time 方法带有不同的格式和时区"""
        current_time = TimeHelper.get_current_time(format=format, zone=zone)
        expected_time = datetime.now(ZoneInfo(zone)).strftime(expected_format)
        assert current_time == expected_time, f"Expected {expected_time}, but got {current_time}"

    def test_get_current_time_invalid_zone(self):
        """测试 get_current_time 方法传入无效时区时是否正确抛出异常"""
        invalid_zone = "Invalid/Zone"
        with pytest.raises(KeyError):
            TimeHelper.get_current_time(zone=invalid_zone)
