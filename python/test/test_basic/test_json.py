import pytest
from mental1104 import parse_json, PARSERS, PARSER_NAMES
from io import StringIO
from contextlib import redirect_stdout

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
    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_valid_json(self, parser_name):
        result = parse_json(valid_json, parser=parser_name)
        assert isinstance(result, dict)
        assert result["name"] == "Espeon"

    @pytest.mark.parametrize("parser_name", PARSER_NAMES)
    def test_invalid_json_returns_none_and_context_if_supported(self, parser_name):
        buffer = StringIO()
        with redirect_stdout(buffer):
            result = parse_json(invalid_json, parser=parser_name)

        output = buffer.getvalue()
        assert result is None
        assert "[解析失败]" in output

        try:
            PARSERS[parser_name](invalid_json)
        except Exception as e:
            if hasattr(e, "pos") or (hasattr(e, "lineno") and hasattr(e, "colno")):
                assert "[错误上下文]" in output
                assert "^" in output
