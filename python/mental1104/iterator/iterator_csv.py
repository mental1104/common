import os
import csv
from functools import wraps


def iterator_csv(has_header=True):
    """
    装饰器，用于处理 CSV 文件，将其内容解析为字典数组或元组数组，并传递给被装饰函数。

    Args:
        has_header (bool): 是否包含表头。如果为 True，返回字典数组；否则返回元组数组。

    Returns:
        function: 包装后的函数，接受一个文件路径参数。
    Raises:
        FileNotFoundError: 如果指定的文件不存在。
        Exception: 处理文件时发生其他异常。
    例如：
    @iterator_csv(has_header=True)
    def process_csv(data):
        for row in data:
            print(row)
    process_csv('data.csv')
    """
    def decorator(func):
        @wraps(func)
        def wrapper(file_path):
            try:
                with open(file_path, mode='r', encoding='utf-8') as f:
                    if has_header:
                        reader = csv.DictReader(f)  # 包含表头，解析为字典
                        data = [row for row in reader]
                    else:
                        reader = csv.reader(f)  # 不包含表头，解析为元组
                        data = [tuple(row) for row in reader]

                # 将解析的内容传递给被装饰函数，并返回结果
                return func(data)
            except FileNotFoundError:
                print(f"错误: 文件 '{file_path}' 未找到。")
                return None
            except Exception as e:
                print(f"错误: 处理文件 '{file_path}' 时发生异常: {e}")
                return None

        return wrapper
    return decorator
