import os

from functools import wraps


def file_iterator(process_function):
    """
    装饰器，用于遍历指定目录下的所有文件，或者直接处理给定的文件路径，并对每个文件执行给定的处理函数。
    Args:
        process_function (function): 被装饰的函数，接受一个文件路径作为参数。
    Returns:
        function: 包装后的函数，接受一个文件路径参数。
    Raises:
        ValueError: 如果输入路径既不是文件也不是目录。
    例如：
        @file_iterator
        def process_file(file_path):
            print(f"Processing file: {file_path}")
        
        process_file('path/to/directory')  # 处理目录下的所有文件
        process_file('path/to/file.txt')    # 直接处理单个文件
    """
    @wraps(process_function)
    def wrapper(input_path):
        # 如果输入路径是文件，直接处理该文件
        if os.path.isfile(input_path):
            process_function(input_path)  # 传递文件的完整路径
        # 如果输入路径是目录，递归遍历该目录下的所有文件
        elif os.path.isdir(input_path):
            process_directory(input_path)
        else:
            raise ValueError(f"输入路径 '{input_path}' 既不是文件也不是目录。")

    def process_directory(directory):
        """递归处理目录中的所有文件"""
        for entry in os.listdir(directory):
            full_path = os.path.join(directory, entry)
            if os.path.isdir(full_path):
                # 如果是目录，递归调用
                process_directory(full_path)
            elif os.path.isfile(full_path):
                # 如果是文件，处理该文件
                process_function(full_path)

    return wrapper



