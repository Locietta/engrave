#!/usr/bin/env python3
"""Prune Bazel's action/CAS disk cache down to a byte budget (oldest mtime first)."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

MIB = 1024 * 1024


def find_bazel(root: Path) -> str:
    found_bazel = shutil.which("bazel")
    if found_bazel:
        return found_bazel
    pixi_prefix = root / ".pixi/envs/default"
    for cand in (
        pixi_prefix / "Library/bin/bazelisk.exe",
        pixi_prefix / "bin/bazelisk",
        pixi_prefix / "Library/bin/bazel.exe",
        pixi_prefix / "bin/bazel",
    ):
        if cand.is_file():
            return str(cand)
    found_bazelisk = shutil.which("bazelisk")
    if found_bazelisk:
        return found_bazelisk
    raise SystemExit("bazelisk/bazel not found")


def _clean_tmp_dir(tmp_dir: Path) -> int:
    if not tmp_dir.is_dir() or tmp_dir.is_symlink():
        return 0
    removed = 0
    for entry in os.scandir(tmp_dir):
        path = Path(entry.path)
        try:
            if entry.is_dir(follow_symlinks=False):
                shutil.rmtree(path)
            else:
                path.unlink()
            removed += 1
        except FileNotFoundError:
            pass
    return removed


def _collect_cache_files(subdir: Path) -> list[tuple[int, int, Path]]:
    entries: list[tuple[int, int, Path]] = []
    if not subdir.is_dir() or subdir.is_symlink():
        return entries
    stack = [subdir]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    if entry.is_dir(follow_symlinks=False):
                        stack.append(Path(entry.path))
                    elif entry.is_file(follow_symlinks=False):
                        try:
                            st = entry.stat(follow_symlinks=False)
                        except FileNotFoundError:
                            continue
                        entries.append((st.st_mtime_ns, st.st_size, Path(entry.path)))
        except FileNotFoundError:
            continue
    return entries


def prune_disk_cache(disk_cache: Path, max_bytes: int) -> tuple[int, int, int]:
    """Remove stale tmp/ files and evict oldest ac/ and cas/ entries until <= max_bytes."""
    if max_bytes <= 0:
        raise ValueError("max_bytes must be positive")
    if disk_cache.is_symlink():
        raise ValueError(f"refusing to prune symlinked disk cache: {disk_cache}")
    if not disk_cache.is_dir():
        return (0, 0, 0)

    removed = _clean_tmp_dir(disk_cache / "tmp")
    files = _collect_cache_files(disk_cache / "ac") + _collect_cache_files(disk_cache / "cas")
    before_bytes = sum(size for _, size, _ in files)
    after_bytes = before_bytes

    if after_bytes > max_bytes:
        # Bazel updates entry mtime on disk-cache hits and writes, so oldest-first
        # evicts stale artifacts from earlier commits while retaining the active build.
        files.sort(key=lambda item: (item[0], str(item[2])))
        for _, size, path in files:
            if after_bytes <= max_bytes:
                break
            try:
                path.unlink()
            except FileNotFoundError:
                after_bytes -= size
                continue
            after_bytes -= size
            removed += 1

    return (before_bytes, after_bytes, removed)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disk-cache", type=Path, required=True, help="Bazel --disk_cache directory")
    parser.add_argument("--max-size-mb", type=int, required=True, help="Target upper bound in MiB")
    parser.add_argument(
        "--shutdown",
        action="store_true",
        help="Run `bazel shutdown` before pruning so no background server writes to the cache",
    )
    parser.add_argument("--bazel", help="Explicit bazel/bazelisk executable for --shutdown")
    args = parser.parse_args(argv)

    if args.max_size_mb <= 0:
        parser.error("--max-size-mb must be positive")

    root = Path(__file__).resolve().parents[1]
    if args.shutdown:
        bazel = args.bazel or find_bazel(root)
        rc = subprocess.call([bazel, "shutdown"], cwd=root)
        if rc != 0:
            return rc

    before_bytes, after_bytes, removed = prune_disk_cache(args.disk_cache, args.max_size_mb * MIB)
    print(
        f"Disk cache prune ({args.disk_cache}): "
        f"{before_bytes / MIB:.1f} MiB -> {after_bytes / MIB:.1f} MiB "
        f"(removed {removed} entries, limit {args.max_size_mb} MiB)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
