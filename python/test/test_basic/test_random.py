import pytest
from mental1104 import random_pick


class TestRandomHelper:
    def test_random_from_list(self):
        """
        【场景背景】random_pick 应能从非空列表中返回原有元素。
        【步骤输入】传入 [1,2,3,4]。
        【期望输出】返回值属于该列表，证明随机选择逻辑正确。
        """
        items = [1, 2, 3, 4]
        result = random_pick(items)
        assert result in items

    def test_random_from_dict(self):
        """
        【场景背景】当输入字典时，random_pick 应返回 (key,value)。
        【步骤输入】传入 {"a":1,"b":2}。
        【期望输出】key 是字典的键，value 恰为 data[key]。
        """
        data = {"a": 1, "b": 2}
        key, value = random_pick(data)
        assert key in data
        assert value == data[key]

    def test_random_from_empty_list_raises(self):
        """
        【场景背景】空容器不应被允许随机选择。
        【步骤输入】空列表。
        【期望输出】抛 ValueError，提醒调用方处理异常。
        """
        with pytest.raises(ValueError):
            random_pick([])

    def test_random_from_empty_dict_raises(self):
        """
        【场景背景】空字典同样属于非法输入。
        【步骤输入】{}。
        【期望输出】random_pick 抛 ValueError。
        """
        with pytest.raises(ValueError):
            random_pick({})
