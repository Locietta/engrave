#!/usr/bin/env python3
"""Format project files with the formatter selected from their extension.

C++ goes through `clice format` when a clice build is available (one process
that formats on several threads with the clang-format built into clice), and
through `clang-format` itself otherwise, run as parallel batches of files.
Python, JSON, Markdown, YAML and TOML keep their own formatters. The groups
touch disjoint files, so they run at the same time.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from style_files import ENGINE_ROOT, engine_files

FORMATTERS = {
    "clang-format": {".c", ".cc", ".cpp", ".cppm", ".cxx", ".h", ".hh", ".hpp", ".hxx"},
    "ruff": {".py", ".pyi"},
    "prettier": {".json", ".md", ".yaml", ".yml"},
    "taplo": {".toml"},
}

COMMANDS = {
    "clang-format": ["clang-format"],
    "ruff": ["ruff", "format"],
    "prettier": ["prettier"],
    "taplo": ["taplo", "format"],
}

MODE_ARGUMENTS = {
    "clang-format": {False: ["-i"], True: ["--dry-run", "--Werror"]},
    "ruff": {False: [], True: ["--check"]},
    "prettier": {False: ["--write"], True: ["--check"]},
    "taplo": {False: [], True: ["--check"]},
}

LANGUAGES = {
    "cpp": "clang-format",
    "json": "prettier",
    "markdown": "prettier",
    "python": "ruff",
    "toml": "taplo",
}

IGNORED_PREFIXES = (
    "vendor/",
    ".upstream/",
    "tests/data/",
    ".pixi/",
    ".xmake/",
    "build/",
    ".runtime/",
)

IGNORED_FILENAMES = {
    "compile_commands.json",
    "pixi.lock",
    "tools.json",
}

BACKENDS = ("auto", "clice", "clang-format")

# Files per clang-format process. Starting a process costs about as much as
# formatting a dozen small files, and one batch per worker keeps every core
# busy once a tree is large enough to fill them.
BATCH_FILES = 16

# One command line has to fit the platform's limit; Windows stops at 32767
# characters, so leave room for the executable's own arguments.
COMMAND_LIMIT = 24_000 if sys.platform == "win32" else 100_000

# Below this many files one clice process is faster than one per worker.
CLICE_BATCH_FILES = 64


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Format tracked project files, selecting a formatter by extension.")
    parser.add_argument(
        "paths",
        nargs="+",
        type=Path,
        help="files or directories to format",
    )
    parser.add_argument(
        "-l",
        "--language",
        choices=LANGUAGES,
        help="override language detection for every input",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="report files that require formatting without modifying them",
    )
    parser.add_argument(
        "--backend",
        choices=BACKENDS,
        default="auto",
        help=(
            "C++ formatter: 'clice' when the clice binary and this checkout's clice.toml are usable, "
            "'clang-format' always (default: auto)"
        ),
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=max(1, os.cpu_count() or 1),
        help="formatter processes to run at once (default: CPU count)",
    )
    parser.add_argument("--engine-root", type=Path, default=ENGINE_ROOT, help="engine checkout (with --engine)")
    parser.add_argument("--engine", action="store_true", help="format tracked owned engine C++ files")
    return parser.parse_args()


def should_ignore(path: Path) -> bool:
    posix_path = path.as_posix()
    if any(posix_path.startswith(prefix) for prefix in IGNORED_PREFIXES):
        return True
    if path.name in IGNORED_FILENAMES:
        return True
    return False


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


def select_formatters(paths: list[Path], language: str | None) -> dict[str, list[Path]]:
    if language:
        return {LANGUAGES[language]: paths}

    selected: dict[str, list[Path]] = defaultdict(list)
    for path in paths:
        suffix = path.suffix.lower()
        if not suffix and path.is_file():
            with path.open("rb") as file:
                if b"python" in file.readline(128).lower():
                    selected["ruff"].append(path)
                    continue
        for formatter, extensions in FORMATTERS.items():
            if suffix in extensions:
                selected[formatter].append(path)
                break
    return selected


def language_arguments(language: str | None, files: list[Path]) -> list[str]:
    if language in {"json", "markdown"}:
        return ["--parser", language]
    if language == "python":
        extensions = sorted(
            {path.suffix[1:].lower() for path in files if path.suffix and path.suffix.lower() not in {".py", ".pyi"}}
        )
        return [argument for extension in extensions for argument in ("--extension", f"{extension}:python")]
    return []


def is_within(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
    except ValueError:
        return False
    return True


def clice_project_root(files: list[Path]) -> Path | None:
    """The nearest directory at or above the files that holds clice's config."""
    for directory in files[0].resolve().parents:
        if (directory / "clice.toml").is_file() or (directory / ".clice" / "config.toml").is_file():
            return directory
    return None


