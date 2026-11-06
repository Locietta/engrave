#!/usr/bin/env python3
"""Run linters (ruff for Python, clang-tidy for C++) on project files."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from style_files import ENGINE_ROOT, engine_files

CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
PYTHON_EXTENSIONS = {".py", ".pyi"}

IGNORED_PREFIXES = (
    "vendor/",
    ".upstream/",
    "tests/data/",
    ".pixi/",
    ".xmake/",
    "build/",
    ".runtime/",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run linters on tracked project files.")
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        default=[Path(".")],
        help="files or directories to lint (defaults to current directory)",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="automatically apply lint fixes where available",
    )
    parser.add_argument("--engine-root", type=Path, default=ENGINE_ROOT, help="engine checkout (with --engine)")
    parser.add_argument("--engine", action="store_true", help="check compiled owned engine translation units")
    parser.add_argument("-j", "--jobs", type=int, default=min(12, os.cpu_count() or 1))
    return parser.parse_args()


def should_ignore(path: Path) -> bool:
    posix_path = path.as_posix()
    return any(posix_path.startswith(prefix) for prefix in IGNORED_PREFIXES)


def project_files(paths: list[Path]) -> list[Path]:
    for path in paths:
        if not path.exists():
            raise FileNotFoundError(path)
    command = [
        "git",
        "ls-files",
        "-z",
        "--cached",
        "--others",
        "--exclude-standard",
        "--",
        *map(str, paths),
    ]

    output = subprocess.check_output(command)
    files = [Path(path.decode()) for path in output.split(b"\0") if path]
    return [path for path in files if path.is_file() and not should_ignore(path)]


def ensure_compile_commands(repo_root: Path) -> None:
    compdb = repo_root / "compile_commands.json"
    if not compdb.exists():
        print("compile_commands.json not found; generating via xmake...", flush=True)
        subprocess.check_call(["xmake", "project", "-k", "compile_commands"], cwd=repo_root)


def lint_engine(args, repo_root):
    if args.fix:
        raise ValueError("--fix is unsupported with --engine; shared headers require coordinated fixes")
    engine_root = args.engine_root.resolve()
    files = {path.resolve() for path in engine_files(args.paths, engine_root)}
    ensure_compile_commands(repo_root)
    database = json.loads((repo_root / "compile_commands.json").read_text())
    units = set()
    for entry in database:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        if path.resolve() in files:
            units.add(path.resolve())
    if not units:
        print("lint: no compiled engine translation units selected", file=sys.stderr)
        return 1
    root_pattern = re.escape(engine_root.as_posix()).replace("/", r"[\\/]")
    command = [
        "clang-tidy",
        "-p",
        str(repo_root),
        "--config-file=" + str(engine_root / ".clang-tidy"),
        "--header-filter=^" + root_pattern + r"[\\/](engrave|src|muse)[\\/]",
        "--warnings-as-errors=readability-identifier-naming",
    ]
    if sys.platform == "win32":
        # MSVC vector algorithms use intrinsics unsupported by clang-tidy.
        command += ["--extra-arg=-D_USE_STD_VECTOR_ALGORITHMS=0"]
    if args.fix:
        command += ["--fix", "--format-style=file"]

    def check(path):
        result = subprocess.run([*command, str(path)], cwd=repo_root, capture_output=True, text=True)
        print(f"{path}: exit {result.returncode}", flush=True)
        if result.stdout:
            print(result.stdout, flush=True)
        if result.stderr:
            print(result.stderr, file=sys.stderr, flush=True)
        return result.returncode

    print(f"Checking {len(units)} engine translation units", flush=True)
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        return int(any(list(pool.map(check, sorted(units)))))


def main() -> int:
    args = parse_args()
    args.engine = True
    repo_root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())

    try:
        if args.engine:
            return lint_engine(args, repo_root)
        files = project_files(args.paths)
    except (ValueError, FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"lint: {error}", file=sys.stderr)
        return 1

    py_files = [f for f in files if f.suffix.lower() in PYTHON_EXTENSIONS]
    cpp_files = [f for f in files if f.suffix.lower() in CPP_EXTENSIONS]

    total_returncode = 0

    # 1. Python linting with ruff
    if py_files:
        print(f"Running ruff check on {len(py_files)} Python file(s)...", flush=True)
        cmd = ["ruff", "check"]
        if args.fix:
            cmd.append("--fix")
        cmd.extend(map(str, py_files))
        res = subprocess.run(cmd, cwd=repo_root, check=False)
        if res.returncode != 0:
            total_returncode = res.returncode

    # 2. C++ linting with clang-tidy
    if cpp_files:
        ensure_compile_commands(repo_root)
        print(f"Running clang-tidy on {len(cpp_files)} C++ file(s)...", flush=True)
        cmd = ["clang-tidy", "-p", str(repo_root)]
        if args.fix:
            cmd.extend(["--fix-errors", "--format-style=file"])
        cmd.extend(map(str, cpp_files))
        res = subprocess.run(cmd, cwd=repo_root, check=False)
        if res.returncode != 0:
            total_returncode = res.returncode

    return total_returncode


if __name__ == "__main__":
    sys.exit(main())
