from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import rust as rust_ops


@register("clippy-rust")
def configure_clippy(subparsers: ArgumentParser):
    parser = subparsers.add_parser("clippy-rust", help="Run cargo clippy (warnings as errors)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run_clippy)
    return run_clippy


def run_clippy(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    rust_ops.clippy(env)


@register("example-rust")
def configure_example(subparsers: ArgumentParser):
    parser = subparsers.add_parser("example-rust", help="Run example target for rust crate")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.set_defaults(_runner=run_example)
    return run_example


def run_example(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs)
    rust_ops.example(env)
