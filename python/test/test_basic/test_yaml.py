# -*- coding: utf-8 -*-
import pytest
from io import StringIO, BytesIO
from contextlib import redirect_stdout

# 入口：与 JSON 测试保持同风格
from mental1104 import parse_yaml, dump_yaml, YamlUtil

PARSERS = YamlUtil.get_parsers()
PARSER_NAMES = YamlUtil.get_parser_names()

valid_yaml = """\
name: 中文
age: 25
tags:
  - a
  - b
"""

invalid_yaml = """\
user:
  name: Espeon
  age: 25
  address
    city: Shenzhen
"""

obj_simple = {"name": "中文", "age": 25, "tags": ["a", "b"]}


class TestParseYaml:
    # ==== 字符串 OK ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_yaml_from_str(self, parser_name):
        result = parse_yaml(valid_yaml, parser=parser_name)
        assert isinstance(result, dict)
        assert result["name"] == "中文"
        assert result["tags"] == ["a", "b"]

    # ==== 字符串异常：返回 None + 打印错误；若能定位则包含上下文 ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_yaml_from_str_returns_none_and_context_if_supported(self, parser_name):
        buf = StringIO()
        with redirect_stdout(buf):
            result = parse_yaml(invalid_yaml, parser=parser_name)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output

        # 判断当前解析器是否会给出 problem_mark（位置信息）
        try:
            PARSERS[parser_name](invalid_yaml)
        except Exception as e:
            if hasattr(e, "problem_mark"):
                assert "[错误上下文]" in output
                assert "^" in output

    # ==== 文本流 OK ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_yaml_from_text_stream(self, parser_name):
        fp = StringIO(valid_yaml)
        result = parse_yaml(fp, parser=parser_name)
        assert isinstance(result, dict)
        assert result["age"] == 25

    # ==== 文本流异常 ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_yaml_from_text_stream_returns_none(self, parser_name):
        buf = StringIO()
        with redirect_stdout(buf):
            result = parse_yaml(StringIO(invalid_yaml), parser=parser_name)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output
        # 文件流不强制要求片段与指针

    # ==== 二进制流 OK ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_yaml_from_binary_stream(self, parser_name):
        fp = BytesIO(valid_yaml.encode("utf-8"))
        result = parse_yaml(fp, parser=parser_name)
        assert isinstance(result, dict)
        assert result["tags"] == ["a", "b"]

    # ==== 二进制流异常 ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_yaml_from_binary_stream_returns_none(self, parser_name):
        buf = StringIO()
        with redirect_stdout(buf):
            result = parse_yaml(BytesIO(invalid_yaml.encode("utf-8")), parser=parser_name)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output


class TestDumpYaml:
    # ==== 返回字符串（默认 indent=2, sort_keys=False）====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_return_str_default(self, parser_name):
        s = dump_yaml(obj_simple, parser=parser_name)
        assert isinstance(s, str)
        # YAML 为纯文本且默认不转义中文
        assert "中文" in s
        assert "name:" in s
        # 反序列化等价
        roundtrip = parse_yaml(s, parser=parser_name)
        assert roundtrip == obj_simple

    # ==== 写入文本流（TextIO）====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_write_to_text_stream(self, parser_name):
        fp = StringIO()
        ret = dump_yaml(obj_simple, fp, parser=parser_name, indent=2)
        assert ret is None
        text = fp.getvalue()
        assert "中文" in text and "\n" in text
        rt = parse_yaml(text, parser=parser_name)
        assert rt == obj_simple

    # ==== 写入二进制流（BinaryIO）====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_write_to_binary_stream(self, parser_name):
        fp = BytesIO()
        ret = dump_yaml(obj_simple, fp, parser=parser_name)
        assert ret is None
        data = fp.getvalue()
        assert isinstance(data, (bytes, bytearray))
        text = data.decode("utf-8")
        assert "中文" in text
        rt = parse_yaml(BytesIO(data), parser=parser_name)
        assert rt == obj_simple

    # ==== 类型校验 ====
    def test_invalid_first_arg_type_raises(self):
        with pytest.raises(TypeError):
            dump_yaml(("a", "b"))  # 仅支持 dict 或 list

    # ==== 非法解析器名 ====
    def test_unknown_parser_raises(self):
        with pytest.raises(ValueError):
            dump_yaml({"a": 1}, parser="__not_exist__")

    # ==== 非文件句柄对象应报错 ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_fp_type_raises(self, parser_name):
        class Dummy:
            pass
        with pytest.raises(TypeError):
            dump_yaml(obj_simple, Dummy(), parser=parser_name)
