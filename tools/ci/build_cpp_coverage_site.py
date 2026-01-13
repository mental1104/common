from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Dict, List, Tuple

def bar_svg(pct: float, w: int = 220, h: int = 14) -> str:
    pct = max(0.0, min(100.0, pct))
    fill_w = int(round(w * pct / 100.0))
    return (
        f'<rect x="0" y="0" width="{w}" height="{h}" rx="4" ry="4" fill="#e6e6e6"/>'
        f'<rect x="0" y="0" width="{fill_w}" height="{h}" rx="4" ry="4" fill="#4c9aff"/>'
    )

def choose_representatives(items: List[dict]) -> Dict[Tuple[str, str], dict]:
    # 每个 (os, compiler) 取最高 cxx_std 的结果作为代表
    best: Dict[Tuple[str, str], dict] = {}
    for it in items:
        k = (it["os"], it["compiler"])
        cur = best.get(k)
        if cur is None or int(it["cxx_std"]) > int(cur["cxx_std"]):
            best[k] = it
    return best

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cov-dir", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    cov_dir = Path(args.cov_dir)
    out_dir = Path(args.out_dir)
    (out_dir / "coverage").mkdir(parents=True, exist_ok=True)

    items = []
    for p in cov_dir.rglob("*.json"):
        try:
            items.append(json.loads(p.read_text(encoding="utf-8")))
        except Exception:
            continue

    reps = choose_representatives(items)
    rows = []
    for (os_, comp), it in sorted(reps.items()):
        rows.append({
            "os": os_,
            "compiler": comp,
            "cxx_std": it["cxx_std"],
            "line_pct": it["line_pct"],
            "source": it.get("source", ""),
            "sha": it.get("sha", ""),
        })

    overall = 0.0
    if rows:
        overall = round(sum(r["line_pct"] for r in rows) / len(rows), 2)

    # json
    data = {
        "overall": {"line_pct": overall, "env_count": len(rows)},
        "by_env": rows,
    }
    (out_dir / "coverage" / "cpp.json").write_text(
        json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    # svg (README 用)
    y = 24
    lines = []
    lines.append('<svg xmlns="http://www.w3.org/2000/svg" width="560" height="260">')
    lines.append('<style>text{font-family: -apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif; font-size:12px; fill:#111;}</style>')
    lines.append(f'<text x="0" y="14">C++ Coverage (overall): {overall}%</text>')
    lines.append(f'<g transform="translate(0,20)">{bar_svg(overall)}</g>')
    lines.append(f'<text x="240" y="32">{overall}%</text>')

    y = 60
    for r in rows:
        label = f'{r["os"]} / {r["compiler"]} / C++{r["cxx_std"]}'
        lines.append(f'<text x="0" y="{y}">{label}</text>')
        lines.append(f'<g transform="translate(0,{y+6})">{bar_svg(r["line_pct"])}</g>')
        lines.append(f'<text x="240" y="{y+18}">{r["line_pct"]}%</text>')
        y += 38

    lines.append("</svg>")
    (out_dir / "coverage" / "cpp.svg").write_text("\n".join(lines), encoding="utf-8")

    # html (Pages 用)
    tr = "\n".join(
        f"<tr><td>{r['os']}</td><td>{r['compiler']}</td><td>{r['cxx_std']}</td><td>{r['line_pct']}%</td><td><code>{r['source']}</code></td></tr>"
        for r in rows
    )
    html = f"""<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>C++ Coverage</title>
<style>
body{{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif;margin:24px;}}
table{{border-collapse:collapse;width:100%;}}
th,td{{border:1px solid #ddd;padding:8px;}}
th{{background:#f6f8fa;text-align:left;}}
code{{background:#f6f8fa;padding:2px 6px;border-radius:4px;}}
</style></head>
<body>
<h1>C++ Coverage</h1>
<p><strong>Overall:</strong> {overall}%</p>
<p><img src="./cpp.svg" alt="cpp coverage bar"></p>
<table>
<thead><tr><th>os</th><th>compiler</th><th>cxx_std</th><th>line_pct</th><th>source</th></tr></thead>
<tbody>{tr}</tbody>
</table>
</body></html>
"""
    (out_dir / "coverage" / "index.html").write_text(html, encoding="utf-8")

if __name__ == "__main__":
    main()
