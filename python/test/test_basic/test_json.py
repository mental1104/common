import pytest
import json
from io import StringIO, BytesIO
from contextlib import redirect_stdout

# 关键：沿用既有入口与只读视图
from mental1104 import load_json, dump_json, JsonUtil, JsonParserType

PARSERS = JsonUtil.get_parsers()             # MappingProxyType，只读
PARSER_TYPES = JsonParserType.available()

invalid_json = '''
{
  "user": {
    "name": "Espeon",
    "age": 25,
    "hobbies": ["coding", "reading", "gaming",],
    "address": {
      "city": "Shenzhen",
      "zip": 518000,
    }
  }
}
'''
valid_json = '{"name": "Espeon", "age": 25}'
obj_simple = {"name": "中文", "age": 25, "tags": ["a", "b"]}


class TestParseJson:
    # ==== 原有：字符串 ====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_valid_json_from_str(self, parser_type):
        result = load_json(valid_json, parser=parser_type)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_invalid_json_from_str_returns_none_and_context_if_supported(self, parser_type):
        buf = StringIO()
        with redirect_stdout(buf):
            result = load_json(invalid_json, parser=parser_type)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output

        # 仅当底层解析器提供位置信息时，才校验上下文指示
        try:
            parser_name = parser_type.value
            PARSERS[parser_name](invalid_json)
        except Exception as e:
            if hasattr(e, "pos") or (hasattr(e, "lineno") and hasattr(e, "colno")):
                assert "[错误上下文]" in output
                assert "^" in output

    # ==== 新增：文本文件流（TextIO） ====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_valid_json_from_text_stream(self, parser_type):
        fp = StringIO(valid_json)
        result = load_json(fp, parser=parser_type)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_invalid_json_from_text_stream_returns_none(self, parser_type):
        buf = StringIO()
        with redirect_stdout(buf):
            result = load_json(StringIO(invalid_json), parser=parser_type)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output
        # 注意：文件流情况下不强求打印 [错误上下文] 与指针，
        # 不同实现/路径下未必有片段与 ^（尤其 orjson 走 loads 时）。

    # ==== 新增：二进制文件流（BinaryIO） ====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_valid_json_from_binary_stream(self, parser_type):
        fp = BytesIO(valid_json.encode("utf-8"))
        result = load_json(fp, parser=parser_type)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_invalid_json_from_binary_stream_returns_none(self, parser_type):
        buf = StringIO()
        with redirect_stdout(buf):
            result = load_json(BytesIO(invalid_json.encode("utf-8")), parser=parser_type)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output
        # 同上：BinaryIO 下也不强求上下文片段与 '^'


class TestDumpJson:
    # ==== 返回字符串（默认 ensure_ascii=True）====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_return_str_default_ensure_ascii_true(self, parser_type):
        s = dump_json(obj_simple, parser=parser_type)
        assert isinstance(s, str)
        # 默认转义非 ASCII
        assert "\\u" in s and "中文" not in s
        # 反序列化应等价
        roundtrip = load_json(s, parser=parser_type)
        assert roundtrip == obj_simple

    # ==== 返回字符串（ensure_ascii=False, 缩进=2）====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_return_str_ensure_ascii_false_with_indent(self, parser_type):
        s = dump_json(obj_simple, parser=parser_type, ensure_ascii=False, indent=2)
        assert isinstance(s, str)
        # 不转义中文 + 存在换行与 2 空格缩进痕迹
        assert "中文" in s
        assert "\n" in s
        assert ('  "name"' in s) or ('  "age"' in s) or ('  "tags"' in s)
        assert load_json(s, parser=parser_type) == obj_simple

    # ==== 写入文本流（TextIO）====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_write_to_text_stream(self, parser_type):
        fp = StringIO()
        ret = dump_json(obj_simple, fp, parser=parser_type, ensure_ascii=False, indent=2)
        assert ret is None
        text = fp.getvalue()
        assert "中文" in text and "\n" in text
        # 从文本再读回
        rt = load_json(text, parser=parser_type)
        assert rt == obj_simple

    # ==== 写入二进制流（BinaryIO）====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_write_to_binary_stream(self, parser_type):
        fp = BytesIO()
        ret = dump_json(obj_simple, fp, parser=parser_type, ensure_ascii=True)
        assert ret is None
        data = fp.getvalue()
        assert isinstance(data, (bytes, bytearray))
        # ensure_ascii=True → 内容应含 \u 转义
        assert b"\\u" in data
        # 从二进制流再读回
        rt = load_json(BytesIO(data), parser=parser_type)
        assert rt == obj_simple

    # ==== 类型校验：非 dict/list 抛出 TypeError ====
    def test_invalid_first_arg_type_raises(self):
        with pytest.raises(TypeError):
            dump_json(("a", "b"))  # 仅支持 dict 或 list

    # ==== 非法解析器名抛出 ValueError ====
    def test_unknown_parser_raises(self):
        with pytest.raises(ValueError):
            dump_json({"a": 1}, parser="__not_exist__")

    # ==== orjson 特性：indent≠2 时应回退到标准库语义 ====
    def test_orjson_indent_not_2_falls_back_to_stdlib(self):
        orjson_parser = next((p for p in PARSER_TYPES if p.value == "orjson"), None)
        if orjson_parser is None:
            pytest.skip("orjson 未安装，跳过该用例")
        obj = {"a": [1, 2, 3], "b": {"x": 1}}
        out = dump_json(obj, parser=orjson_parser, ensure_ascii=False, indent=4)
        expected = json.dumps(obj, ensure_ascii=False, indent=4)
        # 精确一致，表示按预期回退到标准库格式
        assert out == expected

    # ==== ensure_ascii=False + BinaryIO：应写入原生 UTF-8 中文 ====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_binary_stream_with_unicode_no_escape(self, parser_type):
        fp = BytesIO()
        dump_json({"t": "中文"}, fp, parser=parser_type, ensure_ascii=False)
        raw = fp.getvalue()
        text = raw.decode("utf-8")
        assert "中文" in text
        assert "\\u" not in text

    # ==== 非文件句柄对象应报错 ====
    @pytest.mark.parametrize("parser_type", PARSER_TYPES)
    def test_invalid_fp_type_raises(self, parser_type):
        class Dummy:
            pass

        with pytest.raises(TypeError):
            dump_json(obj_simple, Dummy(), parser=parser_type)
