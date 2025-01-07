import pytest
import os
import csv
from io import StringIO
from unittest.mock import MagicMock  # 从unittest.mock中导入MagicMock
from mental1104.file import file_iterator, CsvHelper

# 测试用例
class TestFileIterator:

    def test_file_iterator(self, mocker):
        # 模拟输入路径
        input_path = "/mock/path"

        # 模拟 os.listdir 返回的目录和文件列表
        mock_listdir = mocker.patch("os.listdir", side_effect=[
            ["dir1", "dir2"],  # 第一次调用返回的目录
            ["file1.txt", "file2.txt"],  # dir1 目录下的文件
            ["file3.txt"]  # dir2 目录下的文件
        ])

        # 模拟 os.path.isdir 返回值，模拟所有目录都为 True
        mock_isdir = mocker.patch("os.path.isdir", side_effect=lambda x: x in ["/mock/path/dir1", "/mock/path/dir2"])

        # 模拟 os.path.isfile 返回值，模拟文件路径
        mock_isfile = mocker.patch("os.path.isfile", side_effect=lambda x: x in [
            "/mock/path/dir1/file1.txt",
            "/mock/path/dir1/file2.txt",
            "/mock/path/dir2/file3.txt"
        ])

        # 记录 process_function 被调用的参数
        processed_files = []

        def process_function(file):
            processed_files.append(file)

        # 包裹原始函数
        decorated_function = file_iterator(process_function)

        # 执行被装饰的函数
        decorated_function(input_path)

        # 断言 process_function 函数被调用的文件列表
        assert processed_files == ["file1.txt", "file2.txt", "file3.txt"]
