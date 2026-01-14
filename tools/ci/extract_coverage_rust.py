#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def safe_float(x, default=0.0):
    try:
        return float(x)
    except Exception:
        return default


def parse_cobertura(xml_path: Path) -> float:
    root = ET.parse(str(xml_path)).getroot()
    line_rate = root.attrib.get("line-rate")
    if line_rate is not None:
        return round(float(line_rate) * 100.0, 3)
    lines_covered = root.attrib.get("lines-covered")
    lines_valid = root.attrib.get("lines-valid")
    if lines_covered is not None and lines_valid is not None:
        valid = int(lines_valid)
        if valid <= 0:
            return 0.0
        return round(int(lines_covered) * 100.0 / valid, 3)
    return 0.0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--os", required=True)
    ap.add_argument("--toolchain", required=True)
    ap.add_argument("--xml", default="coverage.xml")
    ap.add_argument("--out", default="cov.json")
    args = ap.parse_args()

    xml_path = Path(args.xml)
    out_path = Path(args.out)

    cov = {
        "lang": "rust",
        "os": args.os,
        "toolchain": args.toolchain,
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
        cov["line_pct"] = parse_cobertura(xml_path)
        cov["status"] = "ok"
        cov["source"] = "coverage.xml"
        cov["src"] = str(xml_path)
    except Exception as e:
        cov["status"] = "error"
        cov["source"] = "parse_error"
        cov["src"] = str(xml_path)
        cov["error"] = str(e)

    out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
