from functools import wraps
import json

# 可扩展的解析器映射表
PARSERS = {
    "json": json.loads,
}

# 尝试导入可选的解析库
try:
    import ujson
    PARSERS["ujson"] = ujson.loads
except ImportError:
    pass

try:
    import orjson
    PARSERS["orjson"] = lambda s: orjson.loads(s)  # orjson 返回 bytes，可按需转 dict
except ImportError:
    pass

# ✨ 暴露所有已注册解析器的名称列表
PARSER_NAMES = list(PARSERS.keys())

def parse_json(s: str, parser: str = "json"):
    """
    通用 JSON 解析器。

    参数:
        s (str): 待解析的 JSON 字符串。
        parser (str): 使用的解析库，默认 "json"。支持 PARSERS 中的所有 key。

    返回:
        dict | list | None: 解析成功返回对象，失败返回 None。
    """
    if parser not in PARSERS:
        raise ValueError(f"未知解析器 '{parser}'，可选: {list(PARSERS.keys())}")

    try:
        return PARSERS[parser](s)
    except Exception as e:
        print(f"[解析失败] 错误信息：{e}")
        # 获取错误位置
        pos = getattr(e, 'pos', None)
        if not pos and hasattr(e, 'lineno') and hasattr(e, 'colno'):
            try:
                lines = s.splitlines()
                pos = sum(len(line) + 1 for line in lines[:e.lineno - 1]) + e.colno - 1
            except Exception:
                pos = None

        if pos is not None:
            start = max(0, pos - 25)
            end = min(len(s), pos + 25)
            snippet = s[start:end]
            pointer = " " * (pos - start) + "^"
            print("[错误上下文]")
            print(snippet)
            print(pointer)
        return None

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