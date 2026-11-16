"""Shared public output helpers."""

from __future__ import annotations

import subprocess
from concurrent.futures import ThreadPoolExecutor
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


def describe_render_pdf(raw: Path, out: Path) -> list[str]:
    """Normalize and describe the rendered PDF on the caller thread (PyMuPDF is single-threaded)."""
    pdf = raw / "score.pdf"
    if pdf.is_file():
        write_json(out / "score.pdf.json", describe_pdf(pdf))
        return ["score.pdf.json"]
    return []


def describe_render(raw: Path, out: Path, *, include_pdf: bool = True) -> list[str]:
    """Write the golden.py representations of the probe's render, return their names."""
    names = []
    for svg in sorted(raw.glob("page-*.svg")):
        write_text(out / svg.name, normalize_svg(read_text(svg)))
        names.append(svg.name)
    png = raw / "page-0.png"
    if png.is_file():
        write_json(out / "page-0.png.json", describe_png(png))
        names.append("page-0.png.json")
    if include_pdf:
        names.extend(describe_render_pdf(raw, out))
    return names


def run_capture_pipeline(sources, capture_worker, finalize_capture, max_workers: int, on_captured=None):
    """Run worker-safe capture steps concurrently, then finalize single-threaded steps on the caller thread."""
    captured = []
    errors = []
    with ThreadPoolExecutor(max_workers=max_workers) as pool:
        futures = [pool.submit(capture_worker, source) for source in sources]
        for future in futures:
            try:
                item = future.result()
            except Exception as exc:
                errors.append(exc)
            else:
                captured.append(item)
                if on_captured is not None:
                    on_captured(item)
    if errors:
        raise errors[0]
    return [finalize_capture(item) for item in captured]


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
    inventory: dict | None = None,
) -> None:
    if inventory is None:
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
