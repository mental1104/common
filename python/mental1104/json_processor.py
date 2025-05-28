from functools import wraps
import json


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