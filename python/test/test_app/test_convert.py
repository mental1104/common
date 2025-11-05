# -*- coding: utf-8 -*-
import pytest
from io import StringIO, BytesIO
from contextlib import redirect_stdout

from mental1104 import json_to_yaml, yaml_to_json, JsonUtil, YamlUtil, load_json, parse_yaml

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

invalid_json = '{"a": [1, 2, 3,]}'    # 末尾多逗号
invalid_yaml = "a: [1, 2, 3,,]"       # 连续逗号
obj_simple = {"name": "中文", "age": 25, "tags": ["a", "b"]}


# ==== 成功：字符串 <-> 字符串 ====
@pytest.mark.parametrize("jin", JSON_PARSERS)
@pytest.mark.parametrize("yout", YAML_PARSERS)
def test_json_to_yaml_roundtrip_string(jin, yout):
    y = json_to_yaml(valid_json, in_parser=jin, out_parser=yout)
    assert isinstance(y, str)
    back = parse_yaml(y, parser=yout)
    assert back == obj_simple

@pytest.mark.parametrize("yin", YAML_PARSERS)
@pytest.mark.parametrize("jout", JSON_PARSERS)
def test_yaml_to_json_roundtrip_string(yin, jout):
    s = yaml_to_json(valid_yaml, in_parser=yin, out_parser=jout, ensure_ascii=False, indent=2)
    assert isinstance(s, str)
    back = load_json(s, parser=jout)
    assert back == obj_simple


# ==== 成功：流式输入/输出 ====
@pytest.mark.parametrize("jin", JSON_PARSERS)
@pytest.mark.parametrize("yout", YAML_PARSERS)
def test_json_to_yaml_text_in_text_out(jin, yout):
    ret = json_to_yaml(StringIO(valid_json), in_parser=jin, out_parser=yout)
    assert isinstance(ret, str)
    assert parse_yaml(ret, parser=yout) == obj_simple

@pytest.mark.parametrize("jin", JSON_PARSERS)
@pytest.mark.parametrize("yout", YAML_PARSERS)
def test_json_to_yaml_bin_in_bin_out(jin, yout):
    out = BytesIO()
    ret = json_to_yaml(BytesIO(valid_json.encode("utf-8")), in_parser=jin, out_parser=yout, fp=out)
    assert ret is None
    text = out.getvalue().decode("utf-8")
    assert parse_yaml(text, parser=yout) == obj_simple

@pytest.mark.parametrize("yin", YAML_PARSERS)
@pytest.mark.parametrize("jout", JSON_PARSERS)
def test_yaml_to_json_text_in_text_out(yin, jout):
    ret = yaml_to_json(StringIO(valid_yaml), in_parser=yin, out_parser=jout, ensure_ascii=True)
    assert isinstance(ret, str)
    assert load_json(ret, parser=jout) == obj_simple

@pytest.mark.parametrize("yin", YAML_PARSERS)
@pytest.mark.parametrize("jout", JSON_PARSERS)
def test_yaml_to_json_bin_in_bin_out(yin, jout):
    out = BytesIO()
    ret = yaml_to_json(BytesIO(valid_yaml.encode("utf-8")), in_parser=yin, out_parser=jout, ensure_ascii=False, indent=2, fp=out)
    assert ret is None
    s = out.getvalue().decode("utf-8")
    assert load_json(s, parser=jout) == obj_simple


# ==== 失败：解析阶段返回 None（并打印错误）====
@pytest.mark.parametrize("jin", JSON_PARSERS)
def test_json_to_yaml_invalid_input_returns_none(jin):
    buf = StringIO()
    with redirect_stdout(buf):
        y = json_to_yaml(invalid_json, in_parser=jin)
    assert y is None
    assert "[解析失败]" in buf.getvalue() or "[转换失败]" in buf.getvalue()

@pytest.mark.parametrize("yin", YAML_PARSERS)
def test_yaml_to_json_invalid_input_returns_none(yin):
    buf = StringIO()
    with redirect_stdout(buf):
        s = yaml_to_json(invalid_yaml, in_parser=yin)
    assert s is None
    assert "[解析失败]" in buf.getvalue() or "[转换失败]" in buf.getvalue()


# ==== 失败：未知解析器名 -> 返回 None ====
def test_json_to_yaml_unknown_in_parser_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml(valid_json, in_parser="__bad__")
    assert ret is None

def test_json_to_yaml_unknown_out_parser_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml(valid_json, out_parser="__bad__")
    assert ret is None

def test_yaml_to_json_unknown_in_parser_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json(valid_yaml, in_parser="__bad__")
    assert ret is None

def test_yaml_to_json_unknown_out_parser_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json(valid_yaml, out_parser="__bad__")
    assert ret is None


# ==== 失败：根类型不符 -> 返回 None ====
def test_json_to_yaml_non_mapping_sequence_root_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml("123")
    assert ret is None

def test_yaml_to_json_non_mapping_sequence_root_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json("123")
    assert ret is None


# ==== 失败：输出 fp 非法 -> 返回 None ====
class Dummy:
    pass

def test_json_to_yaml_invalid_fp_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = json_to_yaml(valid_json, fp=Dummy())
    assert ret is None

def test_yaml_to_json_invalid_fp_returns_none():
    buf = StringIO()
    with redirect_stdout(buf):
        ret = yaml_to_json(valid_yaml, fp=Dummy())
    assert ret is None
