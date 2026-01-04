from contextlib import redirect_stdout
from io import BytesIO, StringIO

import pytest

from mental1104 import JsonUtil, YamlUtil, json_to_yaml, load_json, parse_yaml, yaml_to_json

JSON_PARSERS = JsonUtil.get_parser_names()
YAML_PARSERS = YamlUtil.get_parser_names()

valid_json = '{"name": "中文", "age": 25, "tags": ["a", "b"]}'
valid_yaml = """\
name: 中文
age: 25
tags:
  - a
  - b
"""

invalid_json = '{"a": [1, 2, 3,]}'  # 末尾多逗号
invalid_yaml = "a: [1, 2, 3,,]"  # 连续逗号
obj_simple = {"name": "中文", "age": 25, "tags": ["a", "b"]}


# ==== 成功：字符串 <-> 字符串 ====
@pytest.mark.parametrize("jin", JSON_PARSERS)
@pytest.mark.parametrize("yout", YAML_PARSERS)
def test_json_to_yaml_roundtrip_string(jin, yout):
    """
    【场景背景】json_to_yaml 应支持字符串输入/输出并保持数据等价。
    【步骤输入】valid_json 字符串, 指定解析/输出 parser。
    【期望输出】返回 YAML 字符串, parse_yaml 后得到 obj_simple。
    """
    y = json_to_yaml(valid_json, in_parser=jin, out_parser=yout)
    assert isinstance(y, str)
    back = parse_yaml(y, parser=yout)
    assert back == obj_simple


@pytest.mark.parametrize("yin", YAML_PARSERS)
@pytest.mark.parametrize("jout", JSON_PARSERS)
def test_yaml_to_json_roundtrip_string(yin, jout):
    """
    【场景背景】yaml_to_json 同样需完成字符串往返。
    【步骤输入】valid_yaml, ensure_ascii=False, indent=2。
    【期望输出】返回 JSON 字符串, load_json 后等于 obj_simple。
    """
    s = yaml_to_json(valid_yaml, in_parser=yin, out_parser=jout, ensure_ascii=False, indent=2)
    assert isinstance(s, str)
    back = load_json(s, parser=jout)
    assert back == obj_simple


# ==== 成功：流式输入/输出 ====
@pytest.mark.parametrize("jin", JSON_PARSERS)
@pytest.mark.parametrize("yout", YAML_PARSERS)
def test_json_to_yaml_text_in_text_out(jin, yout):
    """
    【场景背景】TextIO 输入/输出需兼容。
    【步骤输入】StringIO(valid_json)。
    【期望输出】函数返回字符串并可 parse 成 obj_simple。
    """
    ret = json_to_yaml(StringIO(valid_json), in_parser=jin, out_parser=yout)
    assert isinstance(ret, str)
    assert parse_yaml(ret, parser=yout) == obj_simple


@pytest.mark.parametrize("jin", JSON_PARSERS)
@pytest.mark.parametrize("yout", YAML_PARSERS)
def test_json_to_yaml_bin_in_bin_out(jin, yout):
    """
    【场景背景】二进制输入输出需允许写入 fp。
    【步骤输入】BytesIO(valid_json) + BytesIO fp。
    【期望输出】函数返回 None, fp 内容可 parse 成 obj_simple。
    """
    out = BytesIO()
    ret = json_to_yaml(BytesIO(valid_json.encode("utf-8")), in_parser=jin, out_parser=yout, fp=out)
    assert ret is None
    text = out.getvalue().decode("utf-8")
    assert parse_yaml(text, parser=yout) == obj_simple


@pytest.mark.parametrize("yin", YAML_PARSERS)
@pytest.mark.parametrize("jout", JSON_PARSERS)
def test_yaml_to_json_text_in_text_out(yin, jout):
    """
    【场景背景】TextIO 的 YAML 输入应输出 JSON 字符串。
    【步骤输入】StringIO(valid_yaml)。
    【期望输出】返回字符串, load_json 后等于 obj_simple。
    """
    ret = yaml_to_json(StringIO(valid_yaml), in_parser=yin, out_parser=jout, ensure_ascii=True)
    assert isinstance(ret, str)
    assert load_json(ret, parser=jout) == obj_simple


