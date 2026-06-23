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
from devtool.commands.ops import java as java_ops

if TYPE_CHECKING:
    from argparse import ArgumentParser


def _split_cmd(value: str) -> list[str]:
    return shlex.split(value) if value else []


def _require_installed(env: Mapping[str, str]) -> bool:
    return str(env.get("VERIFY_REQUIRE_INSTALL", "")).strip().lower() in ("1", "true", "yes", "on")


def _with_lib_path(env: Mapping[str, str], lib_dir: Path, bin_dir: Path | None = None) -> dict[str, str]:
    merged = dict(env)
    if sys.platform.startswith("win"):
        key = "PATH"
        existing = merged.get(key, "")
        paths = []
        for candidate in (bin_dir, lib_dir):
            if candidate and candidate.exists():
                paths.append(str(candidate))
        if paths:
            merged[key] = os.pathsep.join(paths + ([existing] if existing else []))
        return merged
    key = "LD_LIBRARY_PATH" if sys.platform.startswith("linux") else "DYLD_LIBRARY_PATH"
    existing = merged.get(key, "")
    if lib_dir.exists():
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


def _go_bin_dir(env: Mapping[str, str]) -> Path | None:
    go_bin = env.get("GO", "go")
    bin_dir = env.get("GOBIN", "").strip()
    if not bin_dir:
        bin_dir = os.popen(f'{go_bin} env GOBIN').read().strip()
    if not bin_dir:
        gopath = os.popen(f'{go_bin} env GOPATH').read().strip()
        if gopath:
            bin_dir = os.path.join(gopath.split(os.pathsep)[0], "bin")
    if not bin_dir:
        return None
    return Path(bin_dir)


def _cargo_bin_dir(env: Mapping[str, str]) -> Path:
    cargo_home = (env.get("CARGO_HOME") or "").strip()
    base = Path(cargo_home) if cargo_home else Path.home() / ".cargo"
    return base / "bin"


def _verify_cpp(env: Mapping[str, str]) -> None:
    prefix = Path(env.get("PREFIX", "/usr/local"))
    include_dir = prefix / "include"
    lib_dir = prefix / "lib"
    bin_dir = prefix / "bin"
    if not (include_dir / "mental1104").exists():
        raise SystemExit(f"[error] missing C++ headers under {include_dir}")
    if sys.platform.startswith("win"):
        if not any(lib_dir.glob("mental1104*.lib")):
            raise SystemExit(f"[error] missing mental1104.lib under {lib_dir}")
        if not any(bin_dir.glob("mental1104*.dll")):
            raise SystemExit(f"[error] missing mental1104.dll under {bin_dir}")
    else:
        if not any(lib_dir.glob("libmental1104*")):
            raise SystemExit(f"[error] missing libmental1104 under {lib_dir}")
        cxx = env.get("CXX", "c++")
        if not shutil.which(cxx.split()[0]):
            raise SystemExit(f"[error] missing C++ compiler: {cxx}")

    with tempfile.TemporaryDirectory(prefix="m1104-verify-cpp-") as tmp:
        tmp_path = Path(tmp)
        src = tmp_path / "verify.cpp"
        src.write_text(
            "#include <mental1104/concurrency/thread/thread_util.h>\n"
            "\n"
            "int main() {\n"
            "  ThreadPool pool(1);\n"
            "  auto fut = pool.submit([] { return 7; });\n"
            "  return fut.get() == 7 ? 0 : 1;\n"
            "}\n"
        )
        if sys.platform.startswith("win"):
            cmake = env.get("CMAKE", "cmake")
            build_dir = tmp_path / "build"
            config = env.get("CMAKE_INSTALL_CONFIG_NAME") or env.get("CPP_BUILD_TYPE") or "Debug"
            cmake_lists = tmp_path / "CMakeLists.txt"
            cmake_lists.write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(verify_mental1104 LANGUAGES CXX)\n"
                "set(CMAKE_CXX_STANDARD 20)\n"
                "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
                "add_executable(verify_cpp verify.cpp)\n"
                f"target_include_directories(verify_cpp PRIVATE \"{include_dir.as_posix()}\")\n"
                f"find_library(M1104_LIB NAMES mental1104 PATHS \"{lib_dir.as_posix()}\" NO_DEFAULT_PATH)\n"
                "if(NOT M1104_LIB)\n"
                f"  message(FATAL_ERROR \"missing mental1104.lib under {lib_dir.as_posix()}\")\n"
                "endif()\n"
                "target_link_libraries(verify_cpp PRIVATE \"${M1104_LIB}\")\n"
            )
            cmake_args = [cmake, "-S", str(tmp_path), "-B", str(build_dir)]
            generator = env.get("CMAKE_GENERATOR")
            if generator:
                cmake_args += ["-G", generator]
            run(cmake_args, env=env, cwd=tmp_path)
            run([cmake, "--build", str(build_dir), "--config", config], env=env, cwd=tmp_path)
            exe = build_dir / config / "verify_cpp.exe"
            if not exe.exists():
                exe = build_dir / "verify_cpp.exe"
            if not exe.exists():
                raise SystemExit(f"[error] missing verify_cpp binary at {exe}")
            run([str(exe)], env=_with_lib_path(env, lib_dir, bin_dir=bin_dir), cwd=tmp_path)
        else:
            exe = tmp_path / "verify_cpp"
            use_system_include = (prefix.resolve() == Path("/usr/local"))
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
    if _require_installed(env):
        bin_dir = _go_bin_dir(env)
        if not bin_dir:
            raise SystemExit("[error] missing GOBIN/GOPATH; cannot verify installed Go binary")
        exe_name = "mental1104-go-verify.exe" if sys.platform.startswith("win") else "mental1104-go-verify"
        verify_bin = bin_dir / exe_name
        if not verify_bin.exists():
            raise SystemExit(f"[error] missing Go install verify binary at {verify_bin}")
        run([str(verify_bin)], env=env, cwd=GO_DIR)
        return
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
        run([go_bin, "build", "-buildvcs=false", "."], env=env_go, cwd=tmp_path)


