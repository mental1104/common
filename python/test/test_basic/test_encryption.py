import pytest
import string

from mental1104 import encrypt, decrypt, generate_salt


class TestEncryption:

    def test_encrypt_decrypt(self):
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
        plaintext = "Another message"
        key = "9NpdMqxt5cI244sh"
        salt = "9NpdMqxt5cI244sh"

        # 加密解密验证
        encrypted = encrypt(plaintext, key=key, salt=salt)
        decrypted = decrypt(encrypted, key=key, salt=salt)
        assert decrypted == plaintext, "解密后的文本应与原始明文一致"

    def test_decrypt_invalid_key(self):
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
        length = 16
        salt = generate_salt()
        assert self.is_valid_salt(salt), f"Salt '{salt}' is invalid for default length {length}"

    def test_custom_length_salt(self):
        length = 32
        salt = generate_salt(length=length)
        assert self.is_valid_salt(salt), f"Salt '{salt}' is invalid for custom length {length}"

    def test_zero_length_salt(self):
        length = 0
        salt = generate_salt(length=length)
        assert salt == "", f"Salt should be empty for length {length}, but got '{salt}'"

    def test_negative_length_salt(self):
        with pytest.raises(ValueError):
            generate_salt(length=-1)
