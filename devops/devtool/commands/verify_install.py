from __future__ import annotations

import os
import shlex
import shutil
import sys
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING, Mapping

from devtool.commands import register
from devtool.commands.common import GO_DIR, ROOT, RUST_DIR, base_env, run

if TYPE_CHECKING:
    from argparse import ArgumentParser


def _split_cmd(value: str) -> list[str]:
    return shlex.split(value) if value else []


def _with_lib_path(env: Mapping[str, str], lib_dir: Path) -> dict[str, str]:
    merged = dict(env)
    key = "LD_LIBRARY_PATH" if sys.platform.startswith("linux") else "DYLD_LIBRARY_PATH"
    existing = merged.get(key, "")
    merged[key] = str(lib_dir) if not existing else f"{lib_dir}{os.pathsep}{existing}"
    return merged


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


def _verify_cpp(env: Mapping[str, str]) -> None:
    prefix = Path(env.get("PREFIX", "/usr/local"))
    include_dir = prefix / "include"
    lib_dir = prefix / "lib"
    if not (include_dir / "mental1104").exists():
        raise SystemExit(f"[error] missing C++ headers under {include_dir}")
    if not any(lib_dir.glob("libmental1104*")):
        raise SystemExit(f"[error] missing libmental1104 under {lib_dir}")

    cxx = env.get("CXX", "c++")
    if not shutil.which(cxx.split()[0]):
        raise SystemExit(f"[error] missing C++ compiler: {cxx}")

    with tempfile.TemporaryDirectory(prefix="m1104-verify-cpp-") as tmp:
        tmp_path = Path(tmp)
        src = tmp_path / "verify.cpp"
        exe = tmp_path / "verify_cpp"
        src.write_text(
            "#include <mental1104/concurrency/thread/thread_util.h>\n"
            "\n"
            "int main() {\n"
            "  ThreadPool pool(1);\n"
            "  auto fut = pool.submit([] { return 7; });\n"
            "  return fut.get() == 7 ? 0 : 1;\n"
            "}\n"
        )
        use_system_include = (not sys.platform.startswith("win")) and (prefix.resolve() == Path("/usr/local"))
        include_args = [] if use_system_include else ["-I", str(include_dir)]
        cmd = [
            *_split_cmd(cxx),
            "-std=c++20",
            *include_args,
            str(src),
            "-L",
            str(lib_dir),
            "-lmental1104",
            "-pthread",
            f"-Wl,-rpath,{lib_dir}",
            "-o",
            str(exe),
        ]
        run(cmd, env=env, cwd=tmp_path)
        run([str(exe)], env=_with_lib_path(env, lib_dir), cwd=tmp_path)
    print("cpp-ok")


def _verify_python(env: Mapping[str, str]) -> None:
    py = env.get("PYTHON", "python3")
    code = "import mental1104; import mental1104_export_layer; print('python-ok')"
    run([py, "-c", code], env=env)


def _verify_go(env: Mapping[str, str]) -> None:
    go_bin = env.get("GO", "go")
    module_path = _read_go_module_path(GO_DIR)
    if not GO_DIR.exists():
        raise SystemExit(f"[error] missing Go module directory at {GO_DIR}")
    env_go = dict(env)
    env_go["GOWORK"] = "off"
    env_go["GOPROXY"] = "off"
    env_go["GOSUMDB"] = "off"

    with tempfile.TemporaryDirectory(prefix="m1104-verify-go-") as tmp:
        tmp_path = Path(tmp)
        (tmp_path / "main.go").write_text(
            "package main\n"
            "\n"
            "import \"github.com/mental1104/common/golang/mental1104\"\n"
            "\n"
            "func main() {\n"
            "  if !mental1104.Contains(\"abc\", \"a\") {\n"
            "    panic(\"verify failed\")\n"
            "  }\n"
            "}\n"
        )
        run([go_bin, "mod", "init", "verify-mental1104"], env=env_go, cwd=tmp_path)
        run([go_bin, "mod", "edit", f"-replace={module_path}={GO_DIR}"], env=env_go, cwd=tmp_path)
        run([go_bin, "mod", "tidy"], env=env_go, cwd=tmp_path)
        run([go_bin, "build", "."], env=env_go, cwd=tmp_path)


