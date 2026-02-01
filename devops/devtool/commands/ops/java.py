from __future__ import annotations

import os
import platform
import re
import shutil
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

from devtool.commands.common import ROOT, run
from devtool.commands.ops import docker as docker_ops

JAVA_DIR = ROOT / "java" / "flink-datastream-demo"
COMPOSE_FILE = ROOT / "devops" / "images" / "flink" / "docker-compose.yaml"
JOB_CLASS = "com.mental1104.flink.examples.SimpleJob"
POM_FILE = JAVA_DIR / "pom.xml"
POM_NS = {"m": "http://maven.apache.org/POM/4.0.0"}


@dataclass(frozen=True)
class JavaCoords:
    group_id: str
    artifact_id: str
    version: str
    final_name: str


def _mvn(env: Mapping[str, str]) -> str:
    return env.get("MVN", env.get("MAVEN", "mvn"))


def _find_pom_text(root: ET.Element, path: str) -> str:
    node = root.find(path, POM_NS)
    if node is None:
        node = root.find(path)
    if node is None or node.text is None:
        return ""
    return node.text.strip()


def read_coords() -> JavaCoords:
    if not POM_FILE.exists():
        raise SystemExit(f"[err] 未找到 pom.xml：{POM_FILE}")
    tree = ET.parse(str(POM_FILE))
    root = tree.getroot()
    group_id = _find_pom_text(root, "m:groupId")
    version = _find_pom_text(root, "m:version")
    if not group_id:
        group_id = _find_pom_text(root, "m:parent/m:groupId")
    if not version:
        version = _find_pom_text(root, "m:parent/m:version")
    artifact_id = _find_pom_text(root, "m:artifactId")
    final_name = _find_pom_text(root, "m:build/m:finalName") or artifact_id
    if not group_id or not artifact_id or not version:
        raise SystemExit(f"[err] pom 坐标缺失：{POM_FILE}")
    return JavaCoords(group_id=group_id, artifact_id=artifact_id, version=version, final_name=final_name)


def _m2_repo(env: Mapping[str, str]) -> Path:
    return Path(env.get("M2_REPO", Path.home() / ".m2" / "repository"))


def _os_label() -> str:
    sys_name = platform.system().lower()
    if sys_name.startswith("linux"):
        return "linux"
    if sys_name.startswith("darwin"):
        return "macos"
    if sys_name.startswith("windows"):
        return "windows"
    return sys_name or "unknown"


def _java_major_version(env: Mapping[str, str]) -> str:
    java_bin = env.get("JAVA", "java")
    output = os.popen(f"{java_bin} -version 2>&1").read()
    match = re.search(r'version \"([0-9]+)(?:\\.([0-9]+))?', output)
    if not match:
        return "unknown"
    major = match.group(1)
    if major == "1" and match.group(2):
        major = match.group(2)
    return major


def target_jar_path() -> Path:
    coords = read_coords()
    return JAVA_DIR / "target" / f"{coords.final_name}.jar"


def installed_jar_path(env: Mapping[str, str]) -> Path:
    coords = read_coords()
    group_path = Path(*coords.group_id.split("."))
    return _m2_repo(env) / group_path / coords.artifact_id / coords.version / f"{coords.artifact_id}-{coords.version}.jar"


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
    xml_path = JAVA_DIR / "target" / "site" / "jacoco" / "jacoco.xml"
    out_path = JAVA_DIR / "target" / "cov.json"
    script = ROOT / "tools" / "ci" / "extract_coverage_java.py"
    py = env.get("PYTHON", "python3")
    lines_mode = str(env.get("JAVA_COVER_LINES", "missed")).strip().lower()
    format_mode = str(env.get("JAVA_COVER_FORMAT", "gcc")).strip().lower()
    cmd = [
        py,
        str(script),
        "--os",
        _os_label(),
        "--java",
        _java_major_version(env),
        "--xml",
        str(xml_path),
        "--out",
        str(out_path),
    ]
    if format_mode in ("gcc", "gcov"):
        cmd.append("--gcc")
    elif format_mode in ("both", "all"):
        cmd += ["--gcc", "--table"]
    else:
        cmd.append("--table")
    if lines_mode and lines_mode not in ("0", "false", "none"):
        cmd += [
            "--lines",
            lines_mode,
            "--source-root",
            str(JAVA_DIR / "src" / "main" / "java"),
        ]
    run(cmd, env=env, cwd=ROOT)


def install(env: Mapping[str, str]) -> None:
    run([_mvn(env), "-q", "-DskipTests", "install"], env=env, cwd=JAVA_DIR)


def uninstall(env: Mapping[str, str]) -> None:
    coords = read_coords()
    group_path = Path(*coords.group_id.split("."))
    version_dir = _m2_repo(env) / group_path / coords.artifact_id / coords.version
    if version_dir.exists():
        shutil.rmtree(version_dir)


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
    jar_path = target_jar_path()
    if not jar_path.exists():
        raise SystemExit(f"[err] 未找到 jar，请先构建：{jar_path}")
    jar_name = jar_path.name
    docker_bin = _docker_bin()
    run([docker_bin, "exec", "flink-jobmanager", "mkdir", "-p", "/opt/flink/usrlib"], env=env, cwd=ROOT)
    run(
        [
            docker_bin,
            "cp",
            str(jar_path),
            f"flink-jobmanager:/opt/flink/usrlib/{jar_name}",
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
            f"/opt/flink/usrlib/{jar_name}",
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
