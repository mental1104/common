# -*- coding: utf-8 -*-
"""使用 pytest-benchmark 对各 JSON 解析器后端执行 load_json 的性能基准。

特性：
- 自动发现 JsonParserType.available() 中的所有解析器（新增第三方库即可被纳入基准）。
- 构造包含 2 万个对象的 JSON 数据集，叠加多次重复解析，保证运行时长≈半分钟，能更直观地区分不同库性能。
"""
from __future__ import annotations

import io
import json
from typing import Callable, Iterable

import pytest

from mental1104 import JsonParserType, load_json
from mental1104.utils.bench_tasks import DatasetFactory
try:
    from export_layer import get_active_strategy_name
except Exception:
    get_active_strategy_name = None

# -------------------- 基准参数，可按需调整 --------------------
OBJECT_COUNT = 20_000
PAYLOAD_REPEAT = 20
INNER_REPEAT = 4          # 单次基准内重复解析次数
PEDANTIC_ROUNDS = 6       # pytest-benchmark pedantic rounds（配合大数据集，整套基准≈30s）
PEDANTIC_ITERATIONS = 1

# 更大数据集，用于凸显 orjson 在重度 CPU 场景下的优势
HEAVY_OBJECT_COUNT = 100_000
HEAVY_PAYLOAD_REPEAT = 30
HEAVY_INNER_REPEAT = 2
HEAVY_ROUNDS = 4


@pytest.fixture(scope="module")
def big_json_text() -> str:
    dataset = DatasetFactory.build_json_dataset(OBJECT_COUNT, PAYLOAD_REPEAT)
    return json.dumps(dataset)


@pytest.fixture(scope="module")
def big_json_bytes(big_json_text: str) -> bytes:
    return big_json_text.encode("utf-8")


@pytest.fixture(scope="module")
def huge_json_text() -> str:
    dataset = DatasetFactory.build_json_dataset(HEAVY_OBJECT_COUNT, HEAVY_PAYLOAD_REPEAT)
    return json.dumps(dataset)


@pytest.fixture(scope="module")
def huge_json_bytes(huge_json_text: str) -> bytes:
    return huge_json_text.encode("utf-8")


def _parser_params() -> Iterable[pytest.ParamSpec]:  # type: ignore[type-arg]
    available = JsonParserType.available()
    if not available:
        available = (JsonParserType.JSON,)
    for parser in available:
        yield pytest.param(parser, id=parser.value)


def _run_benchmark(
    benchmark,
    parser_type: JsonParserType,
    make_input: Callable[[], object],
    *,
    object_count: int = OBJECT_COUNT,
    inner_repeat: int = INNER_REPEAT,
    rounds: int = PEDANTIC_ROUNDS,
):
    if parser_type == JsonParserType.CPP and get_active_strategy_name is not None:
        try:
            benchmark.extra_info["export_backend"] = get_active_strategy_name()
        except Exception:
            pass

    def _parse_many() -> list[dict[str, object]]:
        last: list[dict[str, object]] | None = None
        for _ in range(inner_repeat):
            data = load_json(make_input(), parser=parser_type)
            assert isinstance(data, list)
            assert len(data) == object_count
            last = data
        assert last is not None
        # 额外校验头尾，确保流式场景读取正确
        assert last[0]["id"] == 0
        assert last[-1]["id"] == object_count - 1
        return last

    return benchmark.pedantic(
        _parse_many,
        iterations=PEDANTIC_ITERATIONS,
        rounds=rounds,
    )


@pytest.mark.parametrize("parser_type", list(_parser_params()))
def test_load_json_from_str(parser_type: JsonParserType, benchmark, big_json_text: str):
    result = _run_benchmark(
        benchmark,
        parser_type,
        make_input=lambda: big_json_text,
    )
    assert result[-1]["id"] == OBJECT_COUNT - 1


@pytest.mark.parametrize("parser_type", list(_parser_params()))
def test_load_json_from_text_stream(parser_type: JsonParserType, benchmark, big_json_text: str):
    result = _run_benchmark(
        benchmark,
        parser_type,
        make_input=lambda: io.StringIO(big_json_text),
    )
    assert result[0]["payload"]


@pytest.mark.parametrize("parser_type", list(_parser_params()))
def test_load_json_from_binary_stream(parser_type: JsonParserType, benchmark, big_json_bytes: bytes):
    result = _run_benchmark(
        benchmark,
        parser_type,
        make_input=lambda: io.BytesIO(big_json_bytes),
    )
    assert isinstance(result[10], dict)


@pytest.mark.parametrize("parser_type", list(_parser_params()))
def test_load_json_huge_binary_stream(
    parser_type: JsonParserType,
    benchmark,
    huge_json_bytes: bytes,
):
    result = _run_benchmark(
        benchmark,
        parser_type,
        make_input=lambda: io.BytesIO(huge_json_bytes),
        object_count=HEAVY_OBJECT_COUNT,
        inner_repeat=HEAVY_INNER_REPEAT,
        rounds=HEAVY_ROUNDS,
    )
    assert result[-1]["id"] == HEAVY_OBJECT_COUNT - 1


def test_native_json_loads_str(benchmark, big_json_text: str):
    """基准：直接调用 json.loads（绕过 load_json 封装）作为对照组。"""

    def _parse_many():
        last: list[dict[str, object]] | None = None
        for _ in range(INNER_REPEAT):
            data = json.loads(big_json_text)
            assert isinstance(data, list)
            assert len(data) == OBJECT_COUNT
            last = data
        assert last is not None
        assert last[0]["id"] == 0
        assert last[-1]["id"] == OBJECT_COUNT - 1
        return last

    result = benchmark.pedantic(
        _parse_many,
        iterations=PEDANTIC_ITERATIONS,
        rounds=PEDANTIC_ROUNDS,
    )
    assert result[5]["name"].startswith("Espeon")