def extension_roots() -> list[Path]:
    """Directories holding editor extensions, in the order to search them."""
    roots = []
    configured = os.environ.get("VSCODE_EXTENSIONS")
    if configured:
        roots.append(Path(configured))
    home = Path.home()
    for edition in (
        ".vscode",
        ".vscode-insiders",
        ".vscode-server",
        ".antigravity",
        ".antigravity-ide",
        ".cursor",
        ".windsurf",
    ):
        roots.append(home / edition / "extensions")
    # Portable and Scoop-style installs keep the extension tree inside the
    # install's data directory; the editor's own executable says where that
    # install is, which also covers an install on another drive.
    for editor in ("code", "code-insiders", "codium", "antigravity", "antigravity-ide", "windsurf"):
        executable = shutil.which(editor)
        if not executable:
            continue
        for ancestor in Path(executable).resolve().parents:
            roots.append(ancestor / "data" / "extensions")
            roots += sorted((ancestor / "persist").glob("*/data/extensions"))
    for variable in ("SCOOP", "SCOOP_GLOBAL"):
        scoop = os.environ.get(variable)
        if scoop:
            for edition in ("vscode", "vscode-insiders", "antigravity", "cursor"):
                roots.append(Path(scoop) / "persist" / edition / "data" / "extensions")
    for variable in ("LOCALAPPDATA", "PROGRAMFILES", "PROGRAMFILES(X86)"):
        base = os.environ.get(variable)
        if base:
            roots += sorted((Path(base) / "Programs").glob("*/data/extensions"))
    return list(dict.fromkeys(roots))


def clice_extension_binaries() -> list[Path]:
    """The clice servers shipped inside editor extensions, newest first."""
    if sys.platform != "win32":
        return []
    found: list[Path] = []
    for root in extension_roots():
        if not root.is_dir():
            continue
        for pattern in ("clice-io.clice-*/clice/bin/clice.exe", "clice-io.clice-*/clice/bin/clice"):
            found += [path for path in root.glob(pattern) if path.is_file()]
    return sorted(found, key=lambda path: path.parent.parent.parent.name, reverse=True)


def find_clice() -> Path | None:
    configured = os.environ.get("CLICE_BIN")
    if configured:
        binary = Path(configured).expanduser()
        if binary.is_file():
            return binary
        print(f"format: CLICE_BIN={configured} is not a file", file=sys.stderr)
    on_path = shutil.which("clice")
    if on_path:
        return Path(on_path)
    for binary in clice_extension_binaries():
        return binary
    return None


