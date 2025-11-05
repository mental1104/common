# -*- coding: utf-8 -*-
import io
import json
import importlib
from functools import singledispatch
from types import MappingProxyType
from typing import Any, Callable, Mapping


class YamlUtil:
    """
    私有封装 YAML 解析/序列化相关能力，提供只读视图与统一入口。
    已内建支持：
      - "yaml"   -> PyYAML（pip 包名：pyyaml）
      - "ruamel" -> ruamel.yaml（pip 包名：ruamel.yaml）
    """
    __parsers: dict[str, Callable[[str], Any]] = {}
    __modules: dict[str, Any] = {}

    # —— 注册 PyYAML（优先推荐）——
    try:
        import yaml as _pyyaml
        __parsers["yaml"] = _pyyaml.safe_load  # loads
        __modules["yaml"] = _pyyaml
    except Exception:
        pass

    # —— 注册 ruamel.yaml（可选）——
    try:
        import ruamel.yaml as _ruamel_yaml

        def _ruamel_load(s: str) -> Any:
            # ruamel 的 API 面向对象，这里包装成 loads 形式
            from ruamel.yaml import YAML
            y = YAML(typ="safe")
            return y.load(s)

        __parsers["ruamel"] = _ruamel_load
        __modules["ruamel"] = _ruamel_yaml
    except Exception:
        pass

    __parser_names: tuple[str, ...] = tuple(__parsers.keys())

    @staticmethod
    def get_parsers() -> Mapping[str, Callable[[str], Any]]:
        return MappingProxyType(YamlUtil.__parsers)

    @staticmethod
    def get_parser_names() -> tuple[str, ...]:
        return YamlUtil.__parser_names

    @staticmethod
    def _parse_from_file(fp: io.IOBase, parser: str) -> Any:
        """
        与 JSON 实现保持一致：统一从文件对象读取后走 loads。
        （TextIO/BufferedIO/RawIO 均支持；二进制流按 UTF-8 解码）
        """
        loads = YamlUtil.__parsers[parser]
        data = fp.read()
        if isinstance(data, (bytes, bytearray)):
            data = data.decode("utf-8", errors="replace")
        elif not isinstance(data, str):
            data = str(data)
        return loads(data)


# ========= 使用 singledispatch 分发（字符串 / 文件流）=========
@singledispatch
def _yaml_dispatch(value: object, parser: str) -> Any:
    raise TypeError(f"不支持的入参类型: {type(value)!r}（期望 str 或文件流）")


@_yaml_dispatch.register(str)
def _(s: str, parser: str) -> Any:
    parsers = YamlUtil.get_parsers()
    return parsers[parser](s)


@_yaml_dispatch.register(io.TextIOBase)
def _(fp: io.TextIOBase, parser: str) -> Any:
    return YamlUtil._parse_from_file(fp, parser)


@_yaml_dispatch.register(io.BufferedIOBase)
def _(fp: io.BufferedIOBase, parser: str) -> Any:
    return YamlUtil._parse_from_file(fp, parser)


@_yaml_dispatch.register(io.RawIOBase)
def _(fp: io.RawIOBase, parser: str) -> Any:
    return YamlUtil._parse_from_file(fp, parser)


def parse_yaml(s: str | io.IOBase, parser: str = "yaml") -> Any | None:
    """
    通用 YAML 反序列化入口。
    - 支持 str / 文本流 / 二进制流
    - parser: "yaml"(PyYAML) / "ruamel"(ruamel.yaml)
    失败时打印错误与上下文（若能定位），并返回 None。
    """
    parsers = YamlUtil.get_parsers()
    names = YamlUtil.get_parser_names()

    if parser not in parsers:
        raise ValueError(f"未知解析器 '{parser}'，可选: {list(names)}")

    try:
        return _yaml_dispatch(s, parser)
    except Exception as e:
        print(f"[解析失败] 错误信息：{e}")

        # PyYAML/ruamel.yaml 常见：异常含 problem_mark(line/column)，0-based
        mark = getattr(e, "problem_mark", None)
        pos = None
        if mark is not None and isinstance(s, str):
            try:
                line = getattr(mark, "line", None)
                col = getattr(mark, "column", None)
                if isinstance(line, int) and isinstance(col, int) and line >= 0 and col >= 0:
                    lines = s.splitlines(True)  # 保留换行
                    if line < len(lines):
                        pos = sum(len(lines[i]) for i in range(line)) + col
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


