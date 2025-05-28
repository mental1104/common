import re

def replace_space_with(input_string, seperator='|'):
    """
    替换字符串中的空格为指定的分隔符。
    :param input_string: 输入字符串
    :param seperator: 分隔符，默认为 '|'
    :return: 替换后的字符串
    例如：
        >>> StringHelper.replace_space_with("Hello World", "-")
        "Hello-World"
    """
    words = re.findall(r'\S+', input_string)
    return seperator.join(words)