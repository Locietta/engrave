#!/usr/bin/env python3
"""Compare engrave's public output API with the A2 goldens.

Runs the fork's ``engrave_output_probe``, which renders through the public API
(``Document::render``/``render_audio``) and writes the produced artifacts, then
compares every artifact with the stored A2 golden exactly the way
``tests/support/golden.py`` does:

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

Usage:
  test_public_output.py --probe P --assets A --soundfont SF --golden DIR --allow FILE \\\\
                    --output DIR [--render-fixtures f1,f2] [--audio-fixtures f1,f2]
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


import argparse
import subprocess
import sys
from pathlib import Path

from support.golden import (
    DEFAULT_PLATFORM,
    allow_accept,
    describe_audio,
    describe_pdf,
    describe_png,
    files_equal,
    load_allow,
    normalize_svg,
    read_text,
    sha256,
    write_json,
    write_text,
)

DEFAULT_RENDER_FIXTURES = ("ghost_note.mscx", "repeat70.mscx", "slurs.mscz")
# Two whose audio.json is identical to the golden and one that the golden lane
# accepts through the allow-list.
DEFAULT_AUDIO_FIXTURES = ("ghost_note.mscx", "lyrics.mscx", "repeat70.mscx")


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


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
    scope: str | None = None,
) -> None:
    import json
    from support.golden import verify_golden_index
    index = json.loads((golden_root / "index.json").read_text(encoding="utf8"))
    issues = verify_golden_index(golden_root, index)
    if issues or fixture not in index or not set(names) <= set(index[fixture]):
        raise ValueError("invalid indexed baseline: " + repr(issues))
    if scope is None:
        from support.inventory import validate_actual
        inventory = json.loads((Path(__file__).resolve().parents[1] / "data/gate-inventory.json").read_text(encoding="utf8"))
        validate_actual(fixture, out, names, inventory)
    else:
        expected = {"audio.json"} if scope == "audio" else {
            n for n in index[fixture] if n.startswith("page-") and n.endswith(".svg")
        } | {"page-0.png.json", "score.pdf.json"}
        if set(names) != expected or any(not (out / n).is_file() for n in expected):
            raise ValueError("partial output inventory mismatch")
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
            if matched_allow is not None:
                matched_allow.add(position)
            counts["allowed"] += 1
            print(f"allowed      {fixture}/{name} ({entry['step']})")
            continue
        counts["different"] += 1
        suffix = f" (allow mismatch: {problem})" if problem else ""
        print(f"different    {fixture}/{name} actual sha256={actual_sha}{suffix}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True, help="engrave_output_probe")
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--soundfont", type=Path, action="append", required=True)
    parser.add_argument("--golden", type=Path, required=True, help="tests/data/golden/<platform>")
    parser.add_argument("--allow", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True, help="directory of the golden fixtures")
    parser.add_argument("--render-fixtures", default=",".join(DEFAULT_RENDER_FIXTURES))
    parser.add_argument("--audio-fixtures", default=",".join(DEFAULT_AUDIO_FIXTURES))
    parser.add_argument("--platform", default=DEFAULT_PLATFORM, choices=("windows", "linux"))
    parser.add_argument("--skip-render", action="store_true")
    parser.add_argument("--skip-audio", action="store_true")
    args = parser.parse_args()

    if not args.probe.is_file():
        return fail(f"no probe at {args.probe}")
    allow = load_allow(args.allow)
    counts = {"identical": 0, "allowed": 0, "different": 0, "missing-golden": 0}
    report: dict = {
        "golden": str(args.golden),
        "probe": str(args.probe),
        "fixtures": {},
    }

    fixtures = [] if args.skip_render else [name for name in args.render_fixtures.split(",") if name]
    for fixture in fixtures:
        source = args.corpus / fixture
        if not source.is_file():
            return fail(f"no fixture {source}")
        raw, out = args.output / "raw" / fixture, args.output / "actual" / fixture
        raw.mkdir(parents=True, exist_ok=True)
        out.mkdir(parents=True, exist_ok=True)
        error = run_probe(probe_command(args.probe, args.assets, args.soundfont, "render", source, raw))
        if error:
            return fail(f"{fixture}: render failed: {error}")
        names = describe_render(raw, out)
        import json
        indexed = json.loads((args.golden / "index.json").read_text(encoding="utf8"))[fixture]
        expected = {n for n in indexed if n.startswith("page-") and n.endswith(".svg")} | {"page-0.png.json", "score.pdf.json"}
        if set(names) != expected:
            raise ValueError("render inventory mismatch")
        compare(args.golden, fixture, out, names, allow, args.platform, counts, scope="render")
        report["fixtures"].setdefault(fixture, {})["render"] = names

    fixtures = [] if args.skip_audio else [name for name in args.audio_fixtures.split(",") if name]
    for fixture in fixtures:
        source = args.corpus / fixture
        if not source.is_file():
            return fail(f"no fixture {source}")
        raw, out = args.output / "raw" / fixture, args.output / "actual" / fixture
        raw.mkdir(parents=True, exist_ok=True)
        out.mkdir(parents=True, exist_ok=True)
        error = run_probe(
            probe_command(
                args.probe,
                args.assets,
                args.soundfont,
                "audio",
                source,
                raw / "score.wav",
            )
        )
        if error:
            return fail(f"{fixture}: audio failed: {error}")
        names = describe_audio_file(raw / "score.wav", out)
        compare(args.golden, fixture, out, names, allow, args.platform, counts, scope="audio")
        report["fixtures"].setdefault(fixture, {})["audio"] = names

    report["counts"] = counts
    report["status"] = "pass" if counts["different"] == 0 and counts["missing-golden"] == 0 else "fail"
    write_json(args.output / "report.json", report, indent=2)
    print(
        f"A4 output vs A2 goldens ({args.platform}): " + ", ".join(f"{count} {name}" for name, count in counts.items())
    )
    print(f"status: {report['status'].upper()}")
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