def dump_yaml(
    obj: Any,
    fp: io.IOBase | None = None,
    *,
    parser: str = "yaml",
    indent: int = 2,
    sort_keys: bool = False,
) -> str | None:
    """
    通用 YAML 序列化入口：
    - 仅支持 dict/list
    - 未提供 fp：返回字符串；提供 fp（文本/二进制流）：写入 fp 并返回 None
    - parser: "yaml" (PyYAML) / "ruamel" (ruamel.yaml)
    - indent 默认 2；sort_keys 控制键排序
    - 失败时回退：优先 PyYAML → 最终 JSON（UTF-8，无 ASCII 转义）
    """
    # 1) 类型校验
    if not isinstance(obj, (dict, list)):
        raise TypeError("dump_yaml 的第一个参数必须是 dict 或 list")
    if fp is not None and not isinstance(fp, (io.TextIOBase, io.BufferedIOBase, io.RawIOBase)):
        raise TypeError("fp 必须是文本或二进制文件对象")

    # 2) 解析器名校验（与 dump_json/parse_yaml 对齐）
    parsers = YamlUtil.get_parsers()
    names = YamlUtil.get_parser_names()
    if parser not in parsers:
        raise ValueError(f"未知解析器 '{parser}'，可选: {list(names)}")

    # 3) 写入辅助
    def _write_to_fp(text: str) -> None:
        assert fp is not None
        if isinstance(fp, io.TextIOBase):
            fp.write(text)
        else:
            fp.write(text.encode("utf-8"))

    # 4) ruamel.yaml 分支
    if parser == "ruamel":
        try:
            from ruamel.yaml import YAML
            y = YAML(typ="safe")
            y.indent(mapping=indent, sequence=indent, offset=0)
            y.default_flow_style = False

            if fp is None:
                buf = io.StringIO()
                y.dump(obj, buf)
                out = buf.getvalue()
                if sort_keys:
                    # ruamel 的排序开关不稳定：按需用 PyYAML 生成稳定输出
                    import yaml as _pyyaml
                    out = _pyyaml.safe_dump(obj, allow_unicode=True, indent=indent, sort_keys=True)
                return out
            else:
                if isinstance(fp, io.TextIOBase):
                    if sort_keys:
                        import yaml as _pyyaml
                        _pyyaml.safe_dump(obj, fp, allow_unicode=True, indent=indent, sort_keys=True)
                    else:
                        y.dump(obj, fp)
                else:
                    buf = io.StringIO()
                    if sort_keys:
                        import yaml as _pyyaml
                        _pyyaml.safe_dump(obj, buf, allow_unicode=True, indent=indent, sort_keys=True)
                    else:
                        y.dump(obj, buf)
                    _write_to_fp(buf.getvalue())
                return None

        except Exception as e:
            print(f"[序列化失败-yaml-ruamel] 错误信息：{e}")
            # 回退 PyYAML → 再不行回退 JSON
            try:
                import yaml as _pyyaml
                text = _pyyaml.safe_dump(obj, allow_unicode=True, indent=indent, sort_keys=sort_keys)
            except Exception:
                text = json.dumps(obj, ensure_ascii=False, indent=indent)
            if fp is not None:
                _write_to_fp(text)
                return None
            return text

    # 5) PyYAML 分支（默认）
    try:
        import yaml as _pyyaml
        if fp is None:
            return _pyyaml.safe_dump(obj, allow_unicode=True, indent=indent, sort_keys=sort_keys)
        else:
            if isinstance(fp, io.TextIOBase):
                _pyyaml.safe_dump(obj, fp, allow_unicode=True, indent=indent, sort_keys=sort_keys)
            else:
                text = _pyyaml.safe_dump(obj, allow_unicode=True, indent=indent, sort_keys=sort_keys)
                _write_to_fp(text)
            return None
    except Exception as e:
        print(f"[序列化失败-yaml] 错误信息：{e}")
        # 最终兜底：JSON（不转义中文）
        text = json.dumps(obj, ensure_ascii=False, indent=indent)
        if fp is not None:
            _write_to_fp(text)
            return None
        return text
