#!/usr/bin/env python3
"""命令行脚本：读取 JSON 基准结果并输出 PNG 图表。"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

_PKG_ROOT = Path(__file__).resolve().parents[1]
if str(_PKG_ROOT) not in sys.path:
    sys.path.insert(0, str(_PKG_ROOT))

from mental1104.plot import BenchTestType, BenchmarkPlotter, load_benchmark_suite


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="读取 benchmark JSON 并生成 PNG，可选矩阵/排序/对比三种布局。"
    )
    parser.add_argument("--input", required=True, help="Path to benchmark JSON file.")
    parser.add_argument(
        "--test-type",
        required=True,
        choices=[BenchTestType.PYTEST_BENCHMARK, BenchTestType.GOOGLE_BENCHMARK],
        help="Benchmark framework name.",
    )
    parser.add_argument("--output", help="Optional output image path.")
    parser.add_argument("--title", help="Override chart title.")
    parser.add_argument(
        "--chart",
        choices=("case-matrix", "ranking", "comparison"),
        default="case-matrix",
        help="Chart layout.",
    )
    parser.add_argument("--metrics", nargs="+", help="Metrics used by case-matrix chart.")
    parser.add_argument("--metric", help="Metric for ranking/comparison chart.")
    parser.add_argument("--sort-by", help="case-matrix sorting metric.")
    parser.add_argument("--top-n", type=int, help="Limit number of cases for case-matrix/ranking.")
    parser.add_argument(
        "--include-aggregates",
        action="store_true",
        help="Keep Google Benchmark aggregate rows.",
    )
    parser.add_argument(
        "--ascending",
        choices=("auto", "asc", "desc"),
        default="auto",
        help="Sort direction for ranking & matrix charts.",
    )
    parser.add_argument("--group-field", help="Comparison chart: meta field for group (e.g., arg).")
    parser.add_argument("--variant-field", help="Comparison chart: meta field for variant (e.g., variant).")
    parser.add_argument("--filter", action="append", metavar="KEY=VALUE", help="Comparison chart filters.")
    return parser.parse_args()


def _parse_filters(pairs: list[str] | None) -> dict[str, str]:
    """解析 CLI 传入的 KEY=VALUE 过滤条件。"""
    filters: dict[str, str] = {}
    if not pairs:
        return filters
    for item in pairs:
        if "=" not in item:
            raise SystemExit(f"Invalid filter: {item!r}")
        key, value = item.split("=", 1)
        filters[key] = value
    return filters


def _preferred_metric(plotter: BenchmarkPlotter) -> str | None:
    """优先返回 real_time_ms，没有则退回任意指标。"""
    metrics = plotter.available_metrics()
    if "real_time_ms" in metrics:
        return "real_time_ms"
    return metrics[0] if metrics else None


def main() -> None:
    """主流程：加载 suite -> 选择图表 -> 调用 Plotter 输出。"""
    args = _parse_args()
    suite = load_benchmark_suite(
        test_type=args.test_type,
        result_path=args.input,
        include_aggregates=args.include_aggregates,
    )
    plotter = BenchmarkPlotter(suite, result_path=args.input)
    output_path = Path(args.output) if args.output else None
    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)

    ascending = None
    if args.ascending == "asc":
        ascending = True
    elif args.ascending == "desc":
        ascending = False

    if args.chart == "case-matrix":
        matrix_metrics = args.metrics
        if matrix_metrics is None:
            preferred = _preferred_metric(plotter)
            matrix_metrics = [preferred] if preferred else None
        image = plotter.plot_case_matrix(
            metrics=matrix_metrics,
            output_path=output_path,
            title=args.title,
            sort_by=args.sort_by,
            top_n=args.top_n,
            ascending=ascending,
        )
    elif args.chart == "ranking":
        metric = args.metric or (args.metrics[0] if args.metrics else None) or _preferred_metric(plotter)
        if metric is None:
            raise SystemExit("No metrics available for ranking chart.")
        image = plotter.plot_ranking(
            metric,
            output_path=output_path,
            title=args.title,
            top_n=args.top_n,
            ascending=ascending,
        )
    else:
        if not args.metric:
            raise SystemExit("--metric is required for comparison chart.")
        if not args.group_field or not args.variant_field:
            raise SystemExit("--group-field and --variant-field are required for comparison chart.")
        image = plotter.plot_comparison(
            args.metric,
            group_field=args.group_field,
            variant_field=args.variant_field,
            filters=_parse_filters(args.filter),
            title=args.title,
            output_path=output_path,
        )
    print(image)


if __name__ == "__main__":
    main()
