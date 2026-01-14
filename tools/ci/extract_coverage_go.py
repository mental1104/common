#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import json
import os
import sys
from pathlib import Path


def parse_coverprofile(path: Path) -> float:
    total = 0
    covered = 0
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    for line in lines:
        if not line or line.startswith("mode:"):
            continue
        parts = line.split()
        if len(parts) < 3:
            continue
        try:
            stmts = int(parts[1])
            count = int(parts[2])
        except ValueError:
            continue
        total += stmts
        if count > 0:
            covered += stmts
    if total <= 0:
        return 0.0
    return round(covered * 100.0 / total, 3)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--os", required=True)
    ap.add_argument("--go", required=True)
    ap.add_argument("--coverprofile", default="coverage.out")
    ap.add_argument("--out", default="cov.json")
    args = ap.parse_args()

    profile_path = Path(args.coverprofile)
    out_path = Path(args.out)

    cov = {
        "lang": "golang",
        "os": args.os,
        "go": args.go,
        "line_pct": 0.0,
        "status": "placeholder",
        "sha": os.getenv("GITHUB_SHA", ""),
        "run_id": os.getenv("GITHUB_RUN_ID", ""),
        "source": "placeholder",
        "src": "",
    }

    if not profile_path.exists():
        out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
        return 0

    try:
        cov["line_pct"] = parse_coverprofile(profile_path)
        cov["status"] = "ok"
        cov["source"] = "coverage.out"
        cov["src"] = str(profile_path)
    except Exception as e:
        cov["status"] = "error"
        cov["source"] = "parse_error"
        cov["src"] = str(profile_path)
        cov["error"] = str(e)

    out_path.write_text(json.dumps(cov, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
