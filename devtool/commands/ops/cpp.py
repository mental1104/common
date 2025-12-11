from __future__ import annotations

import platform
import re
import shutil
import subprocess
from pathlib import Path
from typing import Mapping

from devtool.commands.common import (
    BENCH_ARTIFACT_ROOT,
    BOOST_SPARSE_LIST,
    BOOST_REQUIRED_SUBMODULES,
    CPP_BENCH_ARTIFACT_DIR,
    CPP_BUILD_DIR,
    CPP_SRC_DIR,
    EXPORT_CPP_BUILD_DIR,
    ROOT,
    ensure_dir,
    run,
    strip_proxies,
)
from devtool.context import is_windows


_GITMODULE_PATH_RE = re.compile(r"^\s*path\s*=\s*(.+)$")
_BOOST_SUBMODULE_PATH = ROOT / "cpp" / "lib" / "boost"
_BOOST_REL_PATH = "cpp/lib/boost"


def _gitmodule_paths() -> list[str]:
    gm = ROOT / ".gitmodules"
    if not gm.exists():
        return []
    paths: list[str] = []
    for raw in gm.read_text().splitlines():
        m = _GITMODULE_PATH_RE.match(raw)
        if m:
            rel = m.group(1).strip()
            if rel:
                paths.append(rel)
    return paths


def _skip_list(env: Mapping[str, str]) -> set[str]:
    raw = env.get("SKIP_SUBMODULES", "")
    items = [x.strip() for part in raw.replace(",", " ").split() for x in [part] if x.strip()]
    return set(items)

def _wanted_list(env: Mapping[str, str], all_paths: list[str]) -> list[str]:
    raw = env.get("BUILD_SUBMODULES", "")
    if raw.strip().lower() == "all":
        return all_paths
    parts = [x.strip() for part in raw.replace(",", " ").split() for x in [part] if x.strip()]
    if parts:
        return parts
    # default: only the ones that actually produce needed libs
    return ["cpp/lib/cJSON", "cpp/lib/hiredis", "cpp/lib/redis-plus-plus"]


def git_submodules(env: Mapping[str, str]) -> None:
    git_dir = ROOT / ".git"
    if not git_dir.exists():
        return
    env_np = strip_proxies(env)
    skip = _skip_list(env_np)
    run(["git", "submodule", "sync", "--recursive"], env=env_np, cwd=ROOT)
    all_paths = _gitmodule_paths()
    non_boost = [p for p in all_paths if p and p != _BOOST_REL_PATH]
    if skip:
        non_boost = [p for p in non_boost if p not in skip]
    if non_boost:
        base_cmd = [
            "git",
            "submodule",
            "update",
            "--init",
            "--recursive",
            "--jobs",
            env_np.get("JOBS", "1"),
        ]
        update_cmd = list(base_cmd) + ["--depth", "1", "--filter=blob:none", "--", *non_boost]
        try:
            run(update_cmd, env=env_np, cwd=ROOT)
        except subprocess.CalledProcessError:
            print("[warn] 子模块 partial clone 失败，正在回退为完整检出（非 boost）")
            run(base_cmd + ["--", *non_boost], env=env_np, cwd=ROOT)
    if _BOOST_REL_PATH not in skip:
        _update_boost_root(env_np)
        _apply_boost_sparse_checkout(env_np)
        _update_boost_modules(env_np)


def _load_boost_sparse_patterns() -> list[str]:
    if not BOOST_SPARSE_LIST.exists():
        return []
    patterns: list[str] = []
    for raw in BOOST_SPARSE_LIST.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            patterns.append(line)
    return patterns


