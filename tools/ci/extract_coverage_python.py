'''
Date: 2026-01-13 17:14:50
Author: mental1104 mental1104@gmail.com
LastEditors: mental1104 mental1104@gmail.com
LastEditTime: 2026-01-13 17:15:36
'''
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--os", required=True)
    ap.add_argument("--python", required=True)          # e.g. 3.8 / 3.12
    ap.add_argument("--kind", default="python")         # module name
    ap.add_argument("--xml", default="coverage.xml")
    ap.add_argument("--out", default="cov.json")
    args = ap.parse_args()

    xml_path = Path(args.xml)
    out_path = Path(args.out)

    # 默认 placeholder（不让 downstream 裂）
    cov = {
        "lang": "python",
        "os": args.os,
        "python": args.python,
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
        # Cobertura: line-rate in [0,1]
        lr = safe_float(root.attrib.get("line-rate", "0"))
        cov["line_pct"] = round(lr * 100.0, 3)
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
