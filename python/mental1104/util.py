import functools
import time
import os
import re
from typing import Callable, Any
import csv
from functools import wraps

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

class StringHelper:

    """将字符串中间的换行符和空白全部替换成指定分隔符

    Returns:
        _type_: _description_
    """    
    @staticmethod
    def replace_space_with(input_string, seperator='|'):
        words = re.findall(r'\S+', input_string)
        return seperator.join(words)