def _apply_boost_sparse_checkout(env: Mapping[str, str]) -> None:
    if env.get("BOOST_FULL_CHECKOUT", "").lower() in {"1", "true", "yes"}:
        return
    if not _BOOST_SUBMODULE_PATH.exists():
        return
    patterns = _load_boost_sparse_patterns()
    if not patterns:
        return
    git_base = ["git", "-C", str(_BOOST_SUBMODULE_PATH), "sparse-checkout"]
    run_env = {k: str(v) for k, v in env.items()}
    try:
        subprocess.run(git_base + ["init", "--cone"], cwd=str(ROOT), env=run_env, check=True)
        subprocess.run(
            git_base + ["set", "--stdin"],
            cwd=str(ROOT),
            env=run_env,
            input=("\n".join(patterns) + "\n").encode(),
            check=True,
        )
    except subprocess.CalledProcessError as exc:
        print(f"[warn] Boost sparse-checkout 失败（{exc}），已回退为全量检出")


def _load_boost_required_modules() -> list[str]:
    if not BOOST_REQUIRED_SUBMODULES.exists():
        return []
    modules: list[str] = []
    for raw in BOOST_REQUIRED_SUBMODULES.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            modules.append(line)
    return modules


def _update_boost_root(env: Mapping[str, str]) -> None:
    cmd = [
        "git",
        "submodule",
        "update",
        "--init",
        "--depth",
        "1",
        "--",
        _BOOST_REL_PATH,
    ]
    try:
        run(cmd, env=env, cwd=ROOT)
    except subprocess.CalledProcessError:
        print("[warn] Boost partial clone 失败，正在回退为完整检出")
        run(["git", "submodule", "update", "--init", "--", _BOOST_REL_PATH], env=env, cwd=ROOT)


def _update_boost_modules(env: Mapping[str, str]) -> None:
    if env.get("BOOST_FULL_CHECKOUT", "").lower() in {"1", "true", "yes"}:
        run(["git", "-C", str(_BOOST_SUBMODULE_PATH), "submodule", "update", "--init", "--recursive"], env=env)
        return
    modules = _load_boost_required_modules()
    if not modules or not _BOOST_SUBMODULE_PATH.exists():
        return
    cmd = [
        "git",
        "-C",
        str(_BOOST_SUBMODULE_PATH),
        "submodule",
        "update",
        "--init",
        "--recursive",
        "--",
        *modules,
    ]
    try:
        run(cmd, env=env)
    except subprocess.CalledProcessError:
        print("[warn] Boost 内部子模块 partial clone 失败，正在回退为完整检出所需模块")
        run(["git", "-C", str(_BOOST_SUBMODULE_PATH), "submodule", "update", "--init", "--recursive", "--", *modules], env=env)
    _ensure_boost_headers(env, modules)


def _ensure_boost_headers(env: Mapping[str, str], modules: list[str]) -> None:
    required = [
        _BOOST_SUBMODULE_PATH / "boost" / "asio" / "post.hpp",
        _BOOST_SUBMODULE_PATH / "boost" / "multiprecision" / "mpfr.hpp",
    ]
    missing = [p for p in required if not p.exists()]
    if not missing:
        return
    print("[warn] Boost sparse checkout missing headers, fetching required submodules recursively")
    try:
        run(
            ["git", "-C", str(_BOOST_SUBMODULE_PATH), "submodule", "update", "--init", "--recursive", "--depth", "1", "--", *modules],
            env=env,
        )
    except Exception:
        # fall back to full checkout
        print("[warn] Boost targeted fetch failed，fallback to full checkout")
        run(["git", "-C", str(_BOOST_SUBMODULE_PATH), "submodule", "update", "--init", "--recursive"], env=env)


def build_submodules(env: Mapping[str, str]) -> None:
    skip = _skip_list(env)
    paths = _gitmodule_paths()
    wanted = _wanted_list(env, paths)
    extra_cmake_args = {
        "cpp/lib/cJSON": ["-DENABLE_CUSTOM_COMPILER_FLAGS=OFF"],
        "cpp/lib/hiredis": ["-DDISABLE_TESTS=ON"],
    }
    for rel in paths:
        if rel not in wanted or rel in skip:
            continue
        path = ROOT / rel
        if not path.exists() or not (path / "CMakeLists.txt").exists():
            continue
        build_dir = path / "build"
        ensure_dir(build_dir)
        try:
            cmake_args = [
                env["CMAKE"],
                "-S",
                str(path),
                "-B",
                str(build_dir),
                f'-DCMAKE_BUILD_TYPE={env["CPP_BUILD_TYPE"]}',
            ]
            cmake_args += extra_cmake_args.get(rel, [])
            run(cmake_args, env=env)
            run([env["CMAKE"], "--build", str(build_dir), "--parallel", env["JOBS"]], env=env)
        except Exception:
            print(f"[warn] 子模块 {rel} 构建失败，已跳过（可用 BUILD_SUBMODULES=all 重新尝试）")