def declared_databases(workspace: Path) -> list[Path] | None:
    """The compilation databases the active clice configuration declares.

    `None` means the configuration cannot be read here, leaving clice itself
    to decide whether it can run.
    """
    config = workspace / "clice.toml"
    if not config.is_file():
        config = workspace / ".clice" / "config.toml"
    try:
        import tomllib
    except ImportError:  # pragma: no cover - Python earlier than 3.11
        return None
    try:
        settings = tomllib.loads(config.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None

    default = settings.get("default_configuration") or ""
    rules = [rule for rule in settings.get("rules", []) if isinstance(rule, dict)]
    tags = [rule["configuration"] for rule in rules if rule.get("configuration")]
    if not default and tags:
        # clice falls back to the first declared tag.
        default = tags[0]
    entries: list[str] = []
    for rule in rules:
        tag = rule.get("configuration") or ""
        if tag and tag != default:
            continue
        entries += [entry for entry in rule.get("compile_commands", []) if isinstance(entry, str)]
    if not entries:
        entries = ["compile_commands.json"]

    resolved = []
    for entry in entries:
        path = Path(entry.replace("${workspace}", str(workspace)))
        if not path.is_absolute():
            path = workspace / path
        resolved.append(path if path.suffix.lower() == ".json" else path / "compile_commands.json")
    return resolved


def clice_database(workspace: Path) -> Path | None:
    """A compilation database clice loads for its active configuration."""
    databases = declared_databases(workspace)
    if databases is None:
        return None
    for database in databases:
        if database.is_file():
            return database
    for database in (workspace / "compile_commands.json", *workspace.glob("*/compile_commands.json")):
        if database.is_file():
            return database
    return None


def resolve_backend(requested: str, files: list[Path]) -> tuple[str, Path | None, Path | None]:
    """Pick the C++ formatter: the backend, its binary and clice's project root."""
    if requested == "clang-format":
        return "clang-format", None, None

    binary = find_clice()
    workspace = clice_project_root(files)
    if binary is None:
        if requested == "clice":
            print(
                "format: no clice binary (CLICE_BIN, PATH or an editor extension); using clang-format", file=sys.stderr
            )
        return "clang-format", None, None
    if workspace is None:
        if requested == "clice":
            print("format: no clice.toml above the C++ files; using clang-format", file=sys.stderr)
        return "clang-format", None, None
    if requested == "auto":
        # clice format needs no database for named files, but a checkout
        # without one is not a clice project yet; clang-format needs nothing.
        if clice_database(workspace) is None:
            return "clang-format", None, None
        if not all(is_within(file, workspace) for file in files):
            return "clang-format", None, None
    return "clice", binary, workspace


def batch_files(files: list[Path], batch_size: int, prefix: list[str]) -> list[list[Path]]:
    """Split files into batches that each fit one command line."""
    limit = COMMAND_LIMIT - len(subprocess.list2cmdline(prefix)) - 2
    batches: list[list[Path]] = []
    current: list[Path] = []
    length = 0
    for file in files:
        piece = len(str(file)) + 3
        if current and (len(current) >= batch_size or length + piece > limit):
            batches.append(current)
            current = []
            length = 0
        current.append(file)
        length += piece
    if current:
        batches.append(current)
    return batches


def run_tool(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    if Path(command[0]).suffix.lower() in {".bat", ".cmd"}:
        return subprocess.run(subprocess.list2cmdline(command), shell=True, **kwargs)
    return subprocess.run(command, **kwargs)


def report(outcomes: list[tuple[int, str, str]]) -> int:
    returncode = 0
    for code, stdout, stderr in outcomes:
        if stdout:
            sys.stdout.write(stdout)
        if stderr:
            sys.stderr.write(stderr)
        if returncode == 0:
            returncode = code
    sys.stdout.flush()
    sys.stderr.flush()
    return returncode


def worker_count(jobs: int) -> int:
    """Processes to run at once; a non-positive count means the CPU count."""
    return jobs if jobs > 0 else max(1, os.cpu_count() or 1)


def run_clang_format(executable: str | None, files: list[Path], check: bool, jobs: int) -> int:
    if executable is None:
        print("format: could not find clang-format", file=sys.stderr)
        return 1
    jobs = worker_count(jobs)
    prefix = [executable, *MODE_ARGUMENTS["clang-format"][check]]
    batches = batch_files(files, BATCH_FILES, prefix)

    def format_batch(batch: list[Path]) -> tuple[int, str, str]:
        result = run_tool([*prefix, *map(str, batch)], capture_output=True, text=True, errors="replace")
        return result.returncode, result.stdout, result.stderr

    with ThreadPoolExecutor(max_workers=max(1, min(jobs, len(batches)))) as pool:
        return report(list(pool.map(format_batch, batches)))


def run_clice_format(binary: Path, workspace: Path, files: list[Path], check: bool, jobs: int) -> int:
    jobs = worker_count(jobs)
    prefix = [str(binary), "format", "--workspace", str(workspace), "--log-level", "error"]
    if check:
        prefix.append("--check")
    # Spread the files over at most `jobs` processes; clice formats a batch's
    # files on its own threads, but no one process can be given more files
    # than the command line holds.
    target = max(CLICE_BATCH_FILES, -(-len(files) // max(1, jobs)))
    batches = batch_files(files, target, [*prefix, "--jobs", "1"])
    batch_jobs = max(1, jobs // len(batches))

    def format_batch(batch: list[Path]) -> tuple[int, str, str]:
        try:
            result = run_tool(
                [*prefix, "--jobs", str(batch_jobs), *map(str, batch)], capture_output=True, text=True, errors="replace"
            )
        except OSError as error:
            return 2, "", f"format: failed to launch {binary}: {error}\n"
        return result.returncode, result.stdout, result.stderr

    with ThreadPoolExecutor(max_workers=max(1, min(jobs, len(batches)))) as pool:
        return report(list(pool.map(format_batch, batches)))


def run_formatter(formatter: str, files: list[Path], language: str | None, check: bool) -> int:
    executable = shutil.which(COMMANDS[formatter][0])
    if executable is None:
        print(f"format: could not find {formatter}", file=sys.stderr)
        return 1

    if formatter == "taplo" and language == "toml":
        for file in files:
            result = subprocess.run(
                [
                    executable,
                    *COMMANDS[formatter][1:],
                    "--stdin-filepath",
                    f"{file}.toml",
                    "-",
                ],
                input=file.read_bytes(),
                stdout=subprocess.PIPE,
                check=False,
            )
            if result.returncode != 0:
                return result.returncode
            if check:
                if result.stdout != file.read_bytes():
                    print(f"format: {file} requires taplo formatting", file=sys.stderr)
                    return 1
            else:
                file.write_bytes(result.stdout)
        return 0

    # The other formatters take a whole tree in one process; only a command
    # line too long for the platform splits the run.
    prefix = [
        executable,
        *COMMANDS[formatter][1:],
        *MODE_ARGUMENTS[formatter][check],
        *language_arguments(language, files),
    ]
    outcomes = []
    for batch in batch_files(files, len(files), prefix):
        result = run_tool([*prefix, *map(str, batch)], capture_output=True, text=True, errors="replace")
        outcomes.append((result.returncode, result.stdout, result.stderr))
    return report(outcomes)


def format_files(args: argparse.Namespace, selected: dict[str, list[Path]]) -> int:
    cpp_files = selected.get("clang-format", [])
    backend = "clang-format"
    clice = workspace = None
    if cpp_files:
        binary = None
        backend, binary, workspace = resolve_backend(args.backend, cpp_files)
        clice = binary

    groups: list[tuple[str, str, list[Path]]] = []
    for formatter in ("clang-format", "ruff", "prettier", "taplo"):
        if formatter not in selected:
            continue
        label = "clice format" if formatter == "clang-format" and backend == "clice" else formatter
        groups.append((formatter, label, selected[formatter]))

    action = "Checking" if args.check else "Formatting"
    for formatter, label, files in groups:
        print(f"{action} {len(files)} file(s) with {label}...", flush=True)

    def run(group: tuple[str, str, list[Path]]) -> int:
        formatter, _, files = group
        if formatter != "clang-format":
            return run_formatter(formatter, files, args.language, args.check)
        if clice is not None and workspace is not None:
            returncode = run_clice_format(clice, workspace, files, args.check, args.jobs)
            if returncode != 2:
                return returncode
            print("format: clice format could not complete; using clang-format", file=sys.stderr)
        return run_clang_format(shutil.which("clang-format"), files, args.check, args.jobs)

    with ThreadPoolExecutor(max_workers=max(1, len(groups))) as pool:
        returncodes = [future.result() for future in [pool.submit(run, group) for group in groups]]

    for returncode in returncodes:
        if returncode != 0:
            return returncode
    return 0


def main() -> int:
    args = parse_args()
    args.engine = True
    try:
        paths = engine_files(args.paths, args.engine_root)
    except (ValueError, FileNotFoundError, subprocess.CalledProcessError) as error:
        print(f"format: {error}", file=sys.stderr)
        return 1

    if not paths:
        print("format: no owned engine C++ files selected", file=sys.stderr)
        return 1

    selected = select_formatters(paths, args.language)
    if not selected:
        print("No supported files to format.")
        return 0

    return format_files(args, selected)


if __name__ == "__main__":
    sys.exit(main())
