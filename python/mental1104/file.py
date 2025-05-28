import csv
import os
import json
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


def json_processor(func):
    """
    装饰器，用于处理 JSON 文件，将其内容解析为 Python 对象，并传递给被装饰函数。
    若输入文件路径是一个目录，则会假定这个目录下的所有文件都是合法的json，递归式地处理每个文件。
    如果解析失败或文件不存在，则返回 None 作为默认值。
    Args:
        func (function): 被装饰的函数，接受一个参数（解析后的 JSON 数据）。
    Returns:
        function: 包装后的函数，接受一个文件路径参数。
    Raises:
        json.JSONDecodeError: 如果 JSON 文件解析失败。
        FileNotFoundError: 如果指定的文件不存在。
        Exception: 处理文件时发生其他异常。
    例如：
        @json_processor
        def process_json(data):
            print(data)
        
        process_json('data.json')
        
        # 或者处理目录下的所有 JSON 文件
        process_json('path/to/directory')
    """
    @wraps(func)
    def wrapper(file_path):
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)  # 解析 JSON 文件
            return func(data)  # 将解析后的数据传递给被装饰函数
        except json.JSONDecodeError as e:
            print(f"错误: 无法解析 JSON 文件 '{file_path}': {e}")
            return None  # 返回 None 作为默认值
        except FileNotFoundError:
            print(f"错误: 文件 '{file_path}' 未找到。")
            return None  # 返回 None 作为默认值
        except Exception as e:
            print(f"错误: 处理文件 '{file_path}' 时发生异常: {e}")
            return None  # 返回 None 作为默认值

    return wrapper

class CsvHelper:

    @staticmethod
    def csv_processor(has_header=True):
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
        @CsvHelper.csv_processor(has_header=True)
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

    @staticmethod
    def csv_writer(data=None, file_path=None):
        """
        将队列数据写入 CSV 文件。

        Args:
            file_path (str, optional): CSV 文件路径。默认为 None，生成临时文件。
            data (iterable): 包含要写入的数据的迭代对象，可以是列表、元组或字典数组。
        Returns:
            None
        Raises:
            ValueError: 如果提供的数据为空。
            IOError: 如果写入 CSV 文件失败。
        例如：
        CsvHelper.csv_writer(data=my_data, file_path='output.csv')
        """
        import tempfile
        if not data:
            raise ValueError("提供的数据为空！")

        # 如果文件路径为 None，创建临时文件
        if file_path is None:
            tmp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".csv", dir="/tmp")
            file_path = tmp_file.name

        # 如果路径中的目录不存在则创建
        os.makedirs(os.path.dirname(file_path), exist_ok=True)

        # 检查队列数据的类型
        sample = next(iter(data))  # 获取第一个元素用于检查
        is_dict = isinstance(sample, dict)

        try:
            with open(file_path, mode="w", encoding="utf-8", newline="") as f:
                if is_dict:
                    writer = csv.DictWriter(f, fieldnames=sample.keys())
                    writer.writeheader()
                    writer.writerows(data)
                else:
                    writer = csv.writer(f)
                    writer.writerows(data)

            print(f"数据已导出到文件：{file_path}")
        except Exception as e:
            raise IOError(f"写入 CSV 文件失败：{e}")

    @staticmethod
    def export_csv_from_database(query, path=None):
        """
        Args:
            query (SQLalchemy): sqlalchemy的查询语句，需在上层绑定session，关联会话。
            path (string): csv目标路径

        Returns:
            function: 包装后的函数，接受一个文件路径参数。
        Raises:
            ValueError: 如果查询结果为空，无法导出 CSV 文件。
            IOError: 如果写入 CSV 文件失败。
        例如：
        CsvHelper.export_csv_from_database(query, path='output.csv')
        该方法将查询结果导出为 CSV 文件。
        该方法会检查查询结果是否为空，如果为空则抛出 ValueError。
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
