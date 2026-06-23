from __future__ import annotations

import os
import shutil
import tempfile
from pathlib import Path
from typing import Mapping

from devtool.commands.common import GO_DIR, ensure_dir, run, strip_proxies


def _read_go_module_path(go_dir: Path) -> str:
    go_mod = go_dir / "go.mod"
    if not go_mod.exists():
        raise SystemExit(f"[error] missing go.mod at {go_mod}")
    for raw in go_mod.read_text().splitlines():
        line = raw.strip()
        if line.startswith("module "):
            parts = line.split()
            if len(parts) >= 2:
                return parts[1]
    raise SystemExit(f"[error] failed to parse module path from {go_mod}")


def _go_bin_dir(env: Mapping[str, str]) -> Path | None:
    env_np = strip_proxies(env)
    go_bin = env_np.get("GO", "go")
    bin_dir = env_np.get("GOBIN", "").strip()
    if not bin_dir:
        bin_dir = os.popen(f'{go_bin} env GOBIN').read().strip()
    if not bin_dir:
        gopath = os.popen(f'{go_bin} env GOPATH').read().strip()
        if gopath:
            bin_dir = os.path.join(gopath.split(os.pathsep)[0], "bin")
    if not bin_dir:
        return None
    return Path(bin_dir)


def _install_verify_binary(env: Mapping[str, str]) -> None:
    env_np = strip_proxies(env)
    bin_dir = _go_bin_dir(env_np)
    if not bin_dir:
        print("[warn] go install skipped verify binary; GOBIN/GOPATH not available")
        return
    ensure_dir(bin_dir)
    module_path = _read_go_module_path(GO_DIR)
    env_go = dict(env_np)
    env_go["GOWORK"] = "off"
    env_go["GOPROXY"] = "off"
    env_go["GOSUMDB"] = "off"
    with tempfile.TemporaryDirectory(prefix="m1104-go-install-") as tmp:
        tmp_path = Path(tmp)
        (tmp_path / "main.go").write_text(
            "package main\n"
            "\n"
            f"import \"{module_path}/mental1104\"\n"
            "\n"
            "func main() {\n"
            "  if !mental1104.Contains(\"abc\", \"a\") {\n"
            "    panic(\"verify failed\")\n"
            "  }\n"
            "}\n"
        )
        run([env_go["GO"], "mod", "init", "verify-mental1104-install"], env=env_go, cwd=tmp_path)
        run([env_go["GO"], "mod", "edit", f"-replace={module_path}={GO_DIR}"], env=env_go, cwd=tmp_path)
        run([env_go["GO"], "mod", "tidy"], env=env_go, cwd=tmp_path)
        bin_name = "mental1104-go-verify.exe" if os.name == "nt" else "mental1104-go-verify"
        run([env_go["GO"], "build", "-buildvcs=false", "-o", str(bin_dir / bin_name)], env=env_go, cwd=tmp_path)


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
    args = [env["GO"], "build"]
    if str(env.get("VERBOSE", "")).strip().lower() in ("1", "true", "yes", "on"):
        args.append("-v")
    run(args + ["./..."], env=env, cwd=GO_DIR)
    built = build_bins(env)
    if built == 0:
        print("[info] go build completed; no main packages to emit binaries")


def build_bins(env: Mapping[str, str]) -> int:
    ensure_dir(Path(GO_DIR) / "bin")
    result = os.popen(f'cd "{GO_DIR}" && {env["GO"]} list -f "{{{{if eq .Name \\"main\\"}}}}{{{{.ImportPath}}}}|{{{{.Dir}}}}|{{{{.Name}}}}{{{{end}}}}" ./...').read().splitlines()
    built = 0
    for line in result:
        if not line.strip():
            continue
        pkg, dir_path, _ = line.split("|")
        name = Path(dir_path).name
        run([env["GO"], "build", "-o", str(Path("bin") / name), pkg], env=env, cwd=GO_DIR)
        built += 1
    return built


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
    _install_verify_binary(env)


def uninstall(env: Mapping[str, str]) -> None:
    env_np = strip_proxies(env)
    bin_dir = _go_bin_dir(env_np)
    if not bin_dir:
        return
    for line in os.popen(f'cd "{GO_DIR}" && {env_np["GO"]} list -f "{{{{if eq .Name \\"main\\"}}}}{{{{.Dir}}}}{{{{end}}}}" ./...').read().splitlines():
        if not line.strip():
            continue
        target = bin_dir / Path(line).name
        if target.exists():
            target.unlink()
    verify_name = "mental1104-go-verify.exe" if os.name == "nt" else "mental1104-go-verify"
    verify_bin = bin_dir / verify_name
    if verify_bin.exists():
        verify_bin.unlink()


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
