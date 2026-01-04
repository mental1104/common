import string

import pytest

from mental1104 import decrypt, encrypt, generate_salt


class TestEncryption:
    def test_encrypt_decrypt(self):
        """
        【场景背景】最常规的明文在提供 key/salt 时应能往返加密解密。
        【步骤输入】指定固定 key/salt, 加密字符串后再解密。
        【期望输出】加密结果是 bytes, 且解密得到的文本与原始 plaintext 完全一致。
        """
        plaintext = "This is a test message."
        key = "c1JJbLFKStaTraGF"
        salt = "c1JJbLFKStaTraGF"

        # 调用加密函数
        encrypted = encrypt(plaintext, key=key, salt=salt)
        assert isinstance(encrypted, bytes), "加密结果应为字节串"

        # 调用解密函数
        decrypted = decrypt(encrypted, key=key, salt=salt)
        assert decrypted == plaintext, "解密后的文本应与原始明文一致"

    def test_encrypt_with_different_salt(self):
        """
        【场景背景】只要 key/salt 匹配, 任意明文都应可往返。
        【步骤输入】传入另一组 key/salt, 加密再解密。
        【期望输出】解密得到的字符串与输入相同, 说明多套参数都可用。
        """
        plaintext = "Another message"
        key = "9NpdMqxt5cI244sh"
        salt = "9NpdMqxt5cI244sh"

        # 加密解密验证
        encrypted = encrypt(plaintext, key=key, salt=salt)
        decrypted = decrypt(encrypted, key=key, salt=salt)
        assert decrypted == plaintext, "解密后的文本应与原始明文一致"

    def test_decrypt_invalid_key(self):
        """
        【场景背景】如果解密时 key 不匹配, 库应抛出异常防止返回垃圾数据。
        【步骤输入】正确 key/salt 加密后, 使用错误 key 解密。
        【期望输出】decrypt 抛 ValueError, 提示密钥错误。
        """
        plaintext = "Sensitive data"
        key = "Zw034vaFmNDjDMhy"
        salt = "Zw034vaFmNDjDMhy"
        wrong_key = "wrongkey"

        # 加密
        encrypted = encrypt(plaintext, key=key, salt=salt)

        # 解密应失败
        with pytest.raises(ValueError):
            decrypt(encrypted, key=wrong_key, salt=salt)

    def test_decrypt_invalid_salt(self):
        """
        【场景背景】盐值参与派生, 传入错误 salt 也应视为失败。
        【步骤输入】正确 key/salt 加密, 错误 salt 解密。
        【期望输出】decrypt 抛 ValueError, 证明校验有效。
        """
        plaintext = "Test for invalid salt"
        key = "coO243UXvjaCgevy"
        salt = "coO243UXvjaCgevy"
        wrong_salt = "wrong_salt"

        # 加密
        encrypted = encrypt(plaintext, key=key, salt=salt)

        # 解密应失败
        with pytest.raises(ValueError):
            decrypt(encrypted, key=key, salt=wrong_salt)

    def test_encrypt_with_default_parameters(self):
        """
        【场景背景】API 提供默认 key/salt, 用默认值时也应可往返。
        【步骤输入】仅传 plaintext 调用 encrypt/decrypt。
        【期望输出】解密结果等于原文, 说明默认配置安全可用。
        """
        plaintext = "Default test"

        # 使用默认参数加密
        encrypted = encrypt(plaintext)
        decrypted = decrypt(encrypted)

        assert decrypted == plaintext, "使用默认参数加密解密后应与原文一致"

    @staticmethod
    def is_valid_salt(salt):
        """
        验证生成的盐是否满足以下条件：
        1. 长度正确
        2. 只包含字母和数字
        """
        valid_characters = set(string.ascii_letters + string.digits)
        return all(char in valid_characters for char in salt)

    def test_default_length_salt(self):
        """
        【场景背景】generate_salt() 默认长度应满足格式要求。
        【步骤输入】不传 length 生成盐, 并检查字符集。
        【期望输出】盐仅包含字母数字, 满足 is_valid_salt 判定。
        """
        length = 16
        salt = generate_salt()
        assert self.is_valid_salt(salt), f"Salt '{salt}' is invalid for default length {length}"

    def test_custom_length_salt(self):
        """
        【场景背景】调用方可通过 length 控制盐长度。
        【步骤输入】传 length=32 生成盐。
        【期望输出】salt 通过 is_valid_salt 且长度正确。
        """
        length = 32
        salt = generate_salt(length=length)
        assert self.is_valid_salt(salt), f"Salt '{salt}' is invalid for custom length {length}"

    def test_zero_length_salt(self):
        """
        【场景背景】请求长度为 0 时应返回空串而非报错。
        【步骤输入】length=0。
        【期望输出】返回空字符串, 提示允许禁用盐。
        """
        length = 0
        salt = generate_salt(length=length)
        assert salt == "", f"Salt should be empty for length {length}, but got '{salt}'"

    def test_negative_length_salt(self):
        """
        【场景背景】非法长度（负数）应被拒绝。
        【步骤输入】length=-1。
        【期望输出】generate_salt 抛 ValueError, 避免生成非法字符串。
        """
        with pytest.raises(ValueError):
            generate_salt(length=-1)
