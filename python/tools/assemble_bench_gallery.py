#!/usr/bin/env python3
"""把每个语言目录下的 PNG 汇总成一个简易 HTML Gallery。"""
from __future__ import annotations

import argparse
import datetime as _dt
import html
from pathlib import Path


def _parse_args() -> argparse.Namespace:
    """定义 CLI 参数，仅需 root 和可选输出路径。"""
    parser = argparse.ArgumentParser(description="Generate an HTML gallery for benchmark plots.")
    parser.add_argument(
        "--root",
        required=True,
        help="Root directory containing per-language benchmark artifacts.",
    )
    parser.add_argument(
        "--output",
        help="Optional explicit output file path; defaults to <root>/index.html.",
    )
    return parser.parse_args()


def _discover(root: Path) -> list[tuple[str, list[tuple[str, Path]]]]:
    """扫描 root 下的语言目录，返回 (语言, [(label, path), ...])。"""
    sections: list[tuple[str, list[tuple[str, Path]]]] = []
    for lang_dir in sorted(p for p in root.iterdir() if p.is_dir()):
        plot_dir = lang_dir / "plots"
        if not plot_dir.is_dir():
            continue
        images = sorted(plot_dir.glob("*.png"))
        if not images:
            continue
        rel_lang = lang_dir.relative_to(root).as_posix()
        entries = [(img.stem, img.relative_to(root)) for img in images]
        sections.append((rel_lang, entries))
    return sections


def _build_html(root: Path, sections: list[tuple[str, list[tuple[str, Path]]]]) -> str:
    """根据扫描结果拼 HTML 字符串，整体风格保持简洁。"""
    timestamp = _dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    cards = []
    for lang, entries in sections:
        rows = []
        for label, rel_path in entries:
            title = html.escape(label)
            img_src = rel_path.as_posix()
            rows.append(
                f"""<figure>
    <img src="{html.escape(img_src)}" alt="{title}">
    <figcaption>{title}</figcaption>
</figure>"""
            )
        cards.append(
            f"""<section>
  <h2>{html.escape(lang)}</h2>
  <div class="stack">
    {' '.join(rows)}
  </div>
</section>"""
        )

    body = "\n".join(cards) if cards else "<p>No plots were generated.</p>"
    return f"""<!DOCTYPE html>
<html lang="zh">
<head>
  <meta charset="utf-8">
  <title>Benchmark Gallery</title>
  <style>
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      margin: 0 auto;
      padding: 2rem;
      max-width: 1200px;
      background: #f7f7f7;
    }}
    h1 {{
      text-align: center;
      margin-bottom: 1rem;
    }}
    section {{
      background: #fff;
      border-radius: 8px;
      padding: 1.5rem;
      margin-bottom: 1.5rem;
      box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    }}
    .stack {{
      display: flex;
      flex-direction: column;
      gap: 1rem;
    }}
    figure {{
      margin: 0;
      text-align: center;
      background: #fafafa;
      border-radius: 6px;
      padding: 0.75rem;
    }}
    img {{
      max-width: 100%;
      height: auto;
      border-radius: 4px;
      box-shadow: inset 0 0 0 1px #e5e5e5;
    }}
    figcaption {{
      font-size: 0.9rem;
      color: #555;
      margin-top: 0.4rem;
    }}
    footer {{
      text-align: center;
      font-size: 0.85rem;
      color: #777;
      margin-top: 2rem;
    }}
  </style>
</head>
<body>
  <h1>Benchmark Gallery</h1>
  {body}
  <footer>生成时间：{html.escape(timestamp)} · 根目录：{html.escape(root.as_posix())}</footer>
</body>
</html>
"""


def main() -> None:
    """入口：解析参数 -> 扫描 -> 生成 HTML -> 写文件。"""
    args = _parse_args()
    root = Path(args.root)
    root.mkdir(parents=True, exist_ok=True)
    sections = _discover(root)
    html_text = _build_html(root, sections)
    output = Path(args.output) if args.output else root / "index.html"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(html_text, encoding="utf-8")
    print(f"[bench-report] gallery -> {output}")


if __name__ == "__main__":
    main()
