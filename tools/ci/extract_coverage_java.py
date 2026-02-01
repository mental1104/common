#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path


def _safe_int(value, default=0):
    try:
        return int(value)
    except Exception:
        return default


def _line_pct_from_counters(counters) -> float:
    missed = 0
    covered = 0
    for counter in counters:
        if counter.attrib.get("type") != "LINE":
            continue
        missed += _safe_int(counter.attrib.get("missed", 0))
        covered += _safe_int(counter.attrib.get("covered", 0))
    total = missed + covered
    if total <= 0:
        return 0.0
    return round(covered * 100.0 / total, 3)


def _line_counts_from_counters(counters) -> tuple[int, int]:
    missed = 0
    covered = 0
    for counter in counters:
        if counter.attrib.get("type") != "LINE":
            continue
        missed += _safe_int(counter.attrib.get("missed", 0))
        covered += _safe_int(counter.attrib.get("covered", 0))
    return missed, covered


@dataclass(frozen=True)
class LineRecord:
    path: str
    line: int
    status: str
    mi: int
    ci: int
    mb: int
    cb: int
    text: str


def _collect_sourcefiles(root) -> list[tuple[str, int, int, float]]:
    rows: list[tuple[str, int, int, float]] = []
    for pkg in root.findall("package"):
        pkg_name = (pkg.attrib.get("name") or "").strip()
        for source in pkg.findall("sourcefile"):
            name = (source.attrib.get("name") or "").strip()
            if not name:
                continue
            full = f"{pkg_name}/{name}" if pkg_name else name
            missed, covered = _line_counts_from_counters(source.findall("counter"))
            total = missed + covered
            pct = round(covered * 100.0 / total, 3) if total > 0 else 0.0
            rows.append((full, total, missed, pct))
    rows.sort(key=lambda r: r[0])
    return rows


def _compress_ranges(nums: list[int]) -> str:
    if not nums:
        return ""
    nums = sorted(set(nums))
    ranges: list[tuple[int, int]] = []
    start = nums[0]
    prev = nums[0]
    for n in nums[1:]:
        if n == prev + 1:
            prev = n
            continue
        ranges.append((start, prev))
        start = n
        prev = n
    ranges.append((start, prev))
    parts = []
    for a, b in ranges:
        parts.append(str(a) if a == b else f"{a}-{b}")
    return ",".join(parts)


def _collect_gcc_rows(root) -> list[tuple[str, int, int, float, str]]:
    rows: list[tuple[str, int, int, float, str]] = []
    for pkg in root.findall("package"):
        pkg_name = (pkg.attrib.get("name") or "").strip()
        for source in pkg.findall("sourcefile"):
            name = (source.attrib.get("name") or "").strip()
            if not name:
                continue
            rel_path = f"{pkg_name}/{name}" if pkg_name else name
            missed, covered = _line_counts_from_counters(source.findall("counter"))
            total = missed + covered
            pct = round(covered * 100.0 / total, 3) if total > 0 else 0.0
            missing_lines = []
            for line in source.findall("line"):
                nr = _safe_int(line.attrib.get("nr", 0))
                mi = _safe_int(line.attrib.get("mi", 0))
                ci = _safe_int(line.attrib.get("ci", 0))
                if ci == 0 and mi > 0:
                    missing_lines.append(nr)
            rows.append((rel_path, total, covered, pct, _compress_ranges(missing_lines)))
    rows.sort(key=lambda r: r[0])
    return rows


def _print_gcc_report(rows: list[tuple[str, int, int, float, str]], directory: str) -> None:
    max_len = max([len(r[0]) for r in rows] + [4])
    name_width = min(max_len, 60)
    sep = "-" * (name_width + 36)
    print(sep)
    print("                           GCC Code Coverage Report")
    print(f"Directory: {directory}")
    print(sep)
    header = f"{'File':<{name_width}}  {'Lines':>7}  {'Exec':>6}  {'Cover':>6}   Missing"
    print(header)
    print(sep)
    total_lines = 0
    total_exec = 0
    for path, lines, exec_lines, pct, missing in rows:
        total_lines += lines
        total_exec += exec_lines
        cover_text = f"{pct:.0f}%"
        if len(path) > name_width:
            print(path)
            path = ""
        print(f"{path:<{name_width}}  {lines:>7}  {exec_lines:>6}  {cover_text:>6}   {missing}")
    total_pct = round(total_exec * 100.0 / total_lines, 3) if total_lines > 0 else 0.0
    print(sep)
    print(f"{'TOTAL':<{name_width}}  {total_lines:>7}  {total_exec:>6}  {total_pct:.0f}%")
    print(sep)


def _read_source_line(source_root: Path | None, rel_path: str, line_no: int) -> str:
    if source_root is None:
        return ""
    path = source_root / rel_path
    if not path.exists():
        return ""
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except Exception:
        return ""
    if line_no <= 0 or line_no > len(lines):
        return ""
    return lines[line_no - 1].rstrip()


