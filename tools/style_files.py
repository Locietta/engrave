"""Shared tracked, owned engine file selection for style tools."""

import subprocess
from pathlib import Path

CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
ENGINE_ROOT = Path(__file__).resolve().parents[1]


def engine_files(paths, engine_root=ENGINE_ROOT):
    root = Path(engine_root).resolve()
    top = Path(
        subprocess.check_output(["git", "-C", str(root), "rev-parse", "--show-toplevel"], text=True).strip()
    ).resolve()
    if top != root:
        raise ValueError(f"not an initialized engine checkout: {root}")
    selections = [path.resolve() for path in paths]
    for path in selections:
        if not path.exists():
            raise FileNotFoundError(path)
    output = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z", "--", "engrave/", "src/", "muse/"])
    files = []
    for entry in output.split(b"\0"):
        if not entry:
            continue
        relative = Path(entry.decode("utf-8"))
        path = root / relative
        if "thirdparty" in relative.parts or path.suffix.lower() not in CPP_EXTENSIONS or not path.is_file():
            continue
        if any(path == selection or selection in path.parents for selection in selections):
            files.append(path)
    return files
