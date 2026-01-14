#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Optional


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


def find_latest_report(root: Path) -> Optional[Path]:
    if not root.exists():
        return None
    candidates = []
    for name in ("coverage.cobertura.xml", "cobertura.xml", "coverage.xml"):
        for p in root.rglob(name):
            if p.is_file():
                candidates.append(p)
    if not candidates:
        return None
    candidates.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return candidates[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--os", required=True)
    ap.add_argument("--dotnet", required=True)
    ap.add_argument("--xml", default="coverage.cobertura.xml")
    ap.add_argument("--root", default="dotnet")
    ap.add_argument("--out", default="cov.json")
    args = ap.parse_args()

    root = Path(args.root)
    xml_path = Path(args.xml)
    if not xml_path.is_absolute():
        xml_path = root / xml_path
    out_path = Path(args.out)

    cov = {
        "lang": "dotnet",
        "os": args.os,
        "dotnet": args.dotnet,
        "line_pct": 0.0,
        "status": "placeholder",
        "sha": os.getenv("GITHUB_SHA", ""),
        "run_id": os.getenv("GITHUB_RUN_ID", ""),
        "source": "placeholder",
        "src": "",
    }

    target = xml_path if xml_path.exists() else find_latest_report(root)
    if target is None or not target.exists():
        out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
        return 0

    try:
        cov["line_pct"] = parse_cobertura(target)
        cov["status"] = "ok"
        cov["source"] = target.name
        cov["src"] = str(target)
    except Exception as e:
        cov["status"] = "error"
        cov["source"] = "parse_error"
        cov["src"] = str(target)
        cov["error"] = str(e)

    out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
