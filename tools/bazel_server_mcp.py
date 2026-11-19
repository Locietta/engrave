#!/usr/bin/env python3
"""Minimal stdio MCP server that keeps Bazel server JVMs warm across agent commands.

On Windows, agent shell runners typically execute each command inside a Job Object
with KILL_ON_JOB_CLOSE, which terminates any Bazel JVM spawned inside that command.
Running this lightweight MCP server inside the IDE/agent host keeps the Bazel JVM
alive outside per-command Job Objects so subsequent `bazel` / `bbuild.py` / `gate.py`
invocations reuse the warm server automatically.

Zero third-party dependencies (Python 3.10+ stdlib only).
"""

from __future__ import annotations

import argparse
import ctypes
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

PROTOCOL_VERSION = "2024-11-05"
SERVER_INFO = {"name": "bazel-server-mcp", "version": "0.1.0"}


def default_projects(repo_root: Path) -> dict[str, Path]:
    projects: dict[str, Path] = {}
    if (repo_root / "vendor/engrave/MODULE.bazel").is_file():
        projects["host"] = repo_root
        projects["engine"] = (repo_root / "vendor/engrave").resolve()
    else:
        projects["engine"] = repo_root
    return projects


def load_projects(
    repo_root: Path,
    config_path: Path | None,
    cli_projects: list[str],
) -> dict[str, Path]:
    projects = default_projects(repo_root)
    if config_path is not None:
        raw = json.loads(config_path.read_text(encoding="utf-8"))
        mapping = raw.get("projects", raw) if isinstance(raw, dict) else {}
        for name, value in mapping.items():
            projects[str(name)] = Path(value).expanduser().resolve()
    for spec in cli_projects:
        if "=" not in spec:
            raise ValueError(f"Expected --project NAME=PATH, got: {spec!r}")
        name, raw_path = spec.split("=", 1)
        name = name.strip()
        if not name:
            raise ValueError(f"Empty project name in --project {spec!r}")
        projects[name] = Path(raw_path).expanduser().resolve()
    return projects


def find_bazel(workspace: Path) -> str:
    pixi_prefix = workspace / ".pixi/envs/default"
    candidates = [
        pixi_prefix / "Library/bin/bazelisk.exe",
        pixi_prefix / "bin/bazelisk",
        pixi_prefix / "Library/bin/bazel.exe",
        pixi_prefix / "bin/bazel",
    ]
    for cand in candidates:
        if cand.is_file():
            return str(cand)
    for exe in ("bazel", "bazelisk"):
        found = shutil.which(exe)
        if found:
            return found
    raise RuntimeError(f"bazelisk/bazel not found for workspace {workspace}")


def pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        process_query_limited_information = 0x1000
        still_active = 259
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(process_query_limited_information, False, pid)
        if not handle:
            return False
        try:
            exit_code = ctypes.c_ulong(0)
            if not kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
                return False
            return exit_code.value == still_active
        finally:
            kernel32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def read_pid_from_output_base(output_base: Path) -> int | None:
    pid_file = output_base / "server/server.pid.txt"
    if not pid_file.is_file():
        return None
    try:
        pid = int(pid_file.read_text(encoding="utf-8").strip())
    except (OSError, ValueError):
        return None
    return pid if pid_alive(pid) else None


class BazelServerManager:
    def __init__(self, projects: dict[str, Path], default_project: str | None = None) -> None:
        if not projects:
            raise ValueError("At least one Bazel project must be configured")
        self.projects = dict(projects)
        self.default_project = default_project or next(iter(self.projects))
        if self.default_project not in self.projects:
            raise ValueError(f"Unknown default project {self.default_project!r}")
        self._output_bases: dict[str, Path] = {}

    def _resolve_project(self, name: str | None) -> tuple[str, Path]:
        selected = name or self.default_project
        if selected not in self.projects:
            known = ", ".join(sorted(self.projects))
            raise ValueError(f"Unknown project {selected!r}; configured projects: {known}")
        workspace = self.projects[selected]
        if not workspace.is_dir():
            raise RuntimeError(f"Workspace directory does not exist: {workspace}")
        return selected, workspace

    def start(self, project: str | None = None) -> dict[str, Any]:
        name, workspace = self._resolve_project(project)
        bazel = find_bazel(workspace)
        # Use Bazel's default startup options so subsequent `bbuild.py` / `gate.py` / `bazel`
        # invocations match startup flags identically and reuse this JVM without restarting it.
        proc = subprocess.run(
            [bazel, "info", "server_pid", "output_base"],
            cwd=workspace,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            raise RuntimeError(proc.stderr.strip() or f"bazel info failed with exit {proc.returncode}")
        values: dict[str, str] = {}
        for line in proc.stdout.splitlines():
            if ": " in line:
                key, val = line.split(": ", 1)
                values[key.strip()] = val.strip()
        pid = int(values["server_pid"])
        output_base = Path(values["output_base"])
        self._output_bases[name] = output_base
        return {
            "project": name,
            "workspace": str(workspace),
            "server_pid": pid,
            "output_base": str(output_base),
            "running": pid_alive(pid),
        }

    def stop(self, project: str | None = None) -> dict[str, Any]:
        if project == "all":
            results = [self.stop(name) for name in self.projects]
            return {"stopped": results}
        name, workspace = self._resolve_project(project)
        bazel = find_bazel(workspace)
        proc = subprocess.run(
            [bazel, "shutdown"],
            cwd=workspace,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            raise RuntimeError(proc.stderr.strip() or f"bazel shutdown failed with exit {proc.returncode}")
        return {
            "project": name,
            "workspace": str(workspace),
            "running": False,
        }

    def status(self) -> dict[str, Any]:
        items: list[dict[str, Any]] = []
        for name, workspace in self.projects.items():
            output_base = self._output_bases.get(name)
            pid = read_pid_from_output_base(output_base) if output_base is not None else None
            entry: dict[str, Any] = {
                "project": name,
                "workspace": str(workspace),
                "default": name == self.default_project,
                "known_output_base": str(output_base) if output_base is not None else None,
                "server_pid": pid,
                "running": pid is not None if output_base is not None else None,
            }
            items.append(entry)
        return {"projects": items}


def tool_definitions(manager: BazelServerManager) -> list[dict[str, Any]]:
    names = list(manager.projects)
    return [
        {
            "name": "bazel_server_start",
            "description": (
                "Start (or confirm) a persistent Bazel JVM server for a configured project so "
                "subsequent shell build commands reuse it without JVM startup overhead."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "enum": names,
                        "description": f"Project key (default: {manager.default_project}).",
                    }
                },
                "additionalProperties": False,
            },
        },
        {
            "name": "bazel_server_stop",
            "description": "Shut down the persistent Bazel JVM server for a configured project (or 'all').",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "enum": [*names, "all"],
                        "description": f"Project key or 'all' (default: {manager.default_project}).",
                    }
                },
                "additionalProperties": False,
            },
        },
        {
            "name": "bazel_server_status",
            "description": "Report configured Bazel projects and whether known Bazel server JVMs are alive (read-only).",
            "inputSchema": {
                "type": "object",
                "properties": {},
                "additionalProperties": False,
            },
        },
    ]


