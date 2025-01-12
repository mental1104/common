import csv
import os
from functools import wraps


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
    def csv_writer(data=None, file_path=None):
        """
        将队列数据写入 CSV 文件。

        Args:
            file_path (str, optional): CSV 文件路径。默认为 None，生成临时文件。
            data (iterable): 包含要写入的数据的迭代对象。
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
