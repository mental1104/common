from __future__ import annotations

import argparse
import datetime as dt
import html
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parents[2]

# 只关注这 5 个目录
LANG_DIRS = {
    "python": ("python", {".py"}),
    "rust": ("rust", {".rs"}),
    "cpp": ("cpp", {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp"}),
    "golang": ("golang", {".go"}),
    "dotnet": ("dotnet", {".cs", ".csx", ".fs", ".fsx", ".vb"}),
    "devops": ("devops", {".py"}),
}

# 走读完成标记（大小写不敏感；允许后面跟日期/备注）
# 例如：WALKTHROUGH: done 2026-01-11 (notes: xxx)
MARKER = "walkthrough: done"
HEAD_LINES = 30

# 各语言打标指南（展示在 GitHub Pages 面板里）
LANG_GUIDE = {
    "python": {
        "where": "文件头部（建议 shebang / encoding / 模块注释附近），前 30 行内",
        "how": "加入一行注释，包含文本 WALKTHROUGH: done（大小写不敏感）",
        "example": "# WALKTHROUGH: done 2026-01-11",
        "note": "建议用注释行；不要放在字符串里以免误判为“完成但未真正标注”。",
    },
    "rust": {
        "where": "文件头部前 30 行内",
        "how": "加入一行行注释 // ...，包含 WALKTHROUGH: done",
        "example": "// WALKTHROUGH: done 2026-01-11",
        "note": "建议单独成行，避免混入代码尾注释导致可读性差。",
    },
    "cpp": {
        "where": "文件头部前 30 行内（版权/说明注释块之后也可以）",
        "how": "加入一行 // ... 或 /* ... */，包含 WALKTHROUGH: done（脚本按纯文本匹配）",
        "example": "// WALKTHROUGH: done 2026-01-11",
        "note": "推荐 // 单行，便于 diff 与 grep。",
    },
    "golang": {
        "where": "package 声明之前或之后的文件头部前 30 行内",
        "how": "加入一行 // ...，包含 WALKTHROUGH: done",
        "example": "// WALKTHROUGH: done 2026-01-11",
        "note": "Go 文件头通常很干净，建议放在 package 上方。",
    },
    "dotnet": {
        "where": "文件头部前 30 行内",
        "how": "加入一行 // ...，包含 WALKTHROUGH: done",
        "example": "// WALKTHROUGH: done 2026-01-11",
        "note": "适用于 .cs/.fs/.vb 等；脚本按文件扩展名筛选。",
    },
}

@dataclass(frozen=True)
class FileStat:
    path: str
    done: bool

def run_git_ls_files(paths: Iterable[str]) -> list[str]:
    # 只统计被 git 跟踪的文件（避免 build/venv/target 等噪音）
    cmd = ["git", "ls-files", "--"] + list(paths)
    out = subprocess.check_output(cmd, cwd=REPO_ROOT, text=True)
    return [line.strip() for line in out.splitlines() if line.strip()]

def is_done(p: Path) -> bool:
    try:
        with p.open("r", encoding="utf-8", errors="ignore") as f:
            for i, line in enumerate(f):
                if i >= HEAD_LINES:
                    break
                if MARKER in line.lower():
                    return True
        return False
    except OSError:
        return False

def pct(done: int, total: int) -> float:
    return 0.0 if total == 0 else round(done * 100.0 / total, 2)

def bar(p: float, width: int = 24) -> str:
    # p: 0~100
    filled = int(round((p / 100.0) * width))
    filled = max(0, min(width, filled))
    return "█" * filled + "░" * (width - filled)

def render_guidelines() -> str:
    rows = []
    for lang in ["python", "rust", "cpp", "golang", "dotnet"]:
        g = LANG_GUIDE[lang]
        rows.append(
            "<tr>"
            f"<td><strong>{html.escape(lang)}</strong></td>"
            f"<td>{html.escape(g['where'])}</td>"
            f"<td>{html.escape(g['how'])}</td>"
            f"<td><code>{html.escape(g['example'])}</code></td>"
            f"<td>{html.escape(g['note'])}</td>"
            "</tr>"
        )

    return f"""
  <h2>Marking Guide（打标指南）</h2>
  <p>
    <strong>Walkthrough 判定标准（脚本口径）：</strong>
    只要该文件在<strong>前 {HEAD_LINES} 行</strong>内出现文本 <code>{html.escape(MARKER)}</code>（大小写不敏感，可追加日期/备注），就视为已 walkthrough。
  </p>
  <table>
    <thead>
      <tr>
        <th>Lang</th>
        <th>Where</th>
        <th>How</th>
        <th>Example</th>
        <th>Note</th>
      </tr>
    </thead>
    <tbody>
      {''.join(rows)}
    </tbody>
  </table>
"""

def render_index(data: dict) -> str:
    updated_at = html.escape(data["updated_at"])
    rows = []
    for lang, s in data["by_lang"].items():
        rows.append(
            f"<tr><td>{html.escape(lang)}</td>"
            f"<td>{s['done']}/{s['total']}</td>"
            f"<td>{s['pct']}%</td>"
            f"<td><code>{html.escape(bar(s['pct']))}</code></td></tr>"
        )

    remain_sections = []
    for lang, s in data["by_lang"].items():
        remain = s["remaining"]
        if not remain:
            continue
        items = "\n".join(f"<li><code>{html.escape(p)}</code></li>" for p in remain[:200])
        more = ""
        if len(remain) > 200:
            more = f"<p>... and {len(remain) - 200} more</p>"
        remain_sections.append(f"<h3>{html.escape(lang)} remaining</h3><ul>{items}</ul>{more}")

    remain_html = "\n".join(remain_sections) if remain_sections else "<p>All done.</p>"
    guide_html = render_guidelines()

    return f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Walkthrough Progress</title>
  <style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif; margin: 24px; }}
    table {{ border-collapse: collapse; width: 100%; }}
    th, td {{ border: 1px solid #ddd; padding: 8px; vertical-align: top; }}
    th {{ background: #f6f8fa; text-align: left; }}
    code {{ background: #f6f8fa; padding: 2px 6px; border-radius: 4px; }}
  </style>
</head>
<body>
  <h1>Walkthrough Progress</h1>
  <p><strong>Updated:</strong> {updated_at}</p>

  <h2>Summary</h2>
  <p><strong>Total:</strong> {data['done']}/{data['total']} ({data['pct']}%) <code>{html.escape(bar(data['pct']))}</code></p>

  <h2>By language</h2>
  <table>
    <thead><tr><th>Lang</th><th>Done/Total</th><th>Pct</th><th>Bar</th></tr></thead>
    <tbody>
      {"".join(rows)}
    </tbody>
  </table>

  {guide_html}

  <h2>Remaining</h2>
  {remain_html}
</body>
</html>
"""

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="dist", help="output dir (relative to repo root)")
    args = ap.parse_args()

    out_dir = (REPO_ROOT / args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    # 收集文件
    all_stats: dict[str, list[FileStat]] = {}
    for lang, (d, exts) in LANG_DIRS.items():
        files = run_git_ls_files([d])
        picked = []
        for rel in files:
            p = Path(rel)
            if p.suffix.lower() in exts:
                picked.append(rel)
        stats = []
        for rel in picked:
            stats.append(FileStat(path=rel, done=is_done(REPO_ROOT / rel)))
        all_stats[lang] = stats

    # 汇总
    by_lang = {}
    total_done = 0
    total_cnt = 0
    for lang, stats in all_stats.items():
        done_cnt = sum(1 for s in stats if s.done)
        total = len(stats)
        total_done += done_cnt
        total_cnt += total
        remaining = [s.path for s in stats if not s.done]
        by_lang[lang] = {
            "done": done_cnt,
            "total": total,
            "pct": pct(done_cnt, total),
            "remaining": remaining,
        }

    data = {
        "done": total_done,
        "total": total_cnt,
        "pct": pct(total_done, total_cnt),
        "updated_at": dt.datetime.now(dt.timezone(dt.timedelta(hours=9))).isoformat(),
        "by_lang": by_lang,
    }

    # 写 JSON（给 badge/程序化读取用）
    (out_dir / "progress.json").write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")

    # 写 HTML 面板（含打标指南）
    (out_dir / "index.html").write_text(render_index(data), encoding="utf-8")

    # 写 Actions Summary（Markdown）
    summary_lines = []
    summary_lines.append(f"**Total**: {data['done']}/{data['total']} ({data['pct']}%)  `{bar(data['pct'])}`")
    summary_lines.append("")
    summary_lines.append("| lang | done/total | pct |")
    summary_lines.append("|---|---:|---:|")
    for lang in ["python", "rust", "cpp", "golang", "dotnet"]:
        s = data["by_lang"][lang]
        summary_lines.append(f"| {lang} | {s['done']}/{s['total']} | {s['pct']}% |")
    (out_dir / "summary.md").write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()

