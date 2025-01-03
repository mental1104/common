import functools
import time
import os
import re
import sys
import base64
from typing import Callable, Any
import csv
from functools import wraps
from datetime import datetime
from Crypto.Cipher import AES 
from Crypto.Util.Padding import pad, unpad

def timed():
    def wrapper(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapped(*args, **kwargs) -> Any:
            print(f'starting {func} with args {args} {kwargs}')
            start = time.time()
            try:
                return func(*args, **kwargs)
            finally:
                end = time.time()
                total = end - start
                print(f'finished {func} in {total:.4f} second(s)')
        return wrapped
    return wrapper


def delay(delay_seconds: int) -> int:
    print(f'sleeping for {delay_seconds} second(s)')
    time.sleep(delay_seconds)
    print(f'finished sleeping for {delay_seconds} second(s)')
    return delay_seconds


def file_iterator(process_function):
    """
    装饰器，用于遍历指定目录下的所有文件，并对每个文件执行给定的处理函数。
    """
    def wrapper(input_path):
        for entry in os.listdir(input_path):
            dir_path = os.path.join(input_path, entry)
            # 判断是否为目录
            if os.path.isdir(dir_path):
                print(entry)
                for file in os.listdir(dir_path):
                    full_path = os.path.join(dir_path, file)
                    if os.path.isfile(full_path):
                        process_function(file)

    return wrapper

class CsvHelper:
    
    @staticmethod
    def csv_processor(file_path, has_header=True):
        """
        装饰器，用于处理 CSV 文件，将其内容解析为字典数组或元组数组，并传递给被装饰函数。
        
        Args:
            file_path (str): CSV 文件的路径。
            has_header (bool): 是否包含表头。如果为 True，返回字典数组；否则返回元组数组。

        Returns:
            function: 包装后的函数。
        """
        def decorator(func):
            @wraps(func)
            def wrapper(*args, **kwargs):
                with open(file_path, mode='r', encoding='utf-8') as f:
                    if has_header:
                        reader = csv.DictReader(f)  # 包含表头，解析为字典
                        data = [row for row in reader]
                    else:
                        reader = csv.reader(f)  # 不包含表头，解析为元组
                        data = [tuple(row) for row in reader]
                
                # 将解析的内容传递给被装饰函数，并返回结果
                return func(data, *args, **kwargs)
            return wrapper
        return decorator

    @staticmethod
    def export_csv_from_database(query, path=None):
        """
        Args:
            query (SQLalchemy): sqlalchemy的查询语句，需在上层绑定session，关联会话。
            path (string): csv目标路径

        Returns:
            function: 包装后的函数。
        """
        import tempfile
        if path is None:
            with tempfile.NamedTemporaryFile(suffix=".csv", dir="/tmp", delete=False) as tmp_file:
                path = tmp_file.name
            print(f"未提供路径，已生成随机 CSV 文件：{os.path.abspath(path)}")

        with open(path, mode='w') as csvfile:
            row = query.first()
            if row is None:
                raise ValueError("查询结果为空，无法导出 CSV 文件。")

            writer = csv.DictWriter(csvfile, list(row._asdict().keys()))
            writer.writeheader()  # 写入列名

            for row in query.yield_per(1):
                result = row._asdict()
                for key, val in result.items():
                    if isinstance(val, list):
                        result[key] = ','.join(val)
                writer.writerow(result)
        
        print(f"CSV 文件已成功导出至：{os.path.abspath(path)}")
        return os.path.abspath(path)


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
    
    g_salt = "default_salt"

    @staticmethod
    def encrypt(plaintext, key=g_salt, salt=g_salt):
        key = bytes(key, encoding="utf-8")
        salt = bytes(salt, encoding="utf-8")
        aes = AES.new(key, mode=AES.MODE_CBC, IV=salt)

        padded_plaintext = pad(plaintext.encode('utf-8'), AES.block_size)
        encrypted = aes.encrypt(padded_plaintext)

        return base64.b64encode(encrypted)

    @staticmethod
    def decrypt(ciphertext, key=g_salt, salt=g_salt):
        key = bytes(key, encoding="utf-8")
        salt = bytes(salt, encoding="utf-8")
        aes = AES.new(key, mode=AES.MODE_CBC, IV=salt)

        ciphertext = base64.b64decode(ciphertext)

        decrypted = aes.decrypt(ciphertext)
        unpadded_plaintext = unpad(decrypted, AES.block_size)

        return unpadded_plaintext.decode('utf-8')


class TimeHelper:
    
    @staticmethod
    def get_current_time(format="%Y-%m-%d %H:%M:%S", zone="Asia/Shanghai"):
        from zoneinfo import ZoneInfo
        return datetime.now(ZoneInfo(zone)).strftime(format)
    

if __name__ == '__main__':
    print(TimeHelper.get_current_time())
    print(TimeHelper.get_current_time("%Y-%m-%d"))