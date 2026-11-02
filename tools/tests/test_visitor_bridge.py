from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "devops"))
from devtool.commands.ops import docker, docker_image, java


class VisitorBridgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="common-visitor-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.visitor = self.root / "visitor"
        (self.visitor / "tools/development").mkdir(parents=True)
        (self.visitor / "tools/development/cli.py").touch()

    def test_default_sibling_checkout(self):
        with patch.object(docker, "ROOT", self.root / "common"):
            self.assertEqual(docker.visitor_root({}), self.visitor)

    def test_explicit_checkout(self):
        self.assertEqual(docker.visitor_root({"VISITOR_ROOT": str(self.visitor)}), self.visitor)

    def test_missing_checkout_is_actionable(self):
        with self.assertRaisesRegex(SystemExit, "VISITOR_ROOT"):
            docker.visitor_root({"VISITOR_ROOT": str(self.root / "missing")})

    def test_bridge_forwards_current_source_and_proxy(self):
        env = {"VISITOR_ROOT": str(self.visitor), "COMMON_ROOT": "/stale", "HTTPS_PROXY": "http://localhost:7890"}
        with patch.object(docker, "run") as run:
            docker.run_visitor("build-docker", env)
        args = run.call_args.args[0]
        self.assertEqual(args[-1], "build-docker")
        self.assertEqual(args[args.index("--common-root") + 1], str(docker.ROOT))
        self.assertEqual(run.call_args.kwargs["cwd"], self.visitor)
        forwarded = run.call_args.kwargs["env"]
        self.assertEqual(forwarded["COMMON_ROOT"], str(docker.ROOT))
        self.assertEqual(forwarded["HTTPS_PROXY"], env["HTTPS_PROXY"])
        self.assertEqual(env["COMMON_ROOT"], "/stale")

    def test_existing_service_commands_delegate(self):
        for name in ["setup", "clean", "run"]:
            with self.subTest(name=name), patch.object(docker, "run_visitor") as forward:
                getattr(docker, f"{name}_docker")({})
                forward.assert_called_once_with(f"{name}-docker", {})

    def test_existing_image_commands_delegate(self):
        for name in ["build", "push"]:
            with self.subTest(name=name), patch.object(docker_image, "base_env", return_value={}), patch.object(docker_image, "run_visitor") as forward:
                getattr(docker_image, f"run_{name}")(None)
                forward.assert_called_once_with(f"{name}-docker", {})

    def test_java_compose_uses_visitor_and_current_common(self):
        p = self.visitor / "images/development/services/flink/docker-compose.yaml"
        p.parent.mkdir(parents=True)
        p.touch()
        for action in ["up", "down"]:
            with self.subTest(action=action), patch.object(java, "_compose_cmd", return_value=["docker", "compose"]), patch.object(java, "run") as run:
                getattr(java, f"docker_{action}")({"VISITOR_ROOT": str(self.visitor)})
            self.assertIn(str(p), run.call_args.args[0])
            self.assertEqual(run.call_args.kwargs["env"]["COMMON_ROOT"], str(java.ROOT))

    def test_java_requires_compose_only_for_docker_operations(self):
        # Reading Maven coordinates does not require a visitor checkout.
        self.assertEqual(java.read_coords().artifact_id, "flink-datastream-demo")
        with self.assertRaisesRegex(SystemExit, "VISITOR_ROOT"):
            java._compose_file({"VISITOR_ROOT": str(self.root / "missing")})


if __name__ == "__main__":
    unittest.main()
