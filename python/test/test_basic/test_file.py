'''
Date: 2025-01-24 13:55:33
Author: mental1104 mental1104@gmail.com
LastEditors: mental1104 mental1104@gmail.com
LastEditTime: 2025-01-24 22:57:53
'''
from unittest.mock import call
from mental1104 import file_iterator

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

        # 模拟 os.path.isdir 返回值，指定哪些是目录
        mocker.patch("os.path.isdir", side_effect=lambda x: x in ["/mock/path", "/mock/path/dir1", "/mock/path/dir2"])

        # 模拟 os.path.isfile 返回值，指定哪些是文件
        mocker.patch("os.path.isfile", side_effect=lambda x: x in [
            "/mock/path/dir1/file1.txt",
            "/mock/path/dir1/file2.txt",
            "/mock/path/dir2/file3.txt"
        ])

        # 创建一个用于记录被处理文件的函数
        mock_process_function = mocker.Mock()

        # 应用装饰器
        decorated_function = file_iterator(mock_process_function)

        # 执行被装饰的函数
        decorated_function(input_path)

        # 验证 os.listdir 被正确调用
        mock_listdir.assert_has_calls([
            call("/mock/path"),
            call("/mock/path/dir1"),
            call("/mock/path/dir2"),
        ])

        # 验证处理函数被正确调用了三次，并且参数是文件的完整路径
        mock_process_function.assert_has_calls([
            call("/mock/path/dir1/file1.txt"),
            call("/mock/path/dir1/file2.txt"),
            call("/mock/path/dir2/file3.txt"),
        ])

        # 确保处理函数的调用次数符合预期
        assert mock_process_function.call_count == 3