def _verify_java(env: Mapping[str, str]) -> None:
    java_bin = env.get("JAVA", "java")
    javac_bin = env.get("JAVAC", "javac")
    if not shutil.which(java_bin):
        raise SystemExit(f"[error] missing java: {java_bin}")
    if not shutil.which(javac_bin):
        raise SystemExit(f"[error] missing javac: {javac_bin}")
    if _require_installed(env):
        jar_path = java_ops.installed_jar_path(env)
        if not jar_path.exists():
            raise SystemExit(f"[error] missing Java jar at {jar_path}; run ./dev install-java")
    else:
        jar_path = java_ops.target_jar_path()
        if not jar_path.exists():
            raise SystemExit(f"[error] missing Java jar at {jar_path}; run ./dev build-java")

    with tempfile.TemporaryDirectory(prefix="m1104-verify-java-") as tmp:
        tmp_path = Path(tmp)
        src = tmp_path / "Verify.java"
        src.write_text(
            "import com.mental1104.common.Contains;\n"
            "\n"
            "public class Verify {\n"
            "  public static void main(String[] args) {\n"
            "    if (!Contains.contains(\"abc\", \"a\")) {\n"
            "      throw new RuntimeException(\"verify failed\");\n"
            "    }\n"
            "    int[] nums = new int[] {1, 2, 3};\n"
            "    if (!Contains.contains(nums, 2)) {\n"
            "      throw new RuntimeException(\"verify failed\");\n"
            "    }\n"
            "  }\n"
            "}\n"
        )
        classes_dir = tmp_path / "classes"
        classes_dir.mkdir(parents=True, exist_ok=True)
        run([javac_bin, "-d", str(classes_dir), "-cp", str(jar_path), str(src)], env=env, cwd=tmp_path)
        classpath = f"{classes_dir}{os.pathsep}{jar_path}"
        run([java_bin, "-cp", classpath, "Verify"], env=env, cwd=tmp_path)
    print("java-ok")


def _verify_rust(env: Mapping[str, str]) -> None:
    if not shutil.which("cargo"):
        raise SystemExit("[error] missing cargo")
    env_rust = dict(env)
    env_rust["CARGO_NET_OFFLINE"] = "true"
    if _require_installed(env):
        bin_dir = _cargo_bin_dir(env_rust)
        exe_name = "mental1104-verify.exe" if sys.platform.startswith("win") else "mental1104-verify"
        verify_bin = bin_dir / exe_name
        if not verify_bin.exists():
            raise SystemExit(f"[error] missing Rust install verify binary at {verify_bin}")
        run([str(verify_bin)], env=env_rust, cwd=RUST_DIR)
        print("rust-ok")
        return
    rust_path = RUST_DIR.as_posix() if sys.platform.startswith("win") else str(RUST_DIR)

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
            f"mental1104 = {{ path = \"{rust_path}\" }}\n"
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
    if _require_installed(env) and not list(feed_dir.glob("Mental1104*.nupkg")):
        raise SystemExit(f"[error] missing Mental1104 packages under {feed_dir}")
    if not _require_installed(env) and not list(feed_dir.glob("Mental1104*.nupkg")):
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
    parser.add_argument("target", choices=["python", "go", "cpp", "rust", "dotnet", "java", "all"], nargs="?", default="all")
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
    if args.target in ("java", "all"):
        _verify_java(env)
    if args.target in ("rust", "all"):
        _verify_rust(env)
    if args.target in ("dotnet", "all"):
        _verify_dotnet(env)
