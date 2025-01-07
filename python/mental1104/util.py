import time
import os
import re
import sys
import base64
import asyncio
from datetime import datetime
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad


def delay(delay_seconds: int) -> int:
    if delay_seconds < 0:
        raise ValueError("Delay time cannot be negative")
    print(f'sleeping for {delay_seconds} second(s)')
    time.sleep(delay_seconds)
    print(f'finished sleeping for {delay_seconds} second(s)')
    return delay_seconds


async def async_delay(delay_seconds: int) -> int:
    if delay_seconds < 0:
        raise ValueError("Delay time cannot be negative")
    print(f'sleeping for {delay_seconds} second(s)')
    await asyncio.sleep(delay_seconds)
    print(f'finished sleeping for {delay_seconds} second(s)')
    return delay_seconds


class StringHelper:

    """将字符串中间的换行符和空白全部替换成指定分隔符

    Returns:
        _type_: _description_
    """
    @staticmethod
    def replace_space_with(input_string, seperator='|'):
        words = re.findall(r'\S+', input_string)
        return seperator.join(words)


class Environment:

    @staticmethod
    def check_required_env_vars(required_env_vars):
        """
        检查是否具有给定的环境变量，若没有，则中止执行并打印缺少的环境变量。

        :param required_env_vars: 需要检查的环境变量列表
        """
        for var in required_env_vars:
            if var not in os.environ:
                print(f"Error: Missing required environment variable: {var}")
                sys.exit(1)  # 中止执行并返回非零状态


class Encryption:

    @staticmethod
    def encrypt(plaintext, key="0ePThPnLaJcWFcRc", salt=None):
        if salt is None:
            salt = key

        key = bytes(key, encoding="utf-8")
        salt = bytes(salt, encoding="utf-8")
        aes = AES.new(key, mode=AES.MODE_CBC, IV=salt)

        padded_plaintext = pad(plaintext.encode('utf-8'), AES.block_size)
        encrypted = aes.encrypt(padded_plaintext)

        return base64.b64encode(encrypted)

    @staticmethod
    def decrypt(ciphertext, key="0ePThPnLaJcWFcRc", salt=None):
        if salt is None:
            salt = key

        key = bytes(key, encoding="utf-8")
        salt = bytes(salt, encoding="utf-8")
        aes = AES.new(key, mode=AES.MODE_CBC, IV=salt)

        ciphertext = base64.b64decode(ciphertext)

        decrypted = aes.decrypt(ciphertext)
        unpadded_plaintext = unpad(decrypted, AES.block_size)

        return unpadded_plaintext.decode('utf-8')

    def generate_salt(length=16):
        if length < 0:
            raise ValueError("Length must be non-negative")

        import secrets
        import string
        # 生成一个包含字母和数字的随机字符串
        characters = string.ascii_letters + string.digits
        salt = ''.join(secrets.choice(characters) for _ in range(length))
        return salt


class TimeHelper:

    @staticmethod
    def get_current_time(format="%Y-%m-%d %H:%M:%S", zone="Asia/Shanghai"):
        from zoneinfo import ZoneInfo
        return datetime.now(ZoneInfo(zone)).strftime(format)