def configure(env: Mapping[str, str]) -> None:
    ensure_dir(CPP_BUILD_DIR)
    extra = []
    if platform.system().lower() == "windows":
        extra += ["-DENABLE_COVERAGE=OFF", "-DCOVERAGE=OFF"]
    venv_py = env.get("PY_VENV_PYTHON")
    if venv_py and Path(venv_py).exists():
        try:
            out = Path(venv_py)
            res = out.parent
        except Exception:
            res = None
        if res:
            pass
    run(
        [
            env["CMAKE"],
            "-S",
            str(CPP_SRC_DIR),
            "-B",
            str(CPP_BUILD_DIR),
            f'-DCMAKE_BUILD_TYPE={env["CPP_BUILD_TYPE"]}',
            "-DPYBIND11_FINDPYTHON=ON",
            *extra,
        ],
        env=env,
    )


def build(env: Mapping[str, str]) -> None:
    run([env["CMAKE"], "--build", str(CPP_BUILD_DIR), "--parallel", env["JOBS"]], env=env)


def test(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    cache = CPP_BUILD_DIR / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line.startswith("CMAKE_BUILD_TYPE:STRING=") and "Debug" not in line:
                raise SystemExit(f"[error] {CPP_BUILD_DIR} 不是 Debug 构建，请先 ./dev build cpp --config Debug")
    args = [env_np["CTEST"], "--output-on-failure", "-LE", "bench", "-j", env_np["JOBS"]]
    if env_np.get("CTEST_V"):
        args.append(env_np["CTEST_V"])
    if file_pattern:
        args += ["-R", file_pattern]
    cmd_env = dict(env_np)
    if filter_expr:
        cmd_env["GTEST_FILTER"] = filter_expr
    run(args, env=cmd_env, cwd=CPP_BUILD_DIR)


def coverage(env: Mapping[str, str]) -> None:
    import platform

    if platform.system().lower() == "windows":
        print("[info] coverage-cpp not supported on Windows (gcov/lcov unavailable); skipping")
        return
    env_np = strip_proxies(env)
    run([env_np["CTEST"], "--output-on-failure", "-LE", "bench"], env=env_np, cwd=CPP_BUILD_DIR)
    gcovr_bin = shutil.which("gcovr")
    gcov_bin = env_np.get("GCOV", "gcov")
    if gcovr_bin:
        args = [
            gcovr_bin,
            "-r",
            "..",
            "--object-directory",
            ".",
            "--gcov-executable",
            gcov_bin,
            "--exclude",
            "(^|.*/)(test|bench|external|gtest|lib|thirdparty|overlay)/",
            "--exclude",
            "/usr/include/.*",
            "--exclude-directories",
            ".*/lib/.*",
            "--exclude-directories",
            ".*/thirdparty/.*",
            "--exclude-directories",
            ".*/overlay/.*",
            "--exclude-directories",
            ".*/build-(asan|tsan|ubsan|msan).*",
            "--gcov-ignore-parse-errors",
            "--merge-mode-functions=separate",
            "--txt",
            "--print-summary",
        ]
        proc = subprocess.run(
            args,
            cwd=CPP_BUILD_DIR,
            env=env_np,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        print(proc.stdout, end="")
        if proc.returncode == 0:
            return
        print("[warn] gcovr 失败，回退使用 gcov 简单汇总")
    # fallback: per-directory gcov (verbose)
    gcov_exec = shutil.which(gcov_bin) or shutil.which("gcov")
    if not gcov_exec:
        print("[warn] 未找到 gcov，可设置 GCOV 环境变量指定可执行文件")
        return
    gcno_files = [p for p in CPP_BUILD_DIR.rglob("*.gcno")]
    if not gcno_files:
        print("[warn] 未找到 .gcno 文件，请确认已用覆盖率编译构建（-fprofile-arcs -ftest-coverage）")
        return
    groups: dict[Path, list[str]] = {}
    for path in gcno_files:
        groups.setdefault(path.parent, []).append(path.name)
    had_err = False
    total_dirs = len(groups)
    for idx, (dir_path, names) in enumerate(groups.items(), start=1):
        print(f"[cov-fallback] ({idx}/{total_dirs}) gcov in {dir_path}")
        proc = subprocess.run(
            [gcov_exec, "-b", "-c", *names],
            cwd=dir_path,
            env=env_np,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        print(proc.stdout, end="")
        if proc.returncode != 0:
            had_err = True
            print(f"[warn] gcov 退出码 {proc.returncode} @ {dir_path}")
    if had_err:
        print("[warn] 覆盖率生成有部分错误，请查看上方 gcov 输出")


def fmt(env: Mapping[str, str]) -> None:
    clang = shutil.which("clang-format")
    if not clang:
        raise SystemExit("[error] 未找到 clang-format")
    files = [
        str(p)
        for p in CPP_SRC_DIR.rglob("*")
        if p.suffix in {".h", ".hh", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx"}
        and "thirdparty" not in p.parts
        and "lib" not in p.parts
    ]
    if files:
        run([clang, "-i", *files], env=env)


def bench(env: Mapping[str, str], *, file_pattern: str | None, filter_expr: str | None) -> None:
    env_np = strip_proxies(env)
    if not CPP_BUILD_DIR.exists():
        raise SystemExit("[info] 未发现 cpp/build，请先 ./dev build cpp")
    binaries = [p for p in (CPP_BUILD_DIR / "bin").glob("bench_*") if p.is_file()]
    ensure_dir(Path(CPP_BENCH_ARTIFACT_DIR) / "plots")
    for exe in binaries:
        if file_pattern and file_pattern not in exe.name:
            continue
        args = [str(exe), f"--benchmark_out={CPP_BENCH_ARTIFACT_DIR}/{exe.name}.json", "--benchmark_out_format=json"]
        if filter_expr:
            args.append(f"--benchmark_filter={filter_expr}")
        run(args, env=env_np)


def install(env: Mapping[str, str]) -> None:
    run([env.get("SUDO", ""), env["CMAKE"], "--install", str(CPP_BUILD_DIR), "--prefix", env["PREFIX"]], env=env)


def uninstall(env: Mapping[str, str]) -> None:
    manifest = CPP_BUILD_DIR / "install_manifest.txt"
    if not manifest.exists():
        raise SystemExit(f"[error] 未找到 {manifest}")
    for line in manifest.read_text().splitlines():
        target = Path(line)
        if target.exists():
            target.unlink()


def clean(env: Mapping[str, str]) -> None:
    shutil.rmtree(CPP_BUILD_DIR, ignore_errors=True)
    # also clean lib/*/build
    for sub in (CPP_SRC_DIR / "lib").glob("*/build"):
        shutil.rmtree(sub, ignore_errors=True)


def vet(env: Mapping[str, str]) -> None:
    tidy = shutil.which("clang-tidy")
    if not tidy:
        raise SystemExit("[error] 未找到 clang-tidy")
    db = CPP_BUILD_DIR / "compile_commands.json"
    if not db.exists():
        raise SystemExit("[hint] 请先生成 compile_commands.json (cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)")
    targets = [p for p in (CPP_SRC_DIR / "test").rglob("*.cpp")]
    for chunk_start in range(0, len(targets), 50):
        chunk = targets[chunk_start : chunk_start + 50]
        run([tidy, "-p", str(CPP_BUILD_DIR), "--quiet", "--warnings-as-errors=*", *map(str, chunk)], env=env)


def guard(env: Mapping[str, str], *, mode: str | None = None) -> None:
    # Simplified: rely on ctest with sanitizers configured externally.
    test(env, file_pattern=None, filter_expr=None)
