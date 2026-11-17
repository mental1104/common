import base64
import string

import pytest

from mental1104 import CipherAlgorithm, decrypt, encrypt, generate_salt


class TestEncryption:
    """验证显式 CBC IV、历史解密和 GCM 认证的公共契约。"""

    @pytest.mark.parametrize("algorithm", list(CipherAlgorithm))
    def test_encrypt_decrypt(self, algorithm: CipherAlgorithm) -> None:
        """algorithm 为被测实现；准备密钥与适用的 IV，验证完整公开 API 往返。"""
        plaintext = "This is a test message. 中文"
        key = "k" * (32 if algorithm is CipherAlgorithm.AES_GCM else 16)
        # CBC 每次加密先生成 IV 并保存；GCM 的 Nonce 由内部随机生成。
        salt = generate_salt(16) if algorithm is CipherAlgorithm.LEGACY_CBC else None
        encrypted = encrypt(plaintext, key, salt, algorithm=algorithm)
        assert isinstance(encrypted, bytes), "Encrypted result must be bytes"
        # 默认识别算法；CBC 必须复用加密时的 IV，不能重新生成。
        assert decrypt(encrypted, key, salt) == plaintext

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

    def test_encrypt_rejects_missing_cbc_salt(self) -> None:
        """默认入口、仅提供 Key 及显式 None 都不能再把 Key 隐式作为加密 IV。"""
        with pytest.raises(ValueError, match="explicit salt"):
            encrypt("secret")
        with pytest.raises(ValueError, match="explicit salt"):
            encrypt("secret", "0123456789abcdef")
        with pytest.raises(ValueError, match="explicit salt"):
            encrypt("secret", "0123456789abcdef", None, algorithm=CipherAlgorithm.LEGACY_CBC)

    @pytest.mark.parametrize("salt", ["", "short", "v" * 17])
    def test_encrypt_rejects_invalid_cbc_salt(self, salt: str) -> None:
        """salt 为无效 IV；显式提供参数仍必须满足 AES 的 16 字节 IV 契约。"""
        with pytest.raises(ValueError):
            encrypt("secret", salt=salt)

    def test_default_key_with_explicit_iv(self) -> None:
        """默认 Key 仍可与显式 IV 使用，固定历史向量保证 CBC 字节格式不变。"""
        ciphertext = encrypt("secret", salt="abcdefghijklmnop")
        assert ciphertext == b"vIFLz+7UV9fFbJtsi/P9pw=="
        assert decrypt(ciphertext, salt="abcdefghijklmnop") == "secret"

    def test_decrypt_historical_default_iv(self) -> None:
        """固定向量来自旧实现，证明省略 IV 的历史密文仍可解密。"""
        assert decrypt(b"MUSltWQ6CEFtWbeYRymj+A==") == "Default test"

    def test_gcm_randomness_and_reference_ciphertext(self) -> None:
        """独立固定向量约束跨语言格式，并检查相同输入仍生成不同的 GCM 密文。"""
        key = b"0123456789abcdef0123456789abcdef"
        fixture = (
            b"bWVudGFsMTEwNDpBRVM6AQABAgMEBQYHCAkKC5vD5fpUDmE02C64madn0BhO"
            b"l9QPqjW+zca7BN8WqvF3g2tx4kg="
        )
        assert decrypt(fixture, key) == "cross-language secret"
        assert encrypt("secret", key, algorithm=CipherAlgorithm.AES_GCM) != encrypt(
            "secret", key, algorithm=CipherAlgorithm.AES_GCM
        )

    @pytest.mark.parametrize("salt", ["", "v" * 16])
    def test_gcm_rejects_salt(self, salt: str) -> None:
        """salt 是 CBC 专属参数，即使为空串，GCM 加解密均应拒绝。"""
        key = b"k" * 32
        ciphertext = encrypt("secret", key, algorithm=CipherAlgorithm.AES_GCM)
        with pytest.raises(ValueError, match="does not accept salt"):
            encrypt("secret", key, salt, algorithm=CipherAlgorithm.AES_GCM)
        with pytest.raises(ValueError, match="does not accept salt"):
            decrypt(ciphertext, key, salt)

    @pytest.mark.parametrize("key", [b"", b"k" * 16, b"k" * 24, b"k" * 31, b"k" * 33])
    def test_gcm_requires_256_bit_key(self, key: bytes) -> None:
        """key 为非 32 字节输入；不得自动补齐、派生或退回默认 Key。"""
        with pytest.raises(ValueError, match="explicit 32-byte key"):
            encrypt("secret", key, algorithm=CipherAlgorithm.AES_GCM)

    def test_gcm_rejects_default_key(self) -> None:
        """省略 GCM Key 必须明确报错，不能沿用历史硬编码 Key。"""
        with pytest.raises(ValueError, match="explicit 32-byte key"):
            encrypt("secret", algorithm=CipherAlgorithm.AES_GCM)

    @pytest.mark.parametrize("offset", [16, 28, 44])
    def test_gcm_detects_tampering(self, offset: int) -> None:
        """offset 分别落在 Nonce、Tag 和密文；篡改任何字段均应认证失败。"""
        key = b"k" * 32
        payload = bytearray(
            base64.b64decode(encrypt("secret", key, algorithm=CipherAlgorithm.AES_GCM))
        )
        payload[offset] ^= 1
        with pytest.raises(ValueError, match="MAC check failed"):
            decrypt(base64.b64encode(payload), key)

    def test_gcm_rejects_wrong_key(self) -> None:
        """长度合法但内容错误的 Key 必须通过认证机制检测。"""
        ciphertext = encrypt("secret", b"k" * 32, algorithm=CipherAlgorithm.AES_GCM)
        with pytest.raises(ValueError, match="MAC check failed"):
            decrypt(ciphertext, b"x" * 32)

    @pytest.mark.parametrize("payload", [b"mental1104:AES:\x02", b"mental1104:AES:\x01"])
    def test_gcm_rejects_versions_and_truncation(self, payload: bytes) -> None:
        """payload 为未知版本或截断的新封装；只捕获 CBC 不会出现的明确格式错误。"""
        with pytest.raises(ValueError, match=r"version|Truncated"):
            decrypt(base64.b64encode(payload), b"k" * 32)

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
