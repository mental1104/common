from __future__ import annotations

HELLO = "hello world"


def extract_world(greeting: str | None) -> str | None:
    if greeting is None:
        return None
    needle = "world"
    index = greeting.find(needle)
    if index == -1:
        return None
    return greeting[index : index + len(needle)]
