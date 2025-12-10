from __future__ import annotations
from mental1104 import replace_space_with


class TestStringHelper:
    def test_replace_space_with_default_separator(self):
        """
        【场景背景】默认分隔符应把所有空白替换为“|”并去除多余空格。
        【步骤输入】包含换行/制表/多空格的字符串。
        【期望输出】输出各单词用 | 连接。
        """
        input_string = "This is\n   a test\t string"
        expected_output = "This|is|a|test|string"
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_custom_separator(self):
        """
        【场景背景】调用方可以指定自定义分隔符。
        【步骤输入】separator=","。
        【期望输出】输出单词被逗号拼接。
        """
        input_string = "Another\n  example \t string"
        custom_separator = ","
        expected_output = "Another,example,string"
        assert replace_space_with(input_string, custom_separator) == expected_output

    def test_replace_space_with_empty_string(self):
        """
        【场景背景】空输入应返回空字符串。
        【步骤输入】""。
        【期望输出】函数直接返回 ""。
        """
        input_string = ""
        expected_output = ""
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_no_whitespace(self):
        """
        【场景背景】没有空白时应原样返回。
        【步骤输入】"NoWhitespaceHere"。
        【期望输出】输出与输入一致。
        """
        input_string = "NoWhitespaceHere"
        expected_output = "NoWhitespaceHere"
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_only_whitespace(self):
        """
        【场景背景】纯空白字符串经处理应变为空。
        【步骤输入】"\n  \t  "。
        【期望输出】返回 ""。
        """
        input_string = "\n  \t  "
        expected_output = ""
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_special_characters(self):
        """
        【场景背景】替换空白不能影响其它符号。
        【步骤输入】包含特殊字符的字符串。
        【期望输出】仅在空白处插入“|”，其余字符保持不变。
        """
        input_string = "Special@#Characters\nHere"
        expected_output = "Special@#Characters|Here"
        assert replace_space_with(input_string) == expected_output