@pytest.mark.parametrize("yin", YAML_PARSERS)
@pytest.mark.parametrize("jout", JSON_PARSERS)
def test_yaml_to_json_bin_in_bin_out(yin, jout):
    """
    【场景背景】二进制 YAML -> JSON should support streaming fp。
    【步骤输入】BytesIO(valid_yaml) 和输出 BytesIO。
    【期望输出】函数返回 None, 输出流 decode 为 JSON, 再 load_json 得到 obj_simple。
    """
    out = BytesIO()
    ret = yaml_to_json(
        BytesIO(valid_yaml.encode("utf-8")),
        in_parser=yin,
        out_parser=jout,
        ensure_ascii=False,
        indent=2,
        fp=out,
    )
    assert ret is None
    s = out.getvalue().decode("utf-8")
    assert load_json(s, parser=jout) == obj_simple


# ==== 失败：解析阶段返回 None（并打印错误）====
@pytest.mark.parametrize("jin", JSON_PARSERS)
def test_json_to_yaml_invalid_input_returns_none(jin):
    """
    【场景背景】解析阶段失败应返回 None 并打印错误。
    【步骤输入】invalid_json。
    【期望输出】函数返回 None, stdout 含 [解析失败] 或 [转换失败]。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        y = json_to_yaml(invalid_json, in_parser=jin)
    assert y is None
    assert "[解析失败]" in buf.getvalue() or "[转换失败]" in buf.getvalue()


@pytest.mark.parametrize("yin", YAML_PARSERS)
def test_yaml_to_json_invalid_input_returns_none(yin):
    """
    【场景背景】YAML 源解析失败时也应返回 None 并提示。
    【步骤输入】invalid_yaml。
    【期望输出】stdout 包含失败提示。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        s = yaml_to_json(invalid_yaml, in_parser=yin)
    assert s is None
    assert "[解析失败]" in buf.getvalue() or "[转换失败]" in buf.getvalue()


# ==== 失败：未知解析器名 -> 返回 None ====
def test_json_to_yaml_unknown_in_parser_returns_none():
    """
    【场景背景】未知输入解析器名需被拒绝。
    【步骤输入】in_parser="__bad__"。
    【期望输出】函数返回 None 并打印错误（若有）。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml(valid_json, in_parser="__bad__")
    assert ret is None


def test_json_to_yaml_unknown_out_parser_returns_none():
    """
    【场景背景】未知输出解析器同样不可用。
    【步骤输入】out_parser="__bad__"。
    【期望输出】返回 None。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml(valid_json, out_parser="__bad__")
    assert ret is None


def test_yaml_to_json_unknown_in_parser_returns_none():
    """
    【场景背景】yaml_to_json 的输入解析器名必须合法。
    【步骤输入】in_parser="__bad__"。
    【期望输出】返回 None。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json(valid_yaml, in_parser="__bad__")
    assert ret is None


def test_yaml_to_json_unknown_out_parser_returns_none():
    """
    【场景背景】输出解析器无效也应直接返回 None。
    【步骤输入】out_parser="__bad__"。
    【期望输出】函数返回 None。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json(valid_yaml, out_parser="__bad__")
    assert ret is None


# ==== 失败：根类型不符 -> 返回 None ====
def test_json_to_yaml_non_mapping_sequence_root_returns_none():
    """
    【场景背景】转换器要求根为映射, 若传入简单字符串应拒绝。
    【步骤输入】"123"。
    【期望输出】返回 None。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml("123")
    assert ret is None


def test_yaml_to_json_non_mapping_sequence_root_returns_none():
    """
    【场景背景】yaml_to_json 也要求根节点可转换为 JSON 对象。
    【步骤输入】"123"。
    【期望输出】返回 None。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json("123")
    assert ret is None


# ==== 失败：输出 fp 非法 -> 返回 None ====
class Dummy:
    pass


def test_json_to_yaml_invalid_fp_returns_none():
    """
    【场景背景】fp 必须具备写能力。
    【步骤输入】fp=Dummy()。
    【期望输出】函数返回 None, 提示转换失败。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml(valid_json, fp=Dummy())
    assert ret is None


def test_yaml_to_json_invalid_fp_returns_none():
    """
    【场景背景】yaml_to_json 的输出也需要合法文件对象。
    【步骤输入】fp=Dummy()。
    【期望输出】返回 None。
    """
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json(valid_yaml, fp=Dummy())
    assert ret is None
