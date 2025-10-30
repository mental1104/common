import pytest
from io import StringIO, BytesIO
from contextlib import redirect_stdout

# 关键：使用既有 API load_json，并从 JsonUtil 取只读视图
from mental1104 import load_json, JsonUtil

PARSERS = JsonUtil.get_parsers()             # MappingProxyType，只读
PARSER_NAMES = JsonUtil.get_parser_names()   # tuple，只读

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


class TestParseJson:
    # ==== 原有：字符串 ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_json_from_str(self, parser_name):
        result = load_json(valid_json, parser=parser_name)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_json_from_str_returns_none_and_context_if_supported(self, parser_name):
        buf = StringIO()
        with redirect_stdout(buf):
            result = load_json(invalid_json, parser=parser_name)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output

        # 仅当底层解析器提供位置信息时，才校验上下文指示
        try:
            PARSERS[parser_name](invalid_json)
        except Exception as e:
            if hasattr(e, "pos") or (hasattr(e, "lineno") and hasattr(e, "colno")):
                assert "[错误上下文]" in output
                assert "^" in output

    # ==== 新增：文本文件流（TextIO） ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_json_from_text_stream(self, parser_name):
        fp = StringIO(valid_json)
        result = load_json(fp, parser=parser_name)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_json_from_text_stream_returns_none(self, parser_name):
        buf = StringIO()
        with redirect_stdout(buf):
            result = load_json(StringIO(invalid_json), parser=parser_name)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output
        # 注意：文件流情况下不强求打印 [错误上下文] 与指针，
        # 不同实现/路径下未必有片段与 ^（尤其 orjson 走 loads 时）。

    # ==== 新增：二进制文件流（BinaryIO） ====
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_json_from_binary_stream(self, parser_name):
        fp = BytesIO(valid_json.encode("utf-8"))
        result = load_json(fp, parser=parser_name)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_json_from_binary_stream_returns_none(self, parser_name):
        buf = StringIO()
        with redirect_stdout(buf):
            result = load_json(BytesIO(invalid_json.encode("utf-8")), parser=parser_name)
        output = buf.getvalue()

        assert result is None
        assert "[解析失败]" in output
        # 同上：BinaryIO 下也不强求上下文片段与 '^'
