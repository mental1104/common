from __future__ import annotations

import asyncio
import random
import string
import time
from typing import Any

__all__ = ["CpuBoundTask", "DatasetFactory", "IoBoundTask"]


class IoBoundTask:
    """Utility helpers for assembling repeatable I/O-bound benchmark tasks."""

    @staticmethod
    async def io_task(
        delay_ms: int,
        payload_len: int,
        *,
        alphabet: str | None = None,
    ) -> int:
        """Simulate an asynchronous I/O task that waits before producing payload data."""

        if delay_ms > 0:
            await asyncio.sleep(delay_ms / 1000.0)
        payload = CpuBoundTask.rand_payload(payload_len, alphabet=alphabet)
        return len(payload)

    @staticmethod
    def blocking_io_task(
        delay_ms: int,
        payload_len: int,
        *,
        alphabet: str | None = None,
    ) -> int:
        """同步版本：直接阻塞当前线程以模拟 I/O 等待。"""

        if delay_ms > 0:
            time.sleep(delay_ms / 1000.0)
        payload = CpuBoundTask.rand_payload(payload_len, alphabet=alphabet)
        return len(payload)


class CpuBoundTask:
    """Helper routines for deterministic CPU-spin workloads."""

    MODULO = 1_000_000_007
    DEFAULT_ALPHABET = string.ascii_lowercase

    @staticmethod
    def spin(iterations: int) -> int:
        """Perform a lightweight deterministic computation to keep the CPU busy."""

        if iterations < 0:
            raise ValueError("iterations 必须为非负整数")
        acc = 0
        for i in range(iterations):
            acc = (acc + i * i) % CpuBoundTask.MODULO
        return acc

    @staticmethod
    def rand_payload(length: int, *, alphabet: str | None = None) -> str:
        """Return a random lowercase payload of the requested length.

        Pure CPU helper placed here so callers can generate deterministic work without
        going through the I/O helper layer.
        """

        if length < 0:
            raise ValueError("length 必须为非负整数")
        pool = alphabet or CpuBoundTask.DEFAULT_ALPHABET
        if not pool:
            raise ValueError("alphabet 不能为空")
        return "".join(random.choices(pool, k=length))


class DatasetFactory:
    """Generate synthetic JSON datasets used across benchmarks."""

    @staticmethod
    def build_json_dataset(
        n_objects: int = 20_000,
        payload_repeat: int = 20,
    ) -> list[dict[str, Any]]:
        """Synthesize a list of dicts with nested structures and mixed data types."""

        if n_objects <= 0:
            raise ValueError("n_objects 必须大于 0")
        if payload_repeat <= 0:
            raise ValueError("payload_repeat 必须大于 0")

        # 混合布尔/数字/中文字符, 覆盖常见 JSON 序列化类型
        heavy_flags = [True, False, None, 123, 45.6, "中文"] * 5
        # 固定序列的整数数组, 便于下游快速断言
        base_values = list(range(40))
        # 构造冗长字符串以放大解析压力
        payload_seed = "Espeon-☯-payload-" + "0123456789" * 5
        payload = payload_seed * payload_repeat

        return [
            {
                "id": idx,
                "name": f"Espeon-{idx}",
                "payload": payload[idx % len(payload) :] + payload[: idx % len(payload)],
                "flags": heavy_flags[idx % len(heavy_flags) :]
                + heavy_flags[: idx % len(heavy_flags)],
                "nested": {
                    "values": base_values,
                    "metrics": [round(j * 0.1234, 5) for j in range(50)],
                    "tags": [f"tag-{idx % 10}-{j}" for j in range(10)],
                },
            }
            for idx in range(n_objects)
        ]
