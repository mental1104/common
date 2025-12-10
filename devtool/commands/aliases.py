from __future__ import annotations

from argparse import ArgumentParser, REMAINDER
from types import SimpleNamespace

from devtool.commands import register
import devtool.commands.bench as bench
import devtool.commands.build as build
import devtool.commands.clean as clean
import devtool.commands.coverage as coverage
import devtool.commands.fmt as fmt
import devtool.commands.guard as guard
import devtool.commands.install as install
import devtool.commands.test as test
import devtool.commands.uninstall as uninstall
import devtool.commands.vet as vet


def _add_common(parser: ArgumentParser, *, jobs: bool = True, verbose: bool = True) -> None:
    if jobs:
        parser.add_argument("--jobs", type=int, help="Parallelism hint")
    if verbose:
        parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")


def _alias_build(name: str, target: str, *, default_config: str | None = None):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for build {target})")
        if target == "cpp":
            parser.add_argument("--config", default=default_config or "Debug", help="CMake build type")
        _add_common(parser)
        parser.set_defaults(_runner=lambda args: build.run(SimpleNamespace(
            target=target,
            config=getattr(args, "config", default_config or "Debug"),
            jobs=getattr(args, "jobs", None),
            verbose=getattr(args, "verbose", False),
        )))
        return parser.set_defaults


def _alias_test(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for test {target})")
        parser.add_argument("--filter", dest="filter_expr", help="Filter expression")
        parser.add_argument("--file", help="File pattern")
        _add_common(parser)
        parser.add_argument("extra_args", nargs=REMAINDER, help="Extra args (e.g. passed to pytest via --)")
        parser.set_defaults(_runner=lambda args: test.run(SimpleNamespace(
            target=target,
            filter_expr=getattr(args, "filter_expr", None),
            file=getattr(args, "file", None),
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
            extra_args=getattr(args, "extra_args", []),
        )))
        return parser.set_defaults


def _alias_coverage(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for coverage {target})")
        parser.add_argument("--filter", dest="filter_expr", help="Filter expression")
        parser.add_argument("--file", help="File pattern")
        _add_common(parser)
        parser.set_defaults(_runner=lambda args: coverage.run(SimpleNamespace(
            target=target,
            filter_expr=getattr(args, "filter_expr", None),
            file=getattr(args, "file", None),
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


def _alias_fmt(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for fmt {target})")
        _add_common(parser, jobs=True, verbose=True)
        parser.set_defaults(_runner=lambda args: fmt.run(SimpleNamespace(
            target=target,
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


def _alias_bench(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for bench {target})")
        parser.add_argument("--filter", dest="filter_expr", help="Filter expression")
        parser.add_argument("--file", help="File pattern")
        _add_common(parser)
        parser.set_defaults(_runner=lambda args: bench.run(SimpleNamespace(
            target=target,
            filter_expr=getattr(args, "filter_expr", None),
            file=getattr(args, "file", None),
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


def _alias_install(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for install {target})")
        if target == "cpp":
            parser.add_argument("--prefix", help="Install prefix")
        _add_common(parser, jobs=True, verbose=True)
        parser.set_defaults(_runner=lambda args: install.run(SimpleNamespace(
            target=target,
            prefix=getattr(args, "prefix", None),
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


def _alias_uninstall(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for uninstall {target})")
        _add_common(parser, jobs=False, verbose=True)
        parser.set_defaults(_runner=lambda args: uninstall.run(SimpleNamespace(
            target=target,
            verbose=getattr(args, "verbose", False),
            jobs=None,
        )))
        return parser.set_defaults


def _alias_clean(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for clean {target})")
        _add_common(parser, jobs=True, verbose=True)
        parser.set_defaults(_runner=lambda args: clean.run(SimpleNamespace(
            target=target,
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


def _alias_vet(name: str, target: str):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for vet {target})")
        _add_common(parser, jobs=True, verbose=True)
        parser.set_defaults(_runner=lambda args: vet.run(SimpleNamespace(
            target=target,
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


def _alias_guard(name: str, target: str, *, allow_mode: bool = False, preset_mode: str | None = None):
    @register(name)
    def configure(subparsers: ArgumentParser):
        parser = subparsers.add_parser(name, help=f"{name} (alias for guard {target})")
        if allow_mode:
            parser.add_argument("--mode", choices=["mem", "race", "miri", "heap", "all"], help="Guard mode")
        parser.add_argument("--filter", dest="filter_expr", help="Filter expression")
        parser.add_argument("--file", help="File pattern")
        _add_common(parser, jobs=True, verbose=True)
        parser.set_defaults(_runner=lambda args: guard.run(SimpleNamespace(
            target=target,
            mode=preset_mode or getattr(args, "mode", None),
            filter_expr=getattr(args, "filter_expr", None),
            file=getattr(args, "file", None),
            verbose=getattr(args, "verbose", False),
            jobs=getattr(args, "jobs", None),
        )))
        return parser.set_defaults


for lang in ("python", "go", "cpp", "rust"):
    _alias_build(f"build-{lang}", lang)
_alias_build("build-cpp-release", "cpp", default_config="Release")
_alias_build("build-cpp-debug", "cpp", default_config="Debug")

for lang in ("python", "go", "cpp", "rust"):
    _alias_test(f"test-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_coverage(f"coverage-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_fmt(f"fmt-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_bench(f"bench-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_install(f"install-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_uninstall(f"uninstall-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_clean(f"clean-{lang}", lang)
for lang in ("python", "go", "cpp", "rust"):
    _alias_vet(f"vet-{lang}", lang)

_alias_guard("guard-python", "python")
_alias_guard("guard-go", "go")
_alias_guard("guard-cpp", "cpp", allow_mode=True)
_alias_guard("guard-rust", "rust", allow_mode=True)
_alias_guard("guard-rust-mem", "rust", allow_mode=True, preset_mode="mem")
_alias_guard("guard-rust-race", "rust", allow_mode=True, preset_mode="race")
_alias_guard("guard-rust-miri", "rust", allow_mode=True, preset_mode="miri")
