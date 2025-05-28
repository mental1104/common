import pytest
from mental1104 import random_pick


class TestRandomHelper:
    def test_random_from_list(self):
        items = [1, 2, 3, 4]
        result = random_pick(items)
        assert result in items

    def test_random_from_dict(self):
        data = {"a": 1, "b": 2}
        key, value = random_pick(data)
        assert key in data
        assert value == data[key]

    def test_random_from_empty_list_raises(self):
        with pytest.raises(ValueError):
            random_pick([])

    def test_random_from_empty_dict_raises(self):
        with pytest.raises(ValueError):
            random_pick({})
