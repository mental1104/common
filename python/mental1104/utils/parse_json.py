from __future__ import annotations
import json
import io
from enum import Enum
from functools import singledispatch
from types import MappingProxyType
from typing import Any, Callable, Mapping

try:
    from export_layer import parse_json as _cpp_parse_json
except Exception:
    _cpp_parse_json = None

__all__ = ["JsonParserType", "JsonUtil", "load_json", "dump_json"]


class JsonParserType(str, Enum):
    """人类可读的 JSON 解析器类型（按功能支持列出）。"""

    JSON = "json"
    UJSON = "ujson"
    ORJSON = "orjson"
    CPP = "cpp"

    @classmethod
    def available(cls) -> tuple["JsonParserType", ...]:
        """返回当前环境可用的解析器枚举成员。"""
        parsers = JsonUtil.get_parser_names()
        return tuple(member for member in cls if member.value in parsers)


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

    if _cpp_parse_json is not None:
        def _cpp_loader(payload: str | bytes) -> Any:
            text = payload
            if isinstance(payload, (bytes, bytearray)):
                text = payload.decode("utf-8", errors="replace")
            elif not isinstance(payload, str):
                text = str(payload)

            ok, err, offset = _cpp_parse_json(text)
            if not ok:
                raise ValueError(f"C++ parse_json failed at offset {offset}: {err}")
            return json.loads(text)

        __parsers["cpp"] = _cpp_loader
        __modules["cpp"] = None

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


def _coerce_parser_name(parser: JsonParserType | str) -> str:
    if isinstance(parser, JsonParserType):
        return parser.value
    if isinstance(parser, str):
        return parser
    raise TypeError("parser 必须是 JsonParserType 或 str")


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


def load_json(s: str | io.IOBase, parser: JsonParserType | str = JsonParserType.JSON):
    """
    通用 JSON 解析器。

    参数:
        s (str|IO): 待解析的 JSON 字符串或文件流（文本/二进制）。
        parser (JsonParserType|str): 使用的解析库，默认 JsonParserType.JSON。

    返回:
        dict | list | None: 解析成功返回对象，失败返回 None。
    """
    parser_name = _coerce_parser_name(parser)
    parsers = JsonUtil.get_parsers()
    names = JsonUtil.get_parser_names()

    if parser_name not in parsers:
        raise ValueError(f"未知解析器 '{parser_name}'，可选: {list(names)}")

    try:
        # 交给 singledispatch 分发
        return _parse_dispatch(s, parser_name)
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


def dump_json(
    obj: Any,
    fp: io.IOBase | None = None,
    *,
    parser: JsonParserType | str = JsonParserType.JSON,
    ensure_ascii: bool = True,
    indent: int | None = None,
) -> str | None:
    """
    通用 JSON 序列化器：
    - 第一个参数仅支持 dict 或 list
    - 未提供 fp：返回字符串；提供 fp（文本/二进制流）：写入 fp 并返回 None
    - 以标准库 json 的 ensure_ascii/indent 语义为准；第三方不支持时自动回退到标准库
      * orjson：支持 ensure_ascii（若缺少 OPT_ESCAPE_UNICODE 则回退标准库）
                仅支持 indent==2（OPT_INDENT_2 存在才用），其他缩进回退标准库
    """
    # 1) 入参与 fp 类型校验
    if not isinstance(obj, (dict, list)):
        raise TypeError("dump_json 的第一个参数必须是 dict 或 list")
    if fp is not None and not isinstance(fp, (io.TextIOBase, io.BufferedIOBase, io.RawIOBase)):
        raise TypeError("fp 必须是文本或二进制文件对象")

    # 2) 解析器合法性（与 load_json 保持一致）
    parser_name = _coerce_parser_name(parser)
    parsers = JsonUtil.get_parsers()
    names = JsonUtil.get_parser_names()
    if parser_name not in parsers:
        raise ValueError(f"未知解析器 '{parser_name}'，可选: {list(names)}")

    # 3) 工具函数：统一写入
    def _write_to_fp(text: str) -> None:
        assert fp is not None
        if isinstance(fp, io.TextIOBase):
            fp.write(text)
        else:
            fp.write(text.encode("utf-8"))

    # 4) orjson 特殊适配（含能力探测与回退）
    if parser_name == "orjson":
        try:
            import orjson
            has_escape = hasattr(orjson, "OPT_ESCAPE_UNICODE")
            has_indent2 = hasattr(orjson, "OPT_INDENT_2")

            # (a) ensure_ascii：旧版 orjson 可能没有 OPT_ESCAPE_UNICODE → 回退标准库
            if ensure_ascii and not has_escape:
                text = json.dumps(obj, ensure_ascii=True, indent=indent)
                if fp is not None:
                    _write_to_fp(text)
                    return None
                return text

            # (b) indent：orjson 仅在 indent==2 且有 OPT_INDENT_2 时支持；否则回退标准库
            if indent is not None and int(indent) != 0:
                if not (int(indent) == 2 and has_indent2):
                    text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
                    if fp is not None:
                        _write_to_fp(text)
                        return None
                    return text

            # (c) 走 orjson 本体
            opts = 0
            if ensure_ascii and has_escape:
                opts |= orjson.OPT_ESCAPE_UNICODE
            if indent == 2 and has_indent2:
                opts |= orjson.OPT_INDENT_2

            data = orjson.dumps(obj, option=opts)
            text = data.decode("utf-8")
            if fp is not None:
                _write_to_fp(text)
                return None
            return text

        except Exception as e:
            # 极端情况下，打印日志并回退标准库，避免返回 None 破坏语义
            print(f"[序列化失败-orjson] 错误信息：{e}")
            text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
            if fp is not None:
                _write_to_fp(text)
                return None
            return text

    # 5) 标准库 json / ujson：优先调用其自身 API，不兼容时回退标准库
    try:
        mod = importlib.import_module(parser_name)  # "json" 或 "ujson"
    except Exception:
        mod = json  # 理论上不会到达

    dumps = getattr(mod, "dumps", None)
    dump = getattr(mod, "dump", None)

    if fp is None:
        if dumps is None:
            return json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
        try:
            return dumps(obj, ensure_ascii=ensure_ascii, indent=indent)  # type: ignore[arg-type]
        except TypeError:
            return json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
    else:
        if dump is not None:
            try:
                dump(obj, fp, ensure_ascii=ensure_ascii, indent=indent)  # type: ignore[arg-type]
                return None
            except TypeError:
                text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
                _write_to_fp(text)
                return None
        else:
            text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
            _write_to_fp(text)
            return None
