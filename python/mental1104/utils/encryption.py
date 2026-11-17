"""提供显式 CBC IV 和 AES-256-GCM 认证加密，并保留历史密文解密能力。"""

import base64
from enum import Enum
from typing import Optional, Union

from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes
from Crypto.Util.Padding import pad, unpad


class CipherAlgorithm(Enum):
    """选择加密实现；LEGACY_CBC 仅用于兼容，AES_GCM 用于新业务的认证加密。"""

    LEGACY_CBC = "legacy_cbc"
    AES_GCM = "aes_gcm"


# 新格式为 Magic + Version + Nonce + Tag + Ciphertext，整个头部参与认证。
_GCM_MAGIC = b"mental1104:AES:"
_GCM_VERSION = b"\x01"
_GCM_NONCE_SIZE = 12
_GCM_TAG_SIZE = 16


def _encrypt_legacy(plaintext: str, key: str, salt: Optional[str]) -> bytes:
    """按历史 CBC 线格式加密，但必须显式提供 IV。

    :param plaintext: 按 UTF-8 编码的明文字符串。
    :param key: 历史字符串密钥，长度由 AES 后端校验。
    :param salt: 显式提供的字符串 IV，UTF-8 编码后必须为 16 字节。
    :return: 使用 PKCS#7 Padding 的原始密文字节，不包含格式头。
    :raises ValueError: IV 缺失，或密钥、IV 长度无效。
    """
    if salt is None:
        # 只收紧新加密；历史解密仍在独立路径中保留 Key 作为 IV。
        raise ValueError("Legacy CBC encryption requires an explicit salt (IV)")

    key_bytes = bytes(key, encoding="utf-8")
    iv_bytes = bytes(salt, encoding="utf-8")
    aes = AES.new(key_bytes, mode=AES.MODE_CBC, IV=iv_bytes)

    padded_plaintext = pad(plaintext.encode("utf-8"), AES.block_size)
    return aes.encrypt(padded_plaintext)


def _decrypt_legacy(ciphertext: bytes, key: str, salt: Optional[str]) -> str:
    """按历史 CBC 规则解密；该格式没有认证，Padding 校验不能证明完整性。

    :param ciphertext: 已去除 Base64 编码的历史密文字节。
    :param key: 历史字符串密钥，长度由 AES 后端校验。
    :param salt: 字符串 IV；None 表示使用 key。
    :return: 去除 PKCS#7 Padding 后按 UTF-8 解码的明文字符串。
    :raises ValueError: 密钥、IV、密文长度或 Padding 无效。
    :raises UnicodeDecodeError: 解密结果不是有效 UTF-8 文本。
    """
    if salt is None:
        salt = key

    key_bytes = bytes(key, encoding="utf-8")
    iv_bytes = bytes(salt, encoding="utf-8")
    aes = AES.new(key_bytes, mode=AES.MODE_CBC, IV=iv_bytes)

    decrypted = aes.decrypt(ciphertext)
    unpadded_plaintext = unpad(decrypted, AES.block_size)

    return unpadded_plaintext.decode("utf-8")


def _gcm_key(key: Union[str, bytes], salt: Optional[str]) -> bytes:
    """校验 GCM 专属参数，不派生密钥或复用旧 salt。

    :param key: 调用方提供的原始 bytes 密钥，或按 UTF-8 编码的字符串密钥。
    :param salt: 只允许 None；Nonce 必须由加密函数随机生成。
    :return: 恰好 32 字节的 AES-256 密钥。
    :raises TypeError: 密钥不是 str 或 bytes。
    :raises ValueError: 提供了 salt，或密钥长度不是 32 字节（包括旧默认 Key）。
    """
    if salt is not None:
        raise ValueError("AES-GCM does not accept salt; its nonce is generated automatically")
    if isinstance(key, str):
        key_bytes = key.encode("utf-8")
    elif isinstance(key, bytes):
        key_bytes = key
    else:
        raise TypeError("AES-GCM key must be str or bytes")
    if len(key_bytes) != 32:
        raise ValueError("AES-256-GCM requires an explicit 32-byte key")
    return key_bytes


def _encrypt_gcm(plaintext: str, key: Union[str, bytes], salt: Optional[str]) -> bytes:
    """使用随机 Nonce 加密，并把版本头作为附加认证数据。

    :param plaintext: 按 UTF-8 编码的明文字符串，不使用 Padding。
    :param key: 显式提供的 32 字节密钥，字符串按 UTF-8 编码。
    :param salt: 仅允许 None；传入旧 salt 会抛出 ValueError。
    :return: 包含版本头、12 字节 Nonce、16 字节 Tag 和密文的原始字节。
    :raises ValueError: 密钥长度错误，或提供了 salt。
    :raises TypeError: 密钥类型错误。
    """
    key_bytes = _gcm_key(key, salt)
    nonce = get_random_bytes(_GCM_NONCE_SIZE)
    header = _GCM_MAGIC + _GCM_VERSION
    aes = AES.new(key_bytes, mode=AES.MODE_GCM, nonce=nonce, mac_len=_GCM_TAG_SIZE)
    aes.update(header)
    ciphertext, tag = aes.encrypt_and_digest(plaintext.encode("utf-8"))
    return header + nonce + tag + ciphertext


