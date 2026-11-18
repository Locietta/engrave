#!/usr/bin/env python3
"""Check the compiled owned engine translation units with clang-tidy.

The engine is a submodule of the host checkout, which owns the compilation
database and the build this tool reads it from. Every translation unit is
checked by a process of its own, on as many threads as there are jobs, because
clang-tidy parses one file at a time.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from style_files import ENGINE_ROOT, engine_files

# Flags that only name the object file's place. Two database entries for one
# file that differ in these compile the same source the same way.
SEPARATE_OUTPUT_FLAGS = {"-o", "-MF", "-MT", "-MQ", "-Fd", "/Fo", "/Fd", "--output"}
ATTACHED_OUTPUT_PREFIXES = ("/Fo", "/Fd", "-MF", "-MT", "-MQ", "-Fd")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check compiled owned engine translation units with clang-tidy.")
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        default=[Path(".")],
        help="files or directories to check (defaults to the whole engine)",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="automatically apply lint fixes where available",
    )
    parser.add_argument(
        "--backend",
        choices=("auto", "clice", "clang-tidy"),
        default="auto",
        help="accepted for compatibility; engine checking always uses clang-tidy",
    )
    parser.add_argument("--engine-root", type=Path, default=ENGINE_ROOT, help="engine checkout")
    parser.add_argument("--engine", action="store_true", help="accepted for compatibility; always the engine")
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=max(1, os.cpu_count() or 1),
        help="clang-tidy processes to run at once (default: CPU count)",
    )
    return parser.parse_args()


def find_compile_commands(repo_root: Path) -> Path:
    """Return the freshest compilation database, generating one only if none exists."""
    candidates = [
        repo_root / "build/bazel-release/compile_commands.json",
        repo_root / "build/bazel-debug/compile_commands.json",
        repo_root / "build/bazel-releasedbg/compile_commands.json",
        repo_root / "build/bazel-asan/compile_commands.json",
        repo_root / "build/release/compile_commands.json",
        repo_root / "build/debug/compile_commands.json",
        repo_root / "build/releasedbg/compile_commands.json",
        repo_root / "build/asan/compile_commands.json",
        repo_root / "compile_commands.json",
    ]
    existing = [path for path in candidates if path.is_file()]
    if existing:
        return max(existing, key=lambda path: path.stat().st_mtime_ns)
    print("compile_commands.json not found; generating via xmake...", flush=True)
    fallback = repo_root / "compile_commands.json"
    try:
        subprocess.check_call(["xmake", "project", "-k", "compile_commands"], cwd=repo_root)
    except (OSError, subprocess.CalledProcessError) as error:
        raise ValueError(f"cannot generate compile_commands.json: {error}") from error
    return fallback


def compile_database(repo_root: Path) -> tuple[Path, list[dict]]:
    database_path = find_compile_commands(repo_root)
    return database_path, json.loads(database_path.read_text(encoding="utf-8"))


def entry_path(entry: dict) -> Path:
    path = Path(entry["file"])
    if not path.is_absolute():
        path = Path(entry["directory"]) / path
    return path.resolve()


def entry_directory(entry: dict) -> str:
    directory = entry.get("directory")
    if not directory:
        return ""
    return str(Path(directory).resolve()).replace("\\", "/")


def normalized_arguments(entry: dict) -> tuple[str, ...]:
    """The compile command with the flags that only name output files removed."""
    arguments = entry.get("arguments")
    if arguments is None:
        arguments = shlex.split(entry.get("command", ""), posix=False)
    source = str(entry["file"]).replace("\\", "/")
    normalized = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
            continue
        cooked = argument.replace("\\", "/")
        if cooked in SEPARATE_OUTPUT_FLAGS:
            skip = True
            continue
        if cooked.startswith(ATTACHED_OUTPUT_PREFIXES):
            continue
        if cooked == source:
            continue
        normalized.append(cooked)
    return tuple(normalized)


def worker_count(jobs: int) -> int:
    """Processes to run at once; a non-positive count means the CPU count."""
    return jobs if jobs > 0 else max(1, os.cpu_count() or 1)


def lint_engine(args: argparse.Namespace, repo_root: Path) -> int:
    if args.fix:
        raise ValueError("--fix is unsupported with --engine; shared headers require coordinated fixes")
    if args.backend == "clice":
        print(
            "lint: clice lint checks a whole compilation database and cannot be restricted to the "
            "engine's translation units; using clang-tidy",
            file=sys.stderr,
        )
    engine_root = args.engine_root.resolve()
    files = {path.resolve() for path in engine_files(args.paths, engine_root)}
    database_path, database = compile_database(repo_root)
    entries = []
    seen_commands: dict[Path, set[tuple[str, tuple[str, ...]]]] = {}
    duplicates = 0
    variants = 0
    for entry in database:
        path = entry_path(entry)
        if path not in files:
            continue
        command = (entry_directory(entry), normalized_arguments(entry))
        commands = seen_commands.setdefault(path, set())
        if command in commands:
            duplicates += 1
            continue
        if commands:
            variants += 1
        commands.add(command)
        entries.append(entry)
    units = sorted(seen_commands)
    if not units:
        print("lint: no compiled engine translation units selected", file=sys.stderr)
        return 1

    # clang-tidy parses a file once per database entry naming it, so the
    # selected entries are handed over as a database of their own.
    package = database_path.parent
    temporary = None
    if len(entries) != len(database):
        try:
            temporary = tempfile.TemporaryDirectory(prefix="engrave-lint-")
            (Path(temporary.name) / "compile_commands.json").write_text(json.dumps(entries), encoding="utf-8")
            package = Path(temporary.name)
        except OSError as error:
            print(f"lint: cannot stage the collapsed database ({error}); linting {database_path}", file=sys.stderr)
            if temporary is not None:
                temporary.cleanup()
                temporary = None

    root_pattern = re.escape(engine_root.as_posix()).replace("/", r"[\\/]")
    command = [
        "clang-tidy",
        "--quiet",
        "-p",
        str(package),
        "--config-file=" + str(engine_root / ".clang-tidy"),
        "--header-filter=^" + root_pattern + r"[\\/](engrave|src|muse)[\\/]",
        "--warnings-as-errors=readability-identifier-naming",
    ]
    if sys.platform == "win32":
        # MSVC vector algorithms use intrinsics unsupported by clang-tidy;
        # clang-tidy's MSVC-compat frontend also trips ngcpp-proxy's clang-cl
        # [[msvc::no_unique_address]] layout guard (llvm/llvm-project#143245),
        # whereas cl.exe compiles the actual binary.
        command += [
            "--extra-arg=-D_USE_STD_VECTOR_ALGORITHMS=0",
            "--extra-arg=-DMSFT_PROXY_V4_DETAIL_COMPATIBILITY_CHECK_H_",
        ]

    def check(path: Path):
        result = subprocess.run([*command, str(path)], cwd=repo_root, capture_output=True, text=True, errors="replace")
        return path, result.returncode, result.stdout, result.stderr

    print(
        f"Checking {len(units)} engine file(s) from {len(entries)} translation unit entry/entries "
        f"({duplicates} duplicate collapsed, {variants} distinct variant(s) retained) "
        f"with {worker_count(args.jobs)} job(s)",
        flush=True,
    )
    try:
        with ThreadPoolExecutor(max_workers=worker_count(args.jobs)) as pool:
            outcomes = list(pool.map(check, units))
    finally:
        if temporary is not None:
            temporary.cleanup()

    failed = 0
    for path, code, stdout, stderr in outcomes:
        if code != 0 or stdout.strip():
            print(f"{path}: exit {code}", flush=True)
            if stdout:
                print(stdout, end="", flush=True)
            if stderr:
                print(stderr, end="", file=sys.stderr, flush=True)
        if code != 0:
            failed += 1
    print(f"Checked {len(outcomes)} engine file(s), {failed} failed.", flush=True)
    return int(failed > 0)


def main() -> int:
    args = parse_args()
    args.engine = True
    repo_root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())

    try:
        return lint_engine(args, repo_root)
    except (ValueError, FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"lint: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