def _collect_line_records(root, source_root: Path | None) -> list[LineRecord]:
    rows: list[LineRecord] = []
    for pkg in root.findall("package"):
        pkg_name = (pkg.attrib.get("name") or "").strip()
        for source in pkg.findall("sourcefile"):
            name = (source.attrib.get("name") or "").strip()
            if not name:
                continue
            rel_path = f"{pkg_name}/{name}" if pkg_name else name
            for line in source.findall("line"):
                nr = _safe_int(line.attrib.get("nr", 0))
                mi = _safe_int(line.attrib.get("mi", 0))
                ci = _safe_int(line.attrib.get("ci", 0))
                mb = _safe_int(line.attrib.get("mb", 0))
                cb = _safe_int(line.attrib.get("cb", 0))
                if ci == 0 and mi > 0:
                    status = "missed"
                elif ci > 0 and (mi > 0 or mb > 0):
                    status = "partial"
                else:
                    status = "covered"
                text = _read_source_line(source_root, rel_path, nr)
                rows.append(LineRecord(rel_path, nr, status, mi, ci, mb, cb, text))
    rows.sort(key=lambda r: (r.path, r.line))
    return rows


def _print_line_list(title: str, rows: list[LineRecord], max_lines: int) -> None:
    print(title)
    if not rows:
        print("(none)")
        return
    shown = 0
    for row in rows:
        shown += 1
        if max_lines > 0 and shown > max_lines:
            remaining = len(rows) - max_lines
            print(f"... {remaining} more")
            break
        if row.text:
            print(f"{row.path}:{row.line}  {row.text}")
        else:
            print(f"{row.path}:{row.line}")


def _print_table(rows: list[tuple[str, int, int, float]]) -> None:
    name_width = max([len(r[0]) for r in rows] + [4])
    header = f"{'Name':<{name_width}}  {'Stmts':>7}  {'Miss':>6}  {'Cover':>6}"
    print(header)
    print("-" * len(header))
    total_stmts = 0
    total_miss = 0
    for name, stmts, miss, pct in rows:
        total_stmts += stmts
        total_miss += miss
        cover_text = f"{pct:.0f}%"
        print(f"{name:<{name_width}}  {stmts:>7}  {miss:>6}  {cover_text:>6}")
    total_pct = round((total_stmts - total_miss) * 100.0 / total_stmts, 3) if total_stmts > 0 else 0.0
    print("-" * len(header))
    print(f"{'TOTAL':<{name_width}}  {total_stmts:>7}  {total_miss:>6}  {total_pct:.0f}%")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--os", required=True)
    ap.add_argument("--java", required=True)
    ap.add_argument("--kind", default="java")
    ap.add_argument("--xml", default="target/site/jacoco/jacoco.xml")
    ap.add_argument("--out", default="cov.json")
    ap.add_argument("--table", action="store_true", help="Print per-sourcefile coverage table")
    ap.add_argument("--gcc", action="store_true", help="Print GCC-style coverage summary")
    ap.add_argument("--lines", choices=["missed", "partial", "all"], help="Print uncovered line numbers")
    ap.add_argument("--source-root", help="Root directory for source files")
    ap.add_argument("--max-lines", type=int, default=200, help="Max lines to print (0 = no limit)")
    args = ap.parse_args()

    xml_path = Path(args.xml)
    out_path = Path(args.out)

    cov = {
        "lang": "java",
        "os": args.os,
        "java": args.java,
        "line_pct": 0.0,
        "status": "placeholder",
        "sha": os.getenv("GITHUB_SHA", ""),
        "run_id": os.getenv("GITHUB_RUN_ID", ""),
        "source": "placeholder",
        "src": "",
    }

    if not xml_path.exists():
        out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
        return 0

    try:
        tree = ET.parse(str(xml_path))
        root = tree.getroot()
        counters = root.findall("counter")
        if not counters:
            counters = root.findall(".//counter")
        cov["line_pct"] = _line_pct_from_counters(counters)
        cov["status"] = "ok"
        cov["source"] = "jacoco.xml"
        cov["src"] = str(xml_path)
        if args.gcc:
            rows = _collect_gcc_rows(root)
            directory = args.source_root or ".."
            _print_gcc_report(rows, directory)
        if args.table:
            rows = _collect_sourcefiles(root)
            _print_table(rows)
        if args.lines:
            source_root = Path(args.source_root) if args.source_root else None
            records = _collect_line_records(root, source_root)
            missed = [r for r in records if r.status == "missed"]
            partial = [r for r in records if r.status == "partial"]
            if args.lines in ("missed", "all"):
                _print_line_list("Missed lines (ci == 0):", missed, args.max_lines)
            if args.lines in ("partial", "all"):
                _print_line_list("Partially covered lines (mi > 0 or mb > 0):", partial, args.max_lines)
    except Exception as e:
        cov["status"] = "error"
        cov["source"] = "parse_error"
        cov["src"] = str(xml_path)
        cov["error"] = str(e)

    out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