def _decrypt_gcm(payload: bytes, key: Union[str, bytes], salt: Optional[str]) -> str:
    """解析 GCM 版本并验证认证标签，验证成功前不返回任何明文。

    :param payload: 已去除 Base64 编码的完整版本化密文字节。
    :param key: 显式提供的 32 字节密钥，字符串按 UTF-8 编码。
    :param salt: 仅允许 None；传入旧 salt 会抛出 ValueError。
    :return: 通过认证后按 UTF-8 解码的明文字符串。
    :raises ValueError: 格式、版本、参数无效，或密钥错误、密文被篡改导致认证失败。
    :raises TypeError: 密钥类型错误。
    :raises UnicodeDecodeError: 通过认证的明文不是有效 UTF-8 文本。
    """
    header_size = len(_GCM_MAGIC) + len(_GCM_VERSION)
    if not payload.startswith(_GCM_MAGIC) or len(payload) < header_size:
        raise ValueError("Invalid or truncated AES-GCM ciphertext header")
    if payload[len(_GCM_MAGIC) : header_size] != _GCM_VERSION:
        raise ValueError("Unsupported AES-GCM ciphertext version")
    nonce_end = header_size + _GCM_NONCE_SIZE
    tag_end = nonce_end + _GCM_TAG_SIZE
    if len(payload) < tag_end:
        raise ValueError("Truncated AES-GCM ciphertext")

    aes = AES.new(
        _gcm_key(key, salt),
        mode=AES.MODE_GCM,
        nonce=payload[header_size:nonce_end],
        mac_len=_GCM_TAG_SIZE,
    )
    aes.update(payload[:header_size])
    # 认证失败必须直接抛出异常，不能把同一份数据交给 Legacy CBC 重试。
    plaintext = aes.decrypt_and_verify(payload[tag_end:], payload[nonce_end:tag_end])
    return plaintext.decode("utf-8")


def encrypt(
    plaintext: str,
    key: Union[str, bytes] = "0ePThPnLaJcWFcRc",
    salt: Optional[str] = None,
    *,
    algorithm: CipherAlgorithm = CipherAlgorithm.LEGACY_CBC,
) -> bytes:
    """加密 UTF-8 文本并返回 Base64 bytes；默认 CBC 必须显式提供 IV。

    :param plaintext: 明文字符串。
    :param key: CBC 使用历史字符串密钥及默认 Key；GCM 必须显式提供 32 字节
        bytes 或 UTF-8 编码后恰好 32 字节的字符串，不使用默认 Key。
    :param salt: CBC 必须显式提供字符串 IV，例如 generate_salt(16) 的结果；
        None 仅作为共用入口的缺省标记，CBC 会拒绝，GCM 仅允许 None。
    :param algorithm: 仅限关键字；默认 LEGACY_CBC，新业务应显式选择 AES_GCM。
    :return: 整体经标准 Base64 编码的密文 bytes。
    :raises ValueError: 算法或加密参数无效。
    :raises TypeError: 密钥类型不受所选算法支持。
    """
    if algorithm is CipherAlgorithm.LEGACY_CBC:
        if not isinstance(key, str):
            raise TypeError("Legacy CBC requires a string key")
        payload = _encrypt_legacy(plaintext, key, salt)
    elif algorithm is CipherAlgorithm.AES_GCM:
        payload = _encrypt_gcm(plaintext, key, salt)
    else:
        raise ValueError("Unsupported cipher algorithm")
    return base64.b64encode(payload)


def decrypt(
    ciphertext: Union[str, bytes, bytearray],
    key: Union[str, bytes] = "0ePThPnLaJcWFcRc",
    salt: Optional[str] = None,
    *,
    algorithm: Optional[CipherAlgorithm] = None,
) -> str:
    """解密 Base64 密文；默认识别版本化 GCM，否则使用历史 CBC。

    :param ciphertext: 标准 Base64 密文，保留历史 str、bytes 和 bytearray 输入。
    :param key: CBC 使用历史字符串密钥及默认 Key；GCM 必须显式提供 32 字节
        bytes 或 UTF-8 编码后恰好 32 字节的字符串。
    :param salt: CBC 的字符串 IV，None 时使用 Key；GCM 仅允许 None。
    :param algorithm: 仅限关键字；None 自动识别，或显式指定具体算法。
        指定 CBC 不能解密带 GCM Magic 的新格式。
    :return: 解密后的 UTF-8 明文字符串。
    :raises ValueError: 算法、格式、版本、参数、Padding 无效，或 GCM 认证失败。
        识别为新格式后，任何失败均不会降级尝试 CBC。
    :raises TypeError: 密钥或密文类型无效。
    :raises UnicodeDecodeError: 解密结果不是有效 UTF-8 文本。
    """
    if algorithm is not None and not isinstance(algorithm, CipherAlgorithm):
        raise ValueError("Unsupported cipher algorithm")
    payload = base64.b64decode(ciphertext)
    is_gcm = payload.startswith(_GCM_MAGIC)
    if algorithm is None:
        algorithm = CipherAlgorithm.AES_GCM if is_gcm else CipherAlgorithm.LEGACY_CBC
    if algorithm is CipherAlgorithm.AES_GCM:
        return _decrypt_gcm(payload, key, salt)
    # 即使显式选择 CBC，也不能绕过已识别新格式的认证与版本检查。
    if is_gcm:
        raise ValueError("AES-GCM ciphertext cannot be decrypted with Legacy CBC")
    if not isinstance(key, str):
        raise TypeError("Legacy CBC requires a string key")
    return _decrypt_legacy(payload, key, salt)


def generate_salt(length=16):
    """
    生成指定长度的随机盐值。
    :param length: 盐值长度, 默认为 16
    :return: 生成的随机盐值字符串
    例如：
        >>> salt = Encryption.generate_salt(16)
        >>> print(salt)
        'a1b2c3d4e5f6g7h8'
    """
    if length < 0:
        raise ValueError("Length must be non-negative")

    import secrets
    import string

    # 生成一个包含字母和数字的随机字符串
    characters = string.ascii_letters + string.digits
    return "".join(secrets.choice(characters) for _ in range(length))
