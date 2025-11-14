"""Benchmark plotting helpers with typed result suites."""
from __future__ import annotations

import json
import math
import re
from abc import ABC, abstractmethod
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

import matplotlib.pyplot as plt


class BenchTestType(str):
    """基准框架类型常量，避免各处硬编码字符串。"""

    PYTEST_BENCHMARK = "pytest-benchmark"
    GOOGLE_BENCHMARK = "google-benchmark"


_TIME_UNIT_TO_SECONDS = {
    "ns": 1e-9,
    "us": 1e-6,
    "µs": 1e-6,
    "ms": 1e-3,
    "s": 1.0,
}


@dataclass(slots=True)
class BenchmarkRecord:
    label: str
    metrics: dict[str, float]
    meta: dict[str, Any] = field(default_factory=dict)


class BenchmarkSuite(ABC):
    """统一的基准数据容器，负责暴露 records/metrics 等基础能力。"""
    test_type: BenchTestType

    def __init__(self, records: Sequence[BenchmarkRecord]):
        self._records = list(records)

    @property
    def records(self) -> Sequence[BenchmarkRecord]:
        return tuple(self._records)

    def available_metrics(self) -> list[str]:
        metrics: set[str] = set()
        for rec in self._records:
            metrics.update(rec.metrics.keys())
        return sorted(metrics)


class PytestBenchmarkSuite(BenchmarkSuite):
    """针对 pytest-benchmark 的解析逻辑，将统计字段映射到统一指标。"""

    test_type = BenchTestType.PYTEST_BENCHMARK

    @classmethod
    def from_payload(cls, payload: Any) -> PytestBenchmarkSuite:
        records: list[BenchmarkRecord] = []
        for row in payload.get("benchmarks", []):
            stats = row.get("stats", {})
            metrics: dict[str, float] = {}
            for field in ("min", "max", "mean", "median", "stddev", "iqr"):
                value = stats.get(field)
                if value is not None:
                    metrics[f"{field}_ms"] = float(value) * 1000.0
            if "ops" in stats:
                metrics["ops"] = float(stats["ops"])
            for fallback in ("median_ms", "mean_ms", "min_ms"):
                if fallback in metrics:
                    metrics["real_time_ms"] = metrics[fallback]
                    break
            meta = {
                "group": row.get("group"),
                "params": row.get("params"),
                "rounds": stats.get("rounds"),
                "iterations": stats.get("iterations"),
                "case": row.get("fullname") or row.get("name"),
            }
            records.append(BenchmarkRecord(label=meta["case"] or "unknown", metrics=metrics, meta=meta))
        return cls(records)


class GoogleBenchmarkSuite(BenchmarkSuite):
    """针对 Google Benchmark JSON 的解析器，自动拆解名称中的参数/统计信息。"""

    test_type = BenchTestType.GOOGLE_BENCHMARK

    @classmethod
    def from_payload(cls, payload: Any, *, include_aggregates: bool = False) -> GoogleBenchmarkSuite:
        records: list[BenchmarkRecord] = []
        for row in payload.get("benchmarks", []):
            if not include_aggregates and row.get("aggregate_name"):
                continue
            unit = row.get("time_unit", "ns")
            unit_scale = _TIME_UNIT_TO_SECONDS.get(unit, 1e-9)
            metrics = {
                "real_time_ms": row.get("real_time", 0.0) * unit_scale * 1000.0,
                "cpu_time_ms": row.get("cpu_time", 0.0) * unit_scale * 1000.0,
            }
            for key in ("bytes_per_second", "items_per_second"):
                if key in row:
                    metrics[key] = float(row[key])
            meta = cls._parse_name(row.get("name", "unknown"))
            meta.update(
                {
                    "iterations": row.get("iterations"),
                    "thread_index": row.get("thread_index"),
                    "aggregate_name": row.get("aggregate_name"),
                }
            )
            records.append(BenchmarkRecord(label=meta["case"], metrics=metrics, meta=meta))
        return cls(records)

    @staticmethod
    def _parse_name(name: str) -> dict[str, Any]:
        prefix, rest = (name.split("/", 1) + [""])[:2]
        stat = None
        arg = rest
        if "_" in rest:
            *arg_parts, last = rest.split("_")
            if last in {"mean", "median", "stddev", "cv", "min", "max"}:
                stat = last
                arg = "_".join(arg_parts)
        variant = None
        suite = prefix
        if "_" in prefix:
            suite, variant = prefix.split("_", 1)
        return {
            "suite": suite,
            "variant": variant or suite,
            "arg": arg or "default",
            "stat": stat,
            "case": name,
            "name_raw": name,
        }


# 基于类型的工厂映射，供 CLI / plotter 统一构造 suite
SUITE_FACTORIES: dict[BenchTestType, Callable[..., BenchmarkSuite]] = {
    BenchTestType.PYTEST_BENCHMARK: PytestBenchmarkSuite.from_payload,
    BenchTestType.GOOGLE_BENCHMARK: GoogleBenchmarkSuite.from_payload,
}


