from mental1104 import replace_space_with


class TestStringHelper:
    def test_replace_space_with_default_separator(self):
        input_string = "This is\n   a test\t string"
        expected_output = "This|is|a|test|string"
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_custom_separator(self):
        input_string = "Another\n  example \t string"
        custom_separator = ","
        expected_output = "Another,example,string"
        assert replace_space_with(input_string, custom_separator) == expected_output

    def test_replace_space_with_empty_string(self):
        input_string = ""
        expected_output = ""
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_no_whitespace(self):
        input_string = "NoWhitespaceHere"
        expected_output = "NoWhitespaceHere"
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_only_whitespace(self):
        input_string = "\n  \t  "
        expected_output = ""
        assert replace_space_with(input_string) == expected_output

    def test_replace_space_with_special_characters(self):
        input_string = "Special@#Characters\nHere"
        expected_output = "Special@#Characters|Here"
        assert replace_space_with(input_string) == expected_output
