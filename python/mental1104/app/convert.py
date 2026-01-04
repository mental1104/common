from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import io

from mental1104.utils.parse_json import dump_json, load_json
from mental1104.utils.parse_yaml import dump_yaml, parse_yaml


def json_to_yaml(
    src: str | io.IOBase,
    *,
    in_parser: str = "json",
    out_parser: str = "yaml",
    indent: int = 2,
    sort_keys: bool = False,
    fp: io.IOBase | None = None,
) -> str | None:
    """
    将 JSON（字符串或文件流）转换为 YAML。
    约定：失败一律返回 None, 并打印原因; 成功时返回字符串或写入 fp 后返回 None。
    """
    # 解析阶段
    try:
        obj = load_json(src, parser=in_parser)
    except Exception as e:
        print(f"[转换失败] JSON 解析阶段：{e}")
        return None
    if obj is None:
        # load_json 已打印详细上下文
        return None
    if not isinstance(obj, (dict, list)):
        print("[转换失败] 根类型必须为 dict 或 list（JSON -> YAML）")
        return None

    # 序列化阶段
    try:
        return dump_yaml(obj, fp, parser=out_parser, indent=indent, sort_keys=sort_keys)
    except Exception as e:
        print(f"[转换失败] YAML 序列化阶段：{e}")
        return None


def yaml_to_json(
    src: str | io.IOBase,
    *,
    in_parser: str = "yaml",
    out_parser: str = "json",
    ensure_ascii: bool = True,
    indent: int | None = None,
    fp: io.IOBase | None = None,
) -> str | None:
    """
    将 YAML（字符串或文件流）转换为 JSON。
    约定：失败一律返回 None, 并打印原因; 成功时返回字符串或写入 fp 后返回 None。
    """
    # 解析阶段
    try:
        obj = parse_yaml(src, parser=in_parser)
    except Exception as e:
        print(f"[转换失败] YAML 解析阶段：{e}")
        return None
    if obj is None:
        # parse_yaml 已打印详细上下文
        return None
    if not isinstance(obj, (dict, list)):
        print("[转换失败] 根类型必须为 dict 或 list（YAML -> JSON）")
        return None

    # 序列化阶段
    try:
        return dump_json(obj, fp, parser=out_parser, ensure_ascii=ensure_ascii, indent=indent)
    except Exception as e:
        print(f"[转换失败] JSON 序列化阶段：{e}")
        return None
