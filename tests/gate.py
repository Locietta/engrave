#!/usr/bin/env python3
"""Public C++ boundary gate. See docs/dev/validation.md for coverage and invocation."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import zipfile
from pathlib import Path

from integration.test_public_output import compare, describe_audio_file, describe_render, probe_command
from support import golden as g
from support.dependencies import validate_dependencies
from support.inventory import validate_actual, validate_allow_usage, validate_baseline
from support.links import setup_link
from support.paths import INPUT_ROOT, REPO_ROOT


def main():

    validate_dependencies()
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--engine-source", type=Path, default=REPO_ROOT)
    for name in ("build-dir", "assets", "soundfont", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--cairo-prefix", type=Path)
    p.add_argument("--mutex-wrapper", type=Path)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--skip-build", action="store_true", help="diagnostic rerun only")
    p.add_argument("--dual-capture", type=Path, help="old golden lane actual directory")
    a = p.parse_args()
    for key, value in vars(a).items():
        if isinstance(value, Path):
            setattr(a, key, value.resolve())
    if (a.output / "actual").exists() or (a.output / "raw").exists():
        raise RuntimeError("Use a fresh output directory; retained captures must never mask missing outputs")
    a.output.mkdir(parents=True, exist_ok=True)
    platform = "windows" if sys.platform == "win32" else "linux"
    suffix = ".exe" if platform == "windows" else ""
    if a.cairo_prefix:
        os.environ["PATH"] = str(a.cairo_prefix / "bin") + os.pathsep + os.environ.get("PATH", "")
    os.environ["PYTHONPATH"] = os.pathsep.join(
        (str(REPO_ROOT / "tests"), str(REPO_ROOT / "tools"), os.environ.get("PYTHONPATH", ""))
    )
    records = []
    start = time.monotonic()

    def run(name, command, cwd=None):
        command = list(map(str, command))
        t = time.monotonic()
        with (a.output / (name + ".log")).open("wb") as log:
            r = subprocess.run(
                command,
                cwd=cwd or REPO_ROOT,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=1800,
            )
        records.append(
            dict(
                name=name,
                command=command,
                seconds=round(time.monotonic() - t, 3),
                exit=r.returncode,
            )
        )
        (a.output / "timings.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
        print(f"{name}: {r.returncode} ({records[-1]['seconds']}s)", flush=True)
        if r.returncode:
            raise RuntimeError(f"{name}: see {a.output / (name + '.log')}")

    def xmake(name, args):
        cmd = ["xmake", *args]
        if platform == "windows" and a.mutex_wrapper:
            cmd = [
                "pwsh",
                "-NoProfile",
                "-File",
                a.mutex_wrapper,
                "-ProjectPath",
                a.engine_source,
                "-Log",
                a.output / (name + "-mutex.log"),
                "-Command",
                " ".join(map(str, args)),
            ]
        run(name, cmd, a.engine_source)

    run("comparator-mutations", [sys.executable, REPO_ROOT / "tests/test_inventory.py"])
    if not a.skip_build:
        config = [
            "f",
            "-P",
            ".",
            "-y",
            "-m",
            "release",
            "-o",
            a.build_dir,
            "--ccache=y",
            "--engrave_tests=y",
        ]
        if a.cairo_prefix:
            config.append("--cairo_prefix=" + str(a.cairo_prefix))
        if platform == "linux" and a.cairo_prefix and (a.cairo_prefix / "bin/x86_64-conda-linux-gnu-gcc").is_file():
            prefix = a.cairo_prefix
            config += [
                "--cc=" + str(prefix / "bin/x86_64-conda-linux-gnu-gcc"),
                "--cxx=" + str(prefix / "bin/x86_64-conda-linux-gnu-g++"),
            ]
        xmake("configure", config)
        xmake("build", ["build", "-P", ".", "-y", "-j" + str(a.jobs), "engine_tests"])
    forbidden = re.compile(
        r'\bmuse::|\bmu::|\bkors\b|\bnlohmann\b|\bQt\b|\bQ[A-Z]\w*\b|#\s*include\s*[<"](?:muse/|framework/|engraving/|nlohmann/)'
    )
    for header in (a.engine_source / "engrave/include/engrave").rglob("*.h"):
        if forbidden.search(header.read_text(encoding="utf-8")):
            raise RuntimeError(f"public header leak: {header}")
    asset = "--assets=" + str(a.assets)
    corpus = "--corpus=" + str(REPO_ROOT / "tests/data/inputs/golden")
    run(
        "sine",
        [
            sys.executable,
            REPO_ROOT / "tests/support/make_sine_soundfont.py",
            a.output / "sine.sf2",
        ],
    )
    suites = {
        "engrave_api_test": [asset],
        "engrave_handles_test": [asset, corpus],
        "engrave_describe_test": [
            asset,
            "--python=" + sys.executable,
            REPO_ROOT / "tests/data/inputs",
        ],
        "engrave_read_test": [asset, "--corpus=" + str(REPO_ROOT / "tests/data/inputs")],
        "engrave_ops_test": [asset, corpus],
        "engrave_op_policy_test": [],
        "engrave_gp_import_test": [asset],
        "engrave_mnx_import_test": [asset, "--mnx-out=" + str(a.output / "mnx-export")],
        "engrave_output_test": [
            asset,
            corpus,
            "--corpus=" + str(REPO_ROOT / "tests/data/inputs/audio"),
            "--soundfont=" + str(a.soundfont),
            "--soundfont=" + str(a.output / "sine.sf2"),
        ],
        "diagnostics_test": [],
        "io_boundary_test": [],
    }
    for name, args in suites.items():
        cwd = a.output / name
        cwd.mkdir(exist_ok=True)
        run(name, [a.build_dir / (name + suffix), *args], cwd)
    run(
        "write470-excerpts",
        [
            sys.executable,
            REPO_ROOT / "engrave/tests/test_write470_excerpts.py",
            "--probe",
            a.build_dir / ("write470_probe" + suffix),
            "--assets",
            a.assets,
            "--out",
            a.output / "write470-excerpts",
        ],
    )
    run(
        "mnx_schema",
        [
            sys.executable,
            REPO_ROOT / "tests/support/check_mnx_schema.py",
            a.output / "mnx-export",
            a.engine_source / "src/importexport/mnx/tests/data/schema/mnx-schema.json",
        ],
    )
    run(
        "playback",
        [
            sys.executable,
            REPO_ROOT / "tests/integration/test_upstream_gtest.py",
            "--bin-dir",
            a.build_dir,
            "--assets",
            a.assets,
            "--engraving-data",
            a.engine_source / "src/engraving/tests",
            "--output",
            a.output / "gtest",
        ],
    )
    # These pinned engine tests still read cwd-relative fixtures and engine data.
    # Keep that compatibility contract inside the capture, beside generated output.
    engine_cwd = a.output / "engine-cwd"
    (engine_cwd / "vendor").mkdir(parents=True, exist_ok=True)
    setup_link(INPUT_ROOT, engine_cwd / "fixtures")
    setup_link(a.engine_source, engine_cwd / "vendor/engrave")
    for name in ("score_edit_test", "score_audio_test", "score_positions_test"):
        run(
            name,
            [
                a.build_dir / (name + suffix),
                a.assets,
                *([a.engine_source] if name == "score_audio_test" else []),
            ],
            cwd=engine_cwd,
        )
    run(
        "excerpts",
        [
            sys.executable,
            REPO_ROOT / "tests/integration/test_excerpts.py",
            "--binary",
            a.build_dir / ("excerpts_test" + suffix),
            "--assets",
            a.assets,
            "--output",
            a.output / "excerpts",
        ],
    )
    run(
        "archives",
        [
            sys.executable,
            REPO_ROOT / "tests/integration/test_archives.py",
            "--binary",
            a.build_dir / ("archive_test" + suffix),
            "--output",
            a.output / "archives",
        ],
    )
    font = (
        Path("C:/Windows/Fonts/arial.ttf")
        if platform == "windows"
        else (
            a.cairo_prefix / "fonts/DejaVuSans.ttf" if a.cairo_prefix else Path("/usr/share/fonts/TTF/DejaVuSans.ttf")
        )
    )
    run(
        "assets",
        [
            sys.executable,
            REPO_ROOT / "tests/integration/test_assets.py",
            "--binary",
            a.build_dir / ("assets_test" + suffix),
            "--assets",
            a.assets,
            "--font",
            font,
            "--soundfont",
            a.soundfont,
            "--output",
            a.output / "assets",
        ],
    )
    run(
        "asset-manifest",
        [
            sys.executable,
            "-B",
            a.engine_source / "tools/install_assets.py",
            "--verify",
            a.assets,
        ],
    )
    run(
        "asset-installer",
        [sys.executable, "-B", a.engine_source / "tools/test_install_assets.py"],
        cwd=a.engine_source / "tools",
    )
    run(
        "engine-audio",
        [
            sys.executable,
            REPO_ROOT / "tests/integration/test_engine_audio.py",
            "--engine-only",
            "--bin-dir",
            a.build_dir,
            "--resources",
            a.assets,
            "--soundfont",
            a.soundfont,
            "--engine-source",
            a.engine_source,
            "--output",
            a.output / "engine-audio",
        ],
    )
    run("oracle-controls", [sys.executable, REPO_ROOT / "tests/integration/test_oracle_harness.py"])
    sources = sorted((REPO_ROOT / "tests/data/inputs/golden").glob("*")) + [
        REPO_ROOT / "tests/data/inputs/evening.musicxml"
    ]
    sources += [
        a.engine_source / "src/importexport/mnx/tests/data" / name
        for name in (
            "engine/two-notes.mnx",
            "project_examples/tupletSimple.mnx",
            "project_examples/percussionKit.mnx",
            "project_examples/graceArps.mnx",
        )
    ]
    sources = [s for s in sources if s.suffix in {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".gp", ".mnx"}]
    if not sources:
        raise RuntimeError("incomplete supported corpus")
    run(
        "embedder",
        [a.build_dir / ("example_embedder" + suffix), a.assets, *sources],
    )
    counts = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
    dual = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
    allow = g.load_allow(REPO_ROOT / "tests/data/golden/allow.json", context="engine-gate-archive")
    matched_allow = set()

    inventory = validate_baseline(REPO_ROOT / "tests/data/golden" / platform)
    for source in sources:
        raw = a.output / "raw" / source.name
        out = a.output / "actual" / source.name
        raw.mkdir(parents=True, exist_ok=True)
        out.mkdir(parents=True, exist_ok=True)
        probe = a.build_dir / ("engrave_output_probe" + suffix)
        for mode in ("export", "render", "audio"):
            run(
                source.name + "-" + mode,
                probe_command(
                    probe,
                    a.assets,
                    [a.soundfont],
                    mode,
                    source,
                    raw / "score.wav" if mode == "audio" else raw,
                ),
            )
        with zipfile.ZipFile(raw / "score.mscz") as archive:
            master = next(n for n in archive.namelist() if n.endswith(".mscx") and "/" not in n)
            (raw / "score.mscx").write_bytes(archive.read(master))
        dump = subprocess.check_output(
            [
                str(a.build_dir / ("semantic_dump" + suffix)),
                str(a.assets),
                str(raw / "score.mscz"),
            ],
            timeout=120,
        )
        g.write_json(out / "semantic.json", json.loads(dump))
        names = ["semantic.json"] + describe_render(raw, out) + describe_audio_file(raw / "score.wav", out)
        for name, value in [
            ("score.mscx", g.normalize_mscx(g.read_text(raw / "score.mscx"))),
            (
                "score.musicxml",
                g.normalize_musicxml(g.read_text(raw / "score.musicxml")),
            ),
        ]:
            g.write_text(out / name, value)
            names.append(name)
        for name, value in [
            ("mscz.json", g.describe_mscz(raw / "score.mscz")),
            ("mxl.json", g.describe_mxl(raw / "score.mxl")),
            ("midi.json", g.describe_midi((raw / "score.mid").read_bytes())),
        ]:
            g.write_json(out / name, value)
            names.append(name)
        expected_svg = {p.name for p in (REPO_ROOT / "tests/data/golden" / platform / source.name).glob("page-*.svg")}
        if {n for n in names if n.endswith(".svg")} != expected_svg:
            raise RuntimeError(f"{source.name}: SVG page coverage changed")
        validate_actual(source.name, out, names, inventory)
        compare(
            REPO_ROOT / "tests/data/golden" / platform,
            source.name,
            out,
            names,
            allow,
            platform,
            counts,
            matched_allow,
        )
        if a.dual_capture:
            compare(a.dual_capture, source.name, out, names, [], platform, dual)
    if {p.name for p in (a.output / "actual").iterdir()} != set(inventory):
        raise RuntimeError("gate fixture inventory mismatch")
    run(
        "semantic-cross-check",
        [sys.executable, REPO_ROOT / "tests/support/check_semantic_dump.py", a.output / "actual"],
    )
    # Entries are only expected to match on the platforms they pin.
    validate_allow_usage(allow, matched_allow, platform)
    inputs = [
        REPO_ROOT / "tests/data/golden/inputs.json",
        a.soundfont,
        REPO_ROOT / "tests/data/golden/allow.json",
        *sources,
    ]
    provenance = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    report = dict(
        provenance=provenance,
        counts=counts,
        dual=dual,
        seconds=round(time.monotonic() - start, 3),
        engine=subprocess.check_output(["git", "-C", str(a.engine_source), "rev-parse", "HEAD"], text=True).strip(),
    )
    (a.output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if any(c[k] for c in (counts, dual) for k in ("different", "missing-golden")):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
