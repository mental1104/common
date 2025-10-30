import json
import io
from functools import singledispatch
from types import MappingProxyType
from typing import Any, Callable, Mapping


class JsonUtil:
    """
    私有封装解析器相关数据，提供只读访问。
    """
    __parsers: dict[str, Callable[[str | bytes], Any]] = {"json": json.loads}
    __modules: dict[str, Any] = {"json": json}

    # 尝试注册可选解析器（存在才添加）
    try:
        import ujson as _ujson
        __parsers["ujson"] = _ujson.loads
        __modules["ujson"] = _ujson
    except Exception:
        pass

    try:
        import orjson as _orjson
        __parsers["orjson"] = _orjson.loads  # orjson 无 load(fp)
        __modules["orjson"] = _orjson
    except Exception:
        pass

    __parser_names: tuple[str, ...] = tuple(__parsers.keys())

    @staticmethod
    def get_parsers() -> Mapping[str, Callable[[str | bytes], Any]]:
        """返回只读的解析器映射，避免外部修改。"""
        return MappingProxyType(JsonUtil.__parsers)

    @staticmethod
    def get_parser_names() -> tuple[str, ...]:
        """返回只读的解析器名称元组。"""
        return JsonUtil.__parser_names

    @staticmethod
    def _parse_from_file(fp: io.IOBase, parser: str) -> Any:
        """
        统一的“从文件流解析”入口：
        - 若库有 load(fp) 且是文本流，则优先走 load(fp)（保留更好的错误位置信息）
        - 否则读出内容并走 loads(...)
        """
        mods = JsonUtil.__modules
        loads = JsonUtil.__parsers[parser]
        mod = mods.get(parser)

        # 文本流优先使用库的 load（若存在）
        if isinstance(fp, io.TextIOBase) and mod is not None and hasattr(mod, "load"):
            # json/ujson：都有 load(fp)
            return mod.load(fp)

        # 其它情况（含二进制流）：读出后交给 loads
        data = fp.read()
        # 三家 loads 均可接受 str 或 bytes；如为 bytearray 也可直接给大多数实现
        if isinstance(data, (bytes, bytearray)) or isinstance(data, str):
            return loads(data)
        # 极端情况下 read() 返回的不是预期类型，统一转成 str
        return loads(str(data))


# ========= 使用 singledispatch 作为内部分发器 =========
@singledispatch
def _parse_dispatch(value: object, parser: str) -> Any:
    raise TypeError(f"不支持的入参类型: {type(value)!r}（期望 str 或文件流）")

@_parse_dispatch.register(str)
def _(s: str, parser: str) -> Any:
    parsers = JsonUtil.get_parsers()
    return parsers[parser](s)

@_parse_dispatch.register(io.TextIOBase)
def _(fp: io.TextIOBase, parser: str) -> Any:
    return JsonUtil._parse_from_file(fp, parser)

@_parse_dispatch.register(io.BufferedIOBase)
def _(fp: io.BufferedIOBase, parser: str) -> Any:
    return JsonUtil._parse_from_file(fp, parser)

@_parse_dispatch.register(io.RawIOBase)
def _(fp: io.RawIOBase, parser: str) -> Any:
    return JsonUtil._parse_from_file(fp, parser)


def load_json(s: str, parser: str = "json"):
    """
    通用 JSON 解析器。

    参数:
        s (str|IO): 待解析的 JSON 字符串或文件流（文本/二进制）。
        parser (str): 使用的解析库，默认 "json"。支持已注册解析器的所有名称。

    返回:
        dict | list | None: 解析成功返回对象，失败返回 None。
    """
    parsers = JsonUtil.get_parsers()
    names = JsonUtil.get_parser_names()

    if parser not in parsers:
        raise ValueError(f"未知解析器 '{parser}'，可选: {list(names)}")

    try:
        # 交给 singledispatch 分发
        return _parse_dispatch(s, parser)
    except Exception as e:
        print(f"[解析失败] 错误信息：{e}")

        # 若是字符串输入，尽量给出片段 + 光标
        pos = getattr(e, "pos", None)
        if pos is None and hasattr(e, "lineno") and hasattr(e, "colno"):
            try:
                # 针对文本流走 load(fp) 时，很多实现给的是 lineno/colno
                if isinstance(s, str):
                    lines = s.splitlines()
                    pos = sum(len(line) + 1 for line in lines[: e.lineno - 1]) + e.colno - 1
            except Exception:
                pos = None

        if pos is not None and isinstance(s, str):
            start = max(0, pos - 25)
            end = min(len(s), pos + 25)
            snippet = s[start:end]
            pointer = " " * (pos - start) + "^"
            print("[错误上下文]")
            print(snippet)
            print(pointer)
        return None
