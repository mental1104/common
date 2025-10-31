import random
from functools import singledispatch


@singledispatch
def random_pick(data):
    raise NotImplementedError(f"random_pick not implemented for type {type(data).__name__}")

@random_pick.register
def _(data: list):
    """
    从列表中随机选择一个元素。
    如果列表为空，则抛出 ValueError。
    Args:
        data (list): 要选择的列表。
    Returns:
        object: 随机选择的元素。
    Raises:
        ValueError: 如果列表为空。
    """
    if not data:
        raise ValueError("Cannot choose from an empty list.")
    return random.choice(data)


@random_pick.register
def _(data: dict):
    """
    从字典中随机选择一个键值对。
    如果字典为空，则抛出 ValueError。
    Args:
        data (dict): 要选择的字典。
    Returns:
        tuple: 随机选择的键和值。
    Raises:
        ValueError: 如果字典为空。
    """
    if not data:
        raise ValueError("Cannot choose from an empty dictionary.")
    key = random.choice(list(data.keys()))
    return key, data[key]
