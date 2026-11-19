#!/usr/bin/env python3
"""Install or verify the engine's runtime assets from committed Git blobs."""

import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "engrave/assets.json"


def relative(name):
    p = PurePosixPath(name)
    if not name or p.is_absolute() or ".." in p.parts or "\\" in name or ":" in name:
        raise ValueError(f"Unsafe asset path: {name}")
    return p


def read_blobs(sources):
    if not sources:
        return {}
    proc = subprocess.Popen(
        ["git", "-C", str(ROOT), "cat-file", "--batch"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
    )
    query = "".join(f"HEAD:{src}\n" for src in sources).encode("utf-8")
    out, _ = proc.communicate(query)
    if proc.returncode != 0:
        raise RuntimeError(f"git cat-file --batch failed with exit code {proc.returncode}")
    blobs = {}
    pos = 0
    for src in sources:
        nl = out.index(b"\n", pos)
        header = out[pos:nl].decode("utf-8")
        pos = nl + 1
        parts = header.split()
        if len(parts) != 3 or parts[1] != "blob":
            raise ValueError(f"Missing committed asset blob: {src}")
        size = int(parts[2])
        blobs[src] = out[pos : pos + size]
        pos += size + 1  # trailing LF emitted by git cat-file --batch
    return blobs


def install(destination, verify=False):
    # Git checkouts may use CRLF; the installed manifest is portable LF UTF-8.
    manifest_bytes = MANIFEST.read_text(encoding="utf-8").encode("utf-8")
    manifest = json.loads(manifest_bytes)
    if manifest["version"] != 2:
        raise ValueError("Unsupported asset manifest version")
    files = manifest["files"]
    for name, source in files.items():
        relative(name)
        relative(source)
    blobs = read_blobs(list(dict.fromkeys(files.values())))
    if verify:
        if (destination / "assets.json").read_bytes() != manifest_bytes:
            raise ValueError("Installed manifest does not match the engine")
        for name, source in files.items():
            if (destination / name).read_bytes() != blobs[source]:
                raise ValueError(f"Asset content mismatch: {name}")
        return
    # Never adopt or overwrite an existing directory. A failed install leaves
    # no partially published directory and cannot damage an embedder's assets.
    if destination.exists():
        raise FileExistsError(f"{destination}: use --verify or a fresh destination")
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".engrave-assets-", dir=destination.parent))
    try:
        for name, source in files.items():
            target = staging / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(blobs[source])
        (staging / "assets.json").write_bytes(manifest_bytes)
        staging.rename(destination)
    finally:
        if staging.exists():
            shutil.rmtree(staging)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    install(args.destination.resolve(), args.verify)
    print("Verified" if args.verify else "Installed", args.destination)
