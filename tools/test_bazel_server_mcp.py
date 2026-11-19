import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

import bazel_server_mcp as mcp


class BazelServerMcpTests(unittest.TestCase):
    def test_load_projects_from_config_and_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            proj_a = root / "a"
            proj_b = root / "b"
            proj_a.mkdir()
            proj_b.mkdir()
            cfg = root / "projects.json"
            cfg.write_text(json.dumps({"projects": {"alpha": str(proj_a)}}), encoding="utf-8")

            projects = mcp.load_projects(root, cfg, [f"beta={proj_b}"])
            self.assertEqual(projects["engine"], root)
            self.assertEqual(projects["alpha"], proj_a.resolve())
            self.assertEqual(projects["beta"], proj_b.resolve())

    def test_mcp_protocol_start_status_and_keepalive_rewarm(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            out_base = root / "outbase"
            (out_base / "server").mkdir(parents=True)
            pid = os.getpid()
            (out_base / "server/server.pid.txt").write_text(f"{pid}\n", encoding="utf-8")

            manager = mcp.BazelServerManager({"demo": root})
            init_resp = mcp.handle_request(
                manager,
                {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2024-11-05"}},
            )
            self.assertEqual(init_resp["result"]["serverInfo"]["name"], "bazel-server-mcp")

            list_resp = mcp.handle_request(manager, {"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
            names = [t["name"] for t in list_resp["result"]["tools"]]
            self.assertEqual(names, ["bazel_server_start", "bazel_server_status"])

            def fake_run(cmd, **kwargs):
                res = MagicMock()
                res.returncode = 0
                res.stderr = ""
                res.stdout = f"server_pid: {pid}\noutput_base: {out_base}\n"
                return res

            with (
                patch.object(mcp, "find_bazel", return_value="bazel"),
                patch.object(mcp.subprocess, "run", side_effect=fake_run) as run_mock,
            ):
                start_resp = mcp.handle_request(
                    manager,
                    {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "bazel_server_start"}},
                )
                self.assertFalse(start_resp["result"]["isError"])
                self.assertEqual(start_resp["result"]["structuredContent"]["server_pid"], pid)
                self.assertTrue(start_resp["result"]["structuredContent"]["running"])
                self.assertEqual(run_mock.call_args.args[0], ["bazel", "info", "server_pid", "output_base"])

                # Alive JVM -> ensure_warm is a pure read-only no-op (no subprocess call).
                self.assertIsNone(manager.ensure_warm("demo"))
                self.assertEqual(run_mock.call_count, 1)

                # Simulated JVM exit (missing PID file) -> ensure_warm restarts it.
                (out_base / "server/server.pid.txt").unlink()
                rewarmed = manager.ensure_warm("demo")
                self.assertIsNotNone(rewarmed)
                self.assertEqual(run_mock.call_count, 2)

                (out_base / "server/server.pid.txt").write_text(f"{pid}\n", encoding="utf-8")
                status_resp = mcp.handle_request(
                    manager,
                    {"jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": {"name": "bazel_server_status"}},
                )
                self.assertEqual(status_resp["result"]["structuredContent"]["projects"][0]["server_pid"], pid)
                self.assertTrue(status_resp["result"]["structuredContent"]["projects"][0]["running"])


if __name__ == "__main__":
    unittest.main()
