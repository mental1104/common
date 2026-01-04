"""基准:比较三种分发方式的性能
- 手写 if/elif isinstance 链
- functools.singledispatch
- 自定义 dispatch_for(多参数模式)

数据集包含 (int, int)、(str, str)、(bytes, str) 三类输入,便于观察多参数分发路径。
"""

from __future__ import annotations

from functools import singledispatch

import pytest

from mental1104 import dispatch_for

# -------------------- 基准参数 --------------------
DATASET_CHUNK = 5_000  # 每类入参个数,最终三倍大小
PEDANTIC_ROUNDS = 6
PEDANTIC_ITERATIONS = 1


def _build_dataset(n: int) -> tuple[tuple[object, object], ...]:
    items: list[tuple[object, object]] = []
    for i in range(n):
        items.append((i, i + 1))
        text = f"s{i}"
        items.append((text, f"{text}-tail"))
        items.append((f"bytes{i}".encode(), text))
    return tuple(items)


def _consume(fn, pairs: tuple[tuple[object, object], ...]) -> int:
    """轻量校验:混合 hash,避免被优化掉。"""
    checksum = 0
    for a, b in pairs:
        checksum ^= hash(fn(a, b))
    return checksum


@pytest.fixture(scope="module")
def mixed_args() -> tuple[tuple[object, object], ...]:
    return _build_dataset(DATASET_CHUNK)


def _run_benchmark(benchmark, fn, pairs, expected_checksum: int):
    result = benchmark.pedantic(
        lambda: _consume(fn, pairs),
        iterations=PEDANTIC_ITERATIONS,
        rounds=PEDANTIC_ROUNDS,
    )
    assert result == expected_checksum


# -------------------- 方案 1:if / isinstance 链 --------------------
def _if_dispatch(a, b):
    if isinstance(a, int) and isinstance(b, int):
        return a + b
    if isinstance(a, str) and isinstance(b, str):
        return f"{a}:{b}"
    if isinstance(a, bytes) and isinstance(b, str):
        return f"{a.decode('utf-8')}:{b}"
    raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")


# -------------------- 方案 2:singledispatch --------------------
@singledispatch
def singledispatch_op(a, b):
    raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")


@singledispatch_op.register(int)
def _(a: int, b):
    if not isinstance(b, int):
        raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")
    return a + b


@singledispatch_op.register(str)
def _(a: str, b):
    if not isinstance(b, str):
        raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")
    return f"{a}:{b}"


@singledispatch_op.register(bytes)
def _(a: bytes, b):
    if not isinstance(b, str):
        raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")
    text = a.decode("utf-8")
    return f"{text}:{b}"


# -------------------- 方案 3:dispatch_for(多参数模式) --------------------
def _dispatch_for_entry(*args):
    raise NotImplementedError("Implementation is provided by Impl.")


@dispatch_for(_dispatch_for_entry)
class _DispatchForImpl:
    @dispatch_for(int, int)
    def handle_int_int(self, a: int, b: int):
        return a + b

    @dispatch_for(str, str)
    def handle_str_str(self, a: str, b: str):
        return f"{a}:{b}"

    @dispatch_for(bytes, str)
    def handle_bytes_str(self, a: bytes, b: str):
        text = a.decode("utf-8")
        return self(text, b)  # 复用 (str, str) 路径

    def default(self, *args, **kwargs):
        raise TypeError(f"Unsupported types: {[type(a) for a in args]}")


# class 装饰器会替换模块级符号,取回分发入口
_dispatch_for_entry = _dispatch_for_entry


@pytest.fixture(scope="module")
def baseline_checksum(mixed_args) -> int:
    return _consume(_if_dispatch, mixed_args)


def test_if_chain_baseline(benchmark, mixed_args, baseline_checksum):
    """基准 A:手写 if/elif 链。"""
    _run_benchmark(benchmark, _if_dispatch, mixed_args, baseline_checksum)


def test_singledispatch(benchmark, mixed_args, baseline_checksum):
    """基准 B:functools.singledispatch。"""
    _run_benchmark(benchmark, singledispatch_op, mixed_args, baseline_checksum)