def load_benchmark_suite(
    *,
    test_type: BenchTestType,
    result_path: str | Path | None = None,
    result_data: Any | None = None,
    include_aggregates: bool = False,
) -> BenchmarkSuite:
    """从文件或内存对象加载 suite，外部调用只需指定类型即可。"""
    if result_data is None and result_path is None:
        raise ValueError("需要提供 result_data 或 result_path。")
    if result_data is None:
        text = Path(result_path).read_text(encoding="utf-8")
        result_data = json.loads(text)
    factory = SUITE_FACTORIES[test_type]
    kwargs = {}
    if test_type == BenchTestType.GOOGLE_BENCHMARK:
        kwargs["include_aggregates"] = include_aggregates
    return factory(result_data, **kwargs)


class BenchmarkPlotter:
    """负责把 BenchmarkSuite 渲染成各种图表的轻量工具类。"""

    def __init__(self, suite: BenchmarkSuite, *, result_path: str | Path | None = None):
        self.suite = suite
        self.result_path = Path(result_path) if result_path else None

    @property
    def records(self) -> Sequence[BenchmarkRecord]:
        return self.suite.records

    def available_metrics(self) -> list[str]:
        return self.suite.available_metrics()

    # ------------------------------------------------------------------ charts
    def plot_ranking(
        self,
        metric: str,
        *,
        output_path: str | Path | None = None,
        title: str | None = None,
        top_n: int | None = None,
        ascending: bool | None = None,
    ) -> Path:
        if metric not in self.available_metrics():
            raise ValueError(f"未知的 metric: {metric}")
        rows = [(rec.label, rec.metrics[metric]) for rec in self.records if metric in rec.metrics]
        if not rows:
            raise ValueError(f"没有记录包含 {metric}")
        if ascending is None:
            ascending = metric.endswith("_ms") or metric.endswith("_s")
        rows.sort(key=lambda item: item[1], reverse=not ascending)
        if top_n:
            rows = rows[:top_n]
        labels = [_format_label(lbl) for lbl, _ in rows]
        values = [val for _, val in rows]
        fig, ax = plt.subplots(figsize=(max(6.5, len(rows) * 0.55), 4.6))
        fig.patch.set_facecolor("white")
        ax.set_facecolor("#F9FAFB")
        bars = ax.bar(range(len(values)), values, width=0.32, color=_color(0))
        ax.set_xticks(range(len(values)))
        ax.set_xticklabels(labels, rotation=45, ha="right")
        ax.set_ylabel(metric)
        ax.set_title(title or f"{self.suite.test_type} · {metric}")
        ax.grid(axis="y", linestyle="--", alpha=0.4)
        target = min(values) if ascending else max(values)
        for idx, bar in enumerate(bars):
            val = values[idx]
            weight = "bold" if math.isclose(val, target, rel_tol=1e-9) else "normal"
            ax.text(
                bar.get_x() + bar.get_width() / 2.0,
                bar.get_height(),
                f"{val:.3g}",
                ha="center",
                va="bottom",
                fontsize=8,
                fontweight=weight,
            )
        output = Path(output_path) if output_path else self._default_output_path(metric)
        output.parent.mkdir(parents=True, exist_ok=True)
        fig.tight_layout()
        fig.savefig(output)
        plt.close(fig)
        return output

    def plot_case_matrix(
        self,
        metrics: Sequence[str] | None = None,
        *,
        output_path: str | Path | None = None,
        title: str | None = None,
        sort_by: str | None = None,
        top_n: int | None = None,
        ascending: bool | None = None,
    ) -> Path:
        metric_list = list(metrics) if metrics else self._default_metric_list()
        if not metric_list:
            raise ValueError("没有可用指标。")
        order_metric = sort_by or metric_list[0]
        if ascending is None:
            ascending = order_metric.endswith("_ms") or order_metric.endswith("_s")
        usable = [rec for rec in self.records if any(m in rec.metrics for m in metric_list)]
        usable.sort(
            key=lambda rec: rec.metrics.get(order_metric, math.inf if ascending else -math.inf),
            reverse=not ascending,
        )
        if top_n:
            usable = usable[:top_n]
        labels = [_format_label(rec.label) for rec in usable]
        height = max(3.2, 0.55 * len(usable) * len(metric_list) + 1.4)
        fig, axes = plt.subplots(len(metric_list), 1, sharex=False, figsize=(8.5, height))
        fig.patch.set_facecolor("white")
        if not isinstance(axes, Iterable):
            axes = (axes,)
        for ax, metric in zip(axes, metric_list):
            values = [rec.metrics.get(metric, math.nan) for rec in usable]
            y_pos = list(range(len(values)))
            bars = ax.barh(
                y_pos,
                values,
                align="center",
                color=_color(metric_list.index(metric)),
                alpha=0.85,
                height=0.45,
            )
            ax.set_facecolor("#F9FAFB")
            ax.set_yticks(y_pos)
            ax.set_yticklabels(labels, fontsize=8)
            ax.invert_yaxis()
            ax.set_xlabel(metric)
            ax.grid(axis="x", linestyle="--", alpha=0.35)
            finite = [val for val in values if math.isfinite(val)]
            target = (min if ascending else max)(finite) if finite else None
            for bar, value in zip(bars, values):
                if not math.isfinite(value):
                    continue
                weight = "bold" if target is not None and math.isclose(value, target, rel_tol=1e-9) else "normal"
                ax.text(
                    value,
                    bar.get_y() + bar.get_height() / 2,
                    f"{value:.3g}",
                    va="center",
                    ha="left",
                    fontsize=8,
                    fontweight=weight,
                )
        fig.suptitle(title or f"{self.suite.test_type} · 案例矩阵")
        fig.tight_layout()
        output = Path(output_path) if output_path else self._default_output_path(f"{order_metric}_matrix")
        output.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(output)
        plt.close(fig)
        return output

    def plot_comparison(
        self,
        metric: str,
        *,
        group_field: str,
        variant_field: str,
        filters: dict[str, str] | None = None,
        title: str | None = None,
        output_path: str | Path | None = None,
    ) -> Path:
        applicable = []
        for rec in self.records:
            if metric not in rec.metrics:
                continue
            if not _match_filters(rec, filters):
                continue
            group = rec.meta.get(group_field)
            variant = rec.meta.get(variant_field)
            if group is None or variant is None:
                continue
            applicable.append((group, variant, rec.metrics[metric]))
        if not applicable:
            raise ValueError("没有满足条件的数据用于比较。")
        grouped: dict[str, dict[str, float]] = defaultdict(dict)
        for group, variant, value in applicable:
            grouped[str(group)][str(variant)] = value
        variants = sorted({variant for _, variant, _ in applicable})
        groups = sorted(grouped.keys())
        fig, ax = plt.subplots(figsize=(max(6.5, len(groups) * 1.4), 4.8))
        fig.patch.set_facecolor("white")
        ax.set_facecolor("#F9FAFB")
        bar_width = 0.65 / max(1, len(variants))
        for idx, variant in enumerate(variants):
            offsets = [i + idx * bar_width for i in range(len(groups))]
            values = [grouped[group].get(variant, math.nan) for group in groups]
            ax.bar(
                offsets,
                values,
                width=bar_width * 0.7,
                label=variant,
                color=_color(idx),
                alpha=0.9,
            )
        ax.set_xticks([i + bar_width * (len(variants) - 1) / 2 for i in range(len(groups))])
        ax.set_xticklabels(groups)
        ax.set_ylabel(metric)
        ax.set_title(title or f"{self.suite.test_type} · {metric} 比较")
        ax.legend()
        ax.grid(axis="y", linestyle="--", alpha=0.4)
        output = Path(output_path) if output_path else self._default_output_path(f"{metric}_comparison")
        output.parent.mkdir(parents=True, exist_ok=True)
        fig.tight_layout()
        fig.savefig(output)
        plt.close(fig)
        return output

    # ----------------------------------------------------------------- helpers
    def _default_output_path(self, suffix: str) -> Path:
        if self.result_path:
            base = self.result_path.with_suffix("")
            return base.with_name(f"{base.name}_{suffix}.png")
        return Path.cwd() / f"{self.suite.test_type}_{suffix}.png"

    def _default_metric_list(self) -> list[str]:
        """默认只关心 real_time_ms，若不存在则退回其它任意指标。"""
        metrics = self.available_metrics()
        if "real_time_ms" in metrics:
            return ["real_time_ms"]
        return metrics[:1]


def _format_label(label: str, width: int = 32) -> str:
    label = label.replace("|", "\n")
    if len(label) <= width:
        return label
    parts = re.split(r"([_\-/])", label)
    lines: list[str] = []
    current = ""
    for token in parts:
        if len(current) + len(token) > width and current:
            lines.append(current)
            current = token
        else:
            current += token
    if current:
        lines.append(current)
    return "\n".join(lines)


def _match_filters(rec: BenchmarkRecord, filters: dict[str, str] | None) -> bool:
    """用于 comparison 图的简单元数据过滤器。"""
    if not filters:
        return True
    for key, expected in filters.items():
        actual = rec.meta.get(key)
        if actual is None:
            return False
        if str(actual) != expected:
            return False
    return True
# 统一的浅色调配色，方便在不同图表间保持视觉一致性
_PALETTE = [
    "#5B8FF9",
    "#5AD8A6",
    "#5D7092",
    "#F6BD16",
    "#E8684A",
    "#6DC8EC",
    "#9270CA",
    "#FF9D4D",
]


def _color(idx: int) -> str:
    """按照索引循环取色，避免颜色数量不足。"""
    return _PALETTE[idx % len(_PALETTE)]
