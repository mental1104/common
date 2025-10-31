import json
import io
import importlib
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


def dump_json(
    obj: Any,
    fp: io.IOBase | None = None,
    *,
    parser: str = "json",
    ensure_ascii: bool = True,
    indent: int | None = None,
) -> str | None:
    """
    通用 JSON 序列化器：
    - 第一个参数仅支持 dict 或 list
    - 未提供 fp：返回字符串
    - 提供 fp（文本/二进制流）：写入 fp 并返回 None
    - 统一兼容 json/ujson/orjson 的 ensure_ascii 与 indent 语义
      * orjson：支持 ensure_ascii；indent 仅支持 2，其他缩进回退到标准库格式
    """
    if not isinstance(obj, (dict, list)):
        raise TypeError("dump_json 的第一个参数必须是 dict 或 list")

    # 非空 fp 必须是标准 IO 类型（提前拦截，避免异常被吞）
    if fp is not None and not isinstance(fp, (io.TextIOBase, io.BufferedIOBase, io.RawIOBase)):
        raise TypeError("fp 必须是文本或二进制文件对象")

    # 校验解析器是否已注册（与 load_json 一致）
    parsers = JsonUtil.get_parsers()
    names = JsonUtil.get_parser_names()
    if parser not in parsers:
        raise ValueError(f"未知解析器 '{parser}'，可选: {list(names)}")

    # 安全导入对应模块（'json'/'ujson'/'orjson'）
    try:
        mod = importlib.import_module(parser)
    except Exception:
        mod = json  # 理论上不会走到；兜底到标准库

    def _write_to_fp(text: str) -> None:
        # 这里不再做类型分支的兜底，前置校验已保证 fp 合法
        assert fp is not None
        if isinstance(fp, io.TextIOBase):
            fp.write(text)
        else:  # BinaryIO
            fp.write(text.encode("utf-8"))

    # --- orjson 特殊适配 ---
    if parser == "orjson":
        try:
            import orjson  # 获取常量
            opts = 0
            if ensure_ascii:
                opts |= orjson.OPT_ESCAPE_UNICODE

            # orjson 仅支持 2 空格缩进；其他缩进为确保语义一致，回退标准库
            if indent is not None and int(indent) != 2:
                text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
                if fp is not None:
                    _write_to_fp(text)
                    return None
                return text

            if indent == 2:
                opts |= orjson.OPT_INDENT_2

            data_bytes = orjson.dumps(obj, option=opts)
            text = data_bytes.decode("utf-8")
            if fp is not None:
                _write_to_fp(text)
                return None
            return text
        except Exception as e:
            print(f"[序列化失败-orjson] 错误信息：{e}")
            return None

    # --- json / ujson 路径 ---
    try:
        dumps = getattr(mod, "dumps", None)
        dump = getattr(mod, "dump", None)

        if fp is None:
            # 返回字符串
            if dumps is None:
                # 极少见：没有 dumps 时回退标准库
                return json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
            try:
                return dumps(obj, ensure_ascii=ensure_ascii, indent=indent)  # type: ignore[arg-type]
            except TypeError:
                # 有些 ujson 老版本不完全支持参数；回退标准库保证语义
                return json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
        else:
            # 写入文件流
            if dump is not None:
                try:
                    dump(obj, fp, ensure_ascii=ensure_ascii, indent=indent)  # type: ignore[arg-type]
                    return None
                except TypeError:
                    # 参数不兼容时，先用标准库 dumps，再统一写入
                    text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
                    _write_to_fp(text)
                    return None
            else:
                # 模块无 dump（极少见），回退标准库生成文本后手写
                text = json.dumps(obj, ensure_ascii=ensure_ascii, indent=indent)
                _write_to_fp(text)
                return None
    except Exception as e:
        print(f"[序列化失败-{parser}] 错误信息：{e}")
        return None