def test_dispatch_for(benchmark, mixed_args, baseline_checksum):
    """基准 C:自定义 dispatch_for,多参数精确匹配。"""
    _run_benchmark(benchmark, _dispatch_for_entry, mixed_args, baseline_checksum)


# -------------------- 较重分支逻辑:更贴近日常代码 --------------------
def _logic_int_int_rich(a: int, b: int) -> int:
    arr = [a, b, a + b]
    arr.extend(range(5))
    mapping = {a: b, b: a + b, "len": len(arr)}
    return sum(arr) + mapping[a] + mapping["len"] + sum(i % 3 for i in arr)


def _logic_str_str_rich(a: str, b: str) -> int:
    joined = f"{a}:{b}"
    pieces = list(joined)
    freq = {}
    for ch in pieces:
        freq[ch] = freq.get(ch, 0) + 1
    return len(joined) + len(pieces) + sum(freq.values()) + len(freq)


def _logic_bytes_str_rich(a: bytes, b: str) -> int:
    text = a.decode("utf-8")
    combo = f"{text}|{b}"
    rev = combo[::-1]
    lookup = {c: i for i, c in enumerate(rev[:5])}
    return len(combo) + len(lookup) + sum(lookup.values()) + rev.count("|")


def _if_dispatch_rich(a, b):
    if isinstance(a, int) and isinstance(b, int):
        return _logic_int_int_rich(a, b)
    if isinstance(a, str) and isinstance(b, str):
        return _logic_str_str_rich(a, b)
    if isinstance(a, bytes) and isinstance(b, str):
        return _logic_bytes_str_rich(a, b)
    raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")


@singledispatch
def singledispatch_op_rich(a, b):
    raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")


@singledispatch_op_rich.register(int)
def _(a: int, b):
    if not isinstance(b, int):
        raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")
    return _logic_int_int_rich(a, b)


@singledispatch_op_rich.register(str)
def _(a: str, b):
    if not isinstance(b, str):
        raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")
    return _logic_str_str_rich(a, b)


@singledispatch_op_rich.register(bytes)
def _(a: bytes, b):
    if not isinstance(b, str):
        raise TypeError(f"Unsupported types: {type(a)}, {type(b)}")
    return _logic_bytes_str_rich(a, b)


def _dispatch_for_entry_rich(*args):
    raise NotImplementedError("Implementation is provided by Impl.")


@dispatch_for(_dispatch_for_entry_rich)
class _DispatchForImplRich:
    @dispatch_for(int, int)
    def handle_int_int(self, a: int, b: int):
        return _logic_int_int_rich(a, b)

    @dispatch_for(str, str)
    def handle_str_str(self, a: str, b: str):
        return _logic_str_str_rich(a, b)

    @dispatch_for(bytes, str)
    def handle_bytes_str(self, a: bytes, b: str):
        return _logic_bytes_str_rich(a, b)

    def default(self, *args, **kwargs):
        raise TypeError(f"Unsupported types: {[type(a) for a in args]}")


_dispatch_for_entry_rich = _dispatch_for_entry_rich


@pytest.fixture(scope="module")
def baseline_checksum_rich(mixed_args) -> int:
    return _consume(_if_dispatch_rich, mixed_args)


def test_if_chain_richer_logic(benchmark, mixed_args, baseline_checksum_rich):
    """基准 A2:手写 if/elif 链,分支内包含列表/字典等常见操作。"""
    _run_benchmark(benchmark, _if_dispatch_rich, mixed_args, baseline_checksum_rich)


def test_singledispatch_richer_logic(benchmark, mixed_args, baseline_checksum_rich):
    """基准 B2:singledispatch,分支内包含常见操作。"""
    _run_benchmark(benchmark, singledispatch_op_rich, mixed_args, baseline_checksum_rich)


def test_dispatch_for_richer_logic(benchmark, mixed_args, baseline_checksum_rich):
    """基准 C2:dispatch_for,分支内包含常见操作。"""
    _run_benchmark(benchmark, _dispatch_for_entry_rich, mixed_args, baseline_checksum_rich)
