from __future__ import annotations

from argparse import ArgumentParser
from pathlib import Path

from devtool.commands import register
from devtool.commands.common import ROOT, base_env
from devtool.commands.ops import cpp as cpp_ops, go as go_ops, python as python_ops, rust as rust_ops


def _write_env_example():
    env_src = ROOT / ".env"
    env_example = ROOT / ".env.example"
    if not env_src.exists():
        print(f"[warn] {env_src} 不存在，跳过 env-example 生成")
        return
    lines = []
    for raw in env_src.read_text().splitlines():
        if not raw.strip() or raw.strip().startswith("#"):
            lines.append(raw)
            continue
        line = raw
        if line.startswith("export "):
            line = line[len("export ") :]
        if "=" not in line:
            lines.append("#" + raw)
            continue
        key = line.split("=", 1)[0].strip()
        lines.append(f"{key}=")
    env_example.write_text("\n".join(lines) + "\n")
    print(f"[ok] 生成 {env_example}")


@register("setup")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("setup", help="Project setup (env + deps + configure)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("--config", default="Debug", help="C++ build type")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, cpp_build_type=args.config)
    _write_env_example()
    python_ops.setup(env)
    go_ops.setup(env)
    cpp_ops.prepare_submodules(env, skip_when_ready=True)
    cpp_ops.configure(env)
    rust_ops.setup(env)