def handle_request(manager: BazelServerManager, request: dict[str, Any]) -> dict[str, Any] | None:
    req_id = request.get("id")
    method = request.get("method")
    params = request.get("params") or {}

    if method == "notifications/initialized":
        return None
    if method == "initialize":
        client_version = params.get("protocolVersion") or PROTOCOL_VERSION
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": client_version,
                "capabilities": {"tools": {}},
                "serverInfo": SERVER_INFO,
            },
        }
    if method == "ping":
        return {"jsonrpc": "2.0", "id": req_id, "result": {}}
    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": req_id, "result": {"tools": tool_definitions(manager)}}
    if method == "tools/call":
        tool_name = params.get("name")
        arguments = params.get("arguments") or {}
        try:
            if tool_name == "bazel_server_start":
                payload = manager.start(arguments.get("project"))
            elif tool_name == "bazel_server_stop":
                payload = manager.stop(arguments.get("project"))
            elif tool_name == "bazel_server_status":
                payload = manager.status()
            else:
                return {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "error": {"code": -32602, "message": f"Unknown tool: {tool_name!r}"},
                }
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": json.dumps(payload, indent=2)}],
                    "structuredContent": payload,
                    "isError": False,
                },
            }
        except Exception as exc:
            err = {"error": str(exc)}
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": json.dumps(err)}],
                    "structuredContent": err,
                    "isError": True,
                },
            }
    if req_id is None:
        return None
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": -32601, "message": f"Method not found: {method!r}"},
    }


def serve_stdio(manager: BazelServerManager) -> int:
    stdin = sys.stdin.buffer
    stdout = sys.stdout.buffer
    for raw_line in stdin:
        line = raw_line.strip()
        if not line:
            continue
        try:
            request = json.loads(line.decode("utf-8"))
        except Exception as exc:
            response: dict[str, Any] | None = {
                "jsonrpc": "2.0",
                "id": None,
                "error": {"code": -32700, "message": f"Parse error: {exc}"},
            }
        else:
            response = handle_request(manager, request)
        if response is not None:
            stdout.write((json.dumps(response, separators=(",", ":")) + "\n").encode("utf-8"))
            stdout.flush()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project",
        action="append",
        default=[],
        metavar="NAME=PATH",
        help="Add or override a Bazel project mapping (repeatable).",
    )
    parser.add_argument(
        "--config",
        type=Path,
        help="Optional JSON file mapping project names to workspace paths.",
    )
    parser.add_argument(
        "--default-project",
        help="Default project key when a tool call omits 'project'.",
    )
    parser.add_argument(
        "--warm",
        action="append",
        default=[],
        metavar="NAME",
        help="Warm the named project's Bazel JVM immediately on server startup (repeatable).",
    )
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parents[1]
    projects = load_projects(repo_root, args.config, args.project)
    manager = BazelServerManager(projects, default_project=args.default_project)
    for name in args.warm:
        try:
            info = manager.start(name)
            sys.stderr.write(f"bazel-server-mcp: warmed {name} (pid={info['server_pid']})\n")
            sys.stderr.flush()
        except Exception as exc:
            sys.stderr.write(f"bazel-server-mcp: failed to warm {name}: {exc}\n")
            sys.stderr.flush()
    return serve_stdio(manager)


if __name__ == "__main__":
    raise SystemExit(main())
