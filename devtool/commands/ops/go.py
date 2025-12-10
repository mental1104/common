from __future__ import annotations

import os
import shutil
from pathlib import Path
from typing import Mapping

from devtool.commands.common import GO_DIR, ensure_dir, run, strip_proxies


def setup(env: Mapping[str, str]) -> None:
    if not (Path(GO_DIR) / "go.mod").exists():
        raise SystemExit(f"[warn] {GO_DIR}/go.mod 不存在，请先初始化 go.mod")
    run(
        [
            env["GO"],
            "mod",
            "tidy",
        ],
        env=env,
        cwd=GO_DIR,
    )
    run(
        [
            env["GO"],
            "mod",
            "download",
        ],
        env=env,
        cwd=GO_DIR,
    )


def build(env: Mapping[str, str]) -> None:
    run([env["GO"], "build", "./..."], env=env, cwd=GO_DIR)


def build_bins(env: Mapping[str, str]) -> None:
    ensure_dir(Path(GO_DIR) / "bin")
    result = os.popen(f'cd "{GO_DIR}" && {env["GO"]} list -f "{{{{if eq .Name \\"main\\"}}}}{{{{.ImportPath}}}}|{{{{.Dir}}}}|{{{{.Name}}}}{{{{end}}}}" ./...').read().splitlines()
    for line in result:
        if not line.strip():
            continue
        pkg, dir_path, _ = line.split("|")
        name = Path(dir_path).name
        run([env["GO"], "build", "-o", str(Path("bin") / name), pkg], env=env, cwd=GO_DIR)


def test(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    args = [env_np["GO"], "test", "-count=1"]
    if env.get("GO_TEST_V"):
        args.append(env["GO_TEST_V"])
    pkg_args = ["./..."]
    if file_pattern:
        matched = []
        for path in Path(GO_DIR).rglob("*_test.go"):
            rel = path.relative_to(GO_DIR)
            if rel.match(file_pattern):
                matched.append(str(rel.parent))
        if matched:
            pkg_args = sorted(set(matched))
    if filter_expr:
        args += ["-run", filter_expr]
    run(args + pkg_args, env=env_np, cwd=GO_DIR)


def coverage(env: Mapping[str, str]) -> None:
    env_np = strip_proxies(env)
    pkgs = os.popen(f'cd "{GO_DIR}" && {env_np["GO"]} list ./...').read().split()
    coverpkg = ",".join(pkgs)
    args = [
        env_np["GO"],
        "test",
        "-count=1",
        "-covermode=atomic",
        f"-coverpkg={coverpkg}",
        "-coverprofile=coverage.out",
        "./...",
    ]
    run(args, env=env_np, cwd=GO_DIR)
    run([env_np["GO"], "tool", "cover", "-func=coverage.out"], env=env_np, cwd=GO_DIR)
    run([env_np["GO"], "tool", "cover", "-html=coverage.out", "-o", "coverage.html"], env=env_np, cwd=GO_DIR)


def fmt(env: Mapping[str, str]) -> None:
    run([env["GO"], "fmt", "./..."], env=env, cwd=GO_DIR)


def bench(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    args = [env_np["GO"], "test"]
    if env.get("GO_TEST_V"):
        args.append(env["GO_TEST_V"])
    bench_pat = filter_expr or "."
    args += [f"-bench={bench_pat}", "-benchmem"]
    pkg_args = ["./..."]
    if file_pattern:
        matched = []
        for path in Path(GO_DIR).rglob("*_test.go"):
            rel = path.relative_to(GO_DIR)
            if rel.match(file_pattern):
                matched.append(str(rel.parent))
        if matched:
            pkg_args = sorted(set(matched))
    run(args + pkg_args, env=env_np, cwd=GO_DIR)


def install(env: Mapping[str, str]) -> None:
    run([env["GO"], "install", "./..."], env=strip_proxies(env), cwd=GO_DIR)


def uninstall(env: Mapping[str, str]) -> None:
    env_np = strip_proxies(env)
    bin_dir = Path(env_np.get("GOBIN") or os.popen(f'{env_np["GO"]} env GOBIN').read().strip() or os.path.join(os.popen(f'{env_np["GO"]} env GOPATH').read().strip(), "bin"))
    if not bin_dir:
        return
    for line in os.popen(f'cd "{GO_DIR}" && {env_np["GO"]} list -f "{{{{if eq .Name \\"main\\"}}}}{{{{.Dir}}}}{{{{end}}}}" ./...').read().splitlines():
        if not line.strip():
            continue
        target = bin_dir / Path(line).name
        if target.exists():
            target.unlink()


def clean(env: Mapping[str, str]) -> None:
    for f in ["coverage.out", "coverage.html"]:
        (Path(GO_DIR) / f).unlink(missing_ok=True)
    shutil.rmtree(Path(GO_DIR) / "bin", ignore_errors=True)
    env_np = strip_proxies(env)
    run([env_np["GO"], "clean", "-testcache"], env=env_np, cwd=GO_DIR)
    run([env_np["GO"], "clean", "./..."], env=env_np, cwd=GO_DIR)


def vet(env: Mapping[str, str]) -> None:
    run([env["GO"], "vet", "./..."], env=strip_proxies(env), cwd=GO_DIR)


def guard(env: Mapping[str, str]) -> None:
    env_np = strip_proxies(env)
    run([env_np["GO"], "test", "-race", "-count=1", "./..."], env=env_np, cwd=GO_DIR)
