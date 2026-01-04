from __future__ import annotations

from typing import TYPE_CHECKING

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import cpp as cpp_ops

if TYPE_CHECKING:
    from argparse import ArgumentParser


@register("test-redispp")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("test-redispp", help="Build and run redis-plus-plus tests")
    parser.add_argument("--host", help="Redis host (default: REDIS_HOST)")
    parser.add_argument("--port", type=int, help="Redis port (default: REDIS_PORT)")
    parser.add_argument("--auth", help="Redis password (default: REDISCLI_AUTH/REDIS_PASSWORD)")
    parser.add_argument("--config", default="Debug", help="CMake build type")
    parser.add_argument("--jobs", type=int, help="Parallelism hint")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose, jobs=args.jobs, cpp_build_type=args.config)
    host = args.host or env.get("REDIS_HOST")
    port = args.port or env.get("REDIS_PORT")
    auth = args.auth or env.get("REDISCLI_AUTH") or env.get("REDIS_PASSWORD")
    if not host or not port:
        raise SystemExit("[error] redis host/port not set; use --host/--port or set REDIS_HOST/REDIS_PORT")
    cpp_ops.test_redispp(env, host=host, port=str(port), auth=auth)
