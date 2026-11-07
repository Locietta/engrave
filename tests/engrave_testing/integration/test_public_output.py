#!/usr/bin/env python3
"""Compare engrave's public output API with the platform goldens.

Runs the fork's ``engrave_output_probe``, which renders through the public API
(``Document::render``/``render_audio``) and writes the produced artifacts, then
compares every artifact with the stored platform golden exactly the way
``tests/engrave_testing/golden.py`` does:

* ``page-0.png.json``: ``describe_png`` (size, PIL mode, sha256 of the RGBA
  bytes) - so the pixels must equal the golden's, not just the file size;
* ``score.pdf.json``: ``describe_pdf`` (PyMuPDF normalization, page count,
  sha256 of the normalized document);
* ``page-N.svg``: the normalized SVG text;
* ``audio.json``: ``describe_audio`` features with the documented tolerances
  (``audio_equal``), including the exact-silence window evidence.

An output that is not identical is accepted only when ``tests/data/golden/allow.json``
pins the actual file's sha256 for this platform, i.e. under the same rule the
golden lane uses.

This module is used by tests/gate.py; partial captures are not an acceptance CLI.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

from engrave_testing.golden import (
    allow_accept,
    describe_audio,
    describe_pdf,
    describe_png,
    files_equal,
    normalize_svg,
    read_text,
    sha256,
    write_json,
    write_text,
)
from engrave_testing.inventory import validate_actual, validate_allow_usage, validate_baseline


def probe_command(
    probe: Path,
    assets: Path,
    soundfonts: list[Path],
    mode: str,
    source: Path,
    out: Path,
) -> list[str]:
    command = [str(probe), f"--assets={assets}"]
    command += [f"--soundfont={soundfont}" for soundfont in soundfonts]
    command.append(f"--{mode}={source}")
    if mode == "audio":
        command.append("--format=wav")
    command.append(f"--out={out}")
    return command


def run_probe(command: list[str]) -> str | None:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        return (result.stderr or result.stdout).strip() or f"probe exited {result.returncode}"
    return None


def describe_render(raw: Path, out: Path) -> list[str]:
    """Write the golden.py representations of the probe's render, return their names."""
    names = []
    for svg in sorted(raw.glob("page-*.svg")):
        write_text(out / svg.name, normalize_svg(read_text(svg)))
        names.append(svg.name)
    png = raw / "page-0.png"
    if png.is_file():
        write_json(out / "page-0.png.json", describe_png(png))
        names.append("page-0.png.json")
    pdf = raw / "score.pdf"
    if pdf.is_file():
        write_json(out / "score.pdf.json", describe_pdf(pdf))
        names.append("score.pdf.json")
    return names


def describe_audio_file(raw: Path, out: Path) -> list[str]:
    write_json(out / "audio.json", describe_audio(raw))
    return ["audio.json"]


def compare(
    golden_root: Path,
    fixture: str,
    out: Path,
    names: list[str],
    allow: list[dict],
    platform: str,
    counts: dict,
    matched_allow: set | None = None,
) -> None:
    inventory = validate_baseline(golden_root)
    validate_actual(fixture, out, names, inventory)
    local_matched = set()
    for name in names:
        golden_file, actual_file = golden_root / fixture / name, out / name
        actual_sha = sha256(actual_file.read_bytes())
        if not golden_file.is_file():
            counts["missing-golden"] += 1
            print(f"no golden    {fixture}/{name}")
            continue
        if files_equal(golden_file, actual_file, name):
            counts["identical"] += 1
            print(f"identical    {fixture}/{name}")
            continue
        position, entry, problem = allow_accept(allow, fixture, name, "different", actual_sha, platform)
        if entry:
            local_matched.add(position)
            if matched_allow is not None:
                matched_allow.add(position)
            counts["allowed"] += 1
            print(f"allowed      {fixture}/{name} ({entry['step']})")
            continue
        counts["different"] += 1
        suffix = f" (allow mismatch: {problem})" if problem else ""
        print(f"different    {fixture}/{name} actual sha256={actual_sha}{suffix}")

    validate_allow_usage(allow, local_matched, platform, {(fixture, n) for n in names})


if __name__ == "__main__":
    raise SystemExit("Partial comparison CLI removed; use tests/gate.py for validated captures")