def _verify_rust(env: Mapping[str, str]) -> None:
    if not shutil.which("cargo"):
        raise SystemExit("[error] missing cargo")
    env_rust = dict(env)
    env_rust["CARGO_NET_OFFLINE"] = "true"

    with tempfile.TemporaryDirectory(prefix="m1104-verify-rust-") as tmp:
        tmp_path = Path(tmp)
        (tmp_path / "src").mkdir(parents=True, exist_ok=True)
        (tmp_path / "Cargo.toml").write_text(
            "[package]\n"
            "name = \"verify_mental1104\"\n"
            "version = \"0.1.0\"\n"
            "edition = \"2021\"\n"
            "\n"
            "[dependencies]\n"
            f"mental1104 = {{ path = \"{RUST_DIR}\" }}\n"
        )
        (tmp_path / "src" / "main.rs").write_text(
            "use mental1104::collections::contains;\n"
            "\n"
            "fn main() {\n"
            "  let v = vec![1, 2, 3];\n"
            "  assert!(contains(v.as_slice(), &2));\n"
            "}\n"
        )
        run(["cargo", "build", "--release", "--offline"], env=env_rust, cwd=tmp_path)
        run([str(tmp_path / "target" / "release" / "verify_mental1104")], env=env_rust, cwd=tmp_path)
    print("rust-ok")


def _verify_dotnet(env: Mapping[str, str]) -> None:
    dotnet = env.get("DOTNET", "dotnet")
    dotnet_dir = ROOT / "dotnet"
    feed_dir = Path(env.get("NUGET_LOCAL_FEED") or env.get("DOTNET_LOCAL_FEED") or ROOT / "artifacts" / "nuget")
    feed_dir.mkdir(parents=True, exist_ok=True)
    if not list(feed_dir.glob("Mental1104*.nupkg")):
        run(
            [
                dotnet,
                "pack",
                str(dotnet_dir / "src" / "Mental1104" / "Mental1104.csproj"),
                "--configuration",
                env.get("DOTNET_CONFIGURATION", "Release"),
                "--output",
                str(feed_dir),
            ],
            env=env,
            cwd=dotnet_dir,
        )

    with tempfile.TemporaryDirectory(prefix="m1104-verify-dotnet-") as tmp:
        tmp_path = Path(tmp)
        run([dotnet, "new", "console", "--framework", "net8.0"], env=env, cwd=tmp_path)
        run([dotnet, "add", "package", "Mental1104", "--source", str(feed_dir)], env=env, cwd=tmp_path)
        (tmp_path / "Program.cs").write_text(
            "using System;\n"
            "using Mental1104.Executables;\n"
            "\n"
            "Console.WriteLine(\n"
            "    ExeChecker.IsValidExe(Environment.GetCommandLineArgs()[0]) ? \"ok\" : \"fail\"\n"
            ");\n"
        )
        run([dotnet, "build", "--configuration", env.get("DOTNET_CONFIGURATION", "Release")], env=env, cwd=tmp_path)
    print("dotnet-ok")


@register("verify-install")
def configure(subparsers: "ArgumentParser"):
    parser = subparsers.add_parser("verify-install", help="Verify install outputs", aliases=["install-verify"])
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "dotnet", "all"], nargs="?", default="all")
    parser.add_argument("--prefix", help="Install prefix for C++ verification")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run_verify)
    return run_verify


def run_verify(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, prefix=args.prefix)
    if args.target in ("cpp", "all"):
        _verify_cpp(env)
    if args.target in ("python", "all"):
        _verify_python(env)
    if args.target in ("go", "all"):
        _verify_go(env)
    if args.target in ("rust", "all"):
        _verify_rust(env)
    if args.target in ("dotnet", "all"):
        _verify_dotnet(env)
