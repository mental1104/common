from __future__ import annotations

import shutil
from typing import Mapping

from devtool.commands.common import ROOT, run
from devtool.commands.ops import docker as docker_ops

JAVA_DIR = ROOT / "java" / "flink-datastream-demo"
COMPOSE_FILE = ROOT / "devops" / "images" / "flink" / "docker-compose.yaml"
JOB_CLASS = "com.mental1104.flink.examples.SimpleJob"
JAR_NAME = "flink-datastream-demo.jar"


def _mvn(env: Mapping[str, str]) -> str:
    return env.get("MVN", env.get("MAVEN", "mvn"))


def _compose_cmd(env: Mapping[str, str]) -> list[str]:
    cmd = docker_ops._detect_compose(env)
    if cmd is None:
        raise SystemExit("[err] 未检测到 docker compose（docker compose 或 docker-compose）")
    return cmd


def _docker_bin() -> str:
    docker_bin = shutil.which("docker")
    if not docker_bin:
        raise SystemExit("[err] 未检测到 docker，请先安装/配置 Docker")
    return docker_bin


def _ensure_compose_file() -> None:
    if not COMPOSE_FILE.exists():
        raise SystemExit(f"[err] 未找到 compose 文件：{COMPOSE_FILE}")

def setup(env: Mapping[str, str]) -> None:
    run(["java", "-version"], env=env, cwd=ROOT)
    run([_mvn(env), "-version"], env=env, cwd=ROOT)


def build(env: Mapping[str, str], *, skip_tests: bool = True) -> None:
    cmd = [_mvn(env), "-q"]
    if skip_tests:
        cmd.append("-DskipTests")
    cmd.append("package")
    run(cmd, env=env, cwd=JAVA_DIR)


def test(env: Mapping[str, str]) -> None:
    run([_mvn(env), "-q", "test"], env=env, cwd=JAVA_DIR)

def coverage(env: Mapping[str, str]) -> None:
    run(
        [
            _mvn(env),
            "-q",
            "jacoco:prepare-agent",
            "test",
            "jacoco:report",
        ],
        env=env,
        cwd=JAVA_DIR,
    )


def run_local(env: Mapping[str, str]) -> None:
    run([_mvn(env), "-q", "-DskipTests", "compile", "exec:java"], env=env, cwd=JAVA_DIR)


def docker_up(env: Mapping[str, str]) -> None:
    _ensure_compose_file()
    cmd = _compose_cmd(env)
    run([*cmd, "-f", str(COMPOSE_FILE), "up", "-d"], env=env, cwd=ROOT)


def docker_down(env: Mapping[str, str]) -> None:
    _ensure_compose_file()
    cmd = _compose_cmd(env)
    run([*cmd, "-f", str(COMPOSE_FILE), "down"], env=env, cwd=ROOT)


def submit_job(env: Mapping[str, str]) -> None:
    jar_path = JAVA_DIR / "target" / JAR_NAME
    if not jar_path.exists():
        raise SystemExit(f"[err] 未找到 jar，请先构建：{jar_path}")
    docker_bin = _docker_bin()
    run([docker_bin, "exec", "flink-jobmanager", "mkdir", "-p", "/opt/flink/usrlib"], env=env, cwd=ROOT)
    run(
        [
            docker_bin,
            "cp",
            str(jar_path),
            f"flink-jobmanager:/opt/flink/usrlib/{JAR_NAME}",
        ],
        env=env,
        cwd=ROOT,
    )
    run(
        [
            docker_bin,
            "exec",
            "flink-jobmanager",
            "./bin/flink",
            "run",
            "-c",
            JOB_CLASS,
            f"/opt/flink/usrlib/{JAR_NAME}",
        ],
        env=env,
        cwd=ROOT,
    )


def run_docker(env: Mapping[str, str], *, build_jar: bool = True, up: bool = True) -> None:
    if build_jar:
        build(env, skip_tests=True)
    if up:
        docker_up(env)
    submit_job(env)
