from __future__ import annotations

import re


def replace_space_with(input_string, seperator="|"):
    """
    分割函数
    替换字符串中的空格为指定的分隔符。
    :param input_string: 输入字符串
    :param seperator: 分隔符, 默认为 '|'
    :return: 替换后的字符串
    例如：
        >>> StringHelper.replace_space_with("Hello World", "-")
        "Hello-World"
    """
    words = re.findall(r"\S+", input_string)
    return seperator.join(words)


def insert_newlines(s, max_line_length):
    """
    按最大行宽向字符串中插入换行符。
    :param s: 原始字符串
    :param max_line_length: 每行允许的最大字符数
    :return: 插入换行符后的新字符串
    例如：
        >>> insert_newlines("这是一个测试字符串, 我们希望在这个字符串中插入换行符, 以确保每行的长度不会太长。", 10)
        "这是一个测试字符串, \\n我们希望在这个字符串中\\n插入换行符, 以确保每行\\n的长度不会太长。"
    """
    # 初始化变量
    last_newline_pos = -1  # 上一个换行符的位置
    output = []  # 输出字符串的列表形式, 用于高效拼接

    # 遍历字符串
    for current_pos, char in enumerate(s):
        if char == "\n":
            # 如果当前字符是换行符, 重置上一个换行符的位置
            last_newline_pos = current_pos
        elif current_pos - last_newline_pos > max_line_length:
            # 如果当前位置与上一个换行符的距离超过了最大行长度
            # 插入换行符, 并重置上一个换行符的位置
            output.append("\n")
            last_newline_pos = current_pos
        # 将当前字符添加到输出列表中
        output.append(char)
    # 将输出列表转换为字符串并返回
    return "".join(output)
