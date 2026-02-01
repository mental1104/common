from __future__ import annotations

from argparse import ArgumentParser

from devtool.commands import register
from devtool.commands.common import base_env
from devtool.commands.ops import java as java_ops


@register("run-java")
def configure(subparsers: ArgumentParser):
    parser = subparsers.add_parser("run-java", help="Run Java Flink demo locally (exec:java)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run)
    return run


def run(args):
    env = base_env(verbose=args.verbose)
    java_ops.run_local(env)


@register("run-java-docker")
def configure_docker(subparsers: ArgumentParser):
    parser = subparsers.add_parser("run-java-docker", help="Build jar, start Flink compose, submit job")
    parser.add_argument("--no-build", action="store_true", help="Skip mvn package")
    parser.add_argument("--no-up", action="store_true", help="Skip docker compose up")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run_docker)
    return run_docker


def run_docker(args):
    env = base_env(verbose=args.verbose)
    java_ops.run_docker(env, build_jar=not args.no_build, up=not args.no_up)


@register("docker-java-up")
def configure_docker_up(subparsers: ArgumentParser):
    parser = subparsers.add_parser("docker-java-up", help="Start Flink compose for Java demo")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run_docker_up)
    return run_docker_up


def run_docker_up(args):
    env = base_env(verbose=args.verbose)
    java_ops.docker_up(env)


@register("docker-java-down")
def configure_docker_down(subparsers: ArgumentParser):
    parser = subparsers.add_parser("docker-java-down", help="Stop Flink compose for Java demo")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.set_defaults(_runner=run_docker_down)
    return run_docker_down


def run_docker_down(args):
    env = base_env(verbose=args.verbose)
    java_ops.docker_down(env)
