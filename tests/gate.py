#!/usr/bin/env python3
"""Public C++ boundary gate. See docs/dev/validation.md for coverage and invocation."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import subprocess
import sys
import threading
import time
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from engrave_testing import golden as g
from engrave_testing.dependencies import validate_dependencies
from engrave_testing.inventory import validate_actual, validate_allow_usage, validate_baseline
from engrave_testing.links import setup_link
from engrave_testing.paths import INPUT_ROOT, REPO_ROOT, pixi_env
from engrave_testing.public_output import (
    compare,
    describe_audio_file,
    describe_render,
    describe_render_pdf,
    probe_command,
    run_capture_pipeline,
)

PROFILES = ("release", "debug", "releasedbg", "asan")


def manifest_fingerprint(manifest: Path) -> dict:
    files = [manifest, manifest.parent / "pixi.lock"]
    return {
        "path": str(manifest),
        "sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files if path.is_file()},
    }


def main():

    validate_dependencies()
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--engine-source", type=Path, default=REPO_ROOT)
    for name in ("assets", "soundfont", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument(
        "--build-dir",
        type=Path,
        help="Build output directory (default: <engine-source>/build/gate-<profile>)",
    )
    p.add_argument(
        "--ccachedir",
        type=Path,
        help="xmake compile cache directory (default: <engine-source>/build/.build_cache)",
    )
    p.add_argument(
        "--profile",
        choices=PROFILES,
        default="release",
        help="xmake build mode (default: release)",
    )
    p.add_argument(
        "--pixi-manifest",
        type=Path,
        help="pixi.toml whose default environment provides cairo and zlib (default: the engine source's)",
    )
    p.add_argument("--mutex-wrapper", type=Path)
    p.add_argument("-j", "--jobs", type=int, default=8, help="Compile jobs (default: 8)")
    p.add_argument(
        "-J",
        "--test-jobs",
        type=int,
        default=min(8, os.cpu_count() or 4),
        help="Concurrent test workers (default: min(8, CPU count))",
    )
    p.add_argument(
        "--clean-config",
        action="store_true",
        help="Force xmake f -c even when the isolated per-build-directory config can be reused",
    )
    p.add_argument(
        "--skip-build",
        action="store_true",
        help="use the engine_tests binaries already built in --build-dir, such as an embedding project's build",
    )
    p.add_argument(
        "--quick",
        action="store_true",
        help="Skip exclusive timing checks (output-timing and dense-cancellation); use full gate at milestones",
    )
    p.add_argument("--dual-capture", type=Path, help="old golden lane actual directory")
    a = p.parse_args()
    for key, value in vars(a).items():
        if isinstance(value, Path):
            setattr(a, key, value.resolve())
    a.build_dir = (a.build_dir or a.engine_source / "build" / ("gate-" + a.profile)).resolve()
    a.ccachedir = (a.ccachedir or a.engine_source / "build" / ".build_cache").resolve()
    a.jobs = max(1, a.jobs)
    a.test_jobs = max(1, a.test_jobs)
    if (a.output / "actual").exists() or (a.output / "raw").exists():
        raise RuntimeError("Use a fresh output directory; retained captures must never mask missing outputs")
    a.output.mkdir(parents=True, exist_ok=True)
    platform = "windows" if sys.platform == "win32" else "linux"
    suffix = ".exe" if platform == "windows" else ""
    manifest = a.pixi_manifest or a.engine_source / "pixi.toml"
    native = pixi_env(manifest.parent)
    native_fonts = native / "fonts"  # the environment root's fonts/ on every platform
    if platform == "windows":
        native = native / "Library"
    # xmake's pixi:: package manager must resolve this workspace, not one
    # inherited from an enclosing `pixi run` (whose CONDA_PREFIX it prefers).
    os.environ["PIXI_PROJECT_MANIFEST"] = str(manifest)
    for name in ("CONDA_PREFIX", "PIXI_ENVIRONMENT_NAME"):
        os.environ.pop(name, None)
    # The binaries load the environment's shared libraries (cairo, zlib) at run
    # time: Windows through PATH, Linux through the run path xmake links in.
    os.environ["PATH"] = str(native / "bin") + os.pathsep + os.environ.get("PATH", "")
    os.environ["PYTHONPATH"] = os.pathsep.join(
        (str(REPO_ROOT / "tests"), str(REPO_ROOT / "tools"), os.environ.get("PYTHONPATH", ""))
    )
    lock = threading.Lock()
    records = []
    start = time.monotonic()

    def log_record(record, quiet=False):
        with lock:
            records.append(record)
            (a.output / "timings.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
            if not quiet:
                print(f"{record['name']}: {record['exit']} ({record['seconds']}s)", flush=True)

    def run(name, command, cwd=None, env=None, quiet=False):
        command = list(map(str, command))
        t = time.monotonic()
        with (a.output / (name + ".log")).open("wb") as log:
            r = subprocess.run(
                command,
                cwd=cwd or REPO_ROOT,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=1800,
                env={**os.environ, **env} if env else None,
            )
        record = dict(
            name=name,
            command=command,
            seconds=round(time.monotonic() - t, 3),
            exit=r.returncode,
        )
        log_record(record, quiet)
        if r.returncode:
            raise RuntimeError(f"{name}: see {a.output / (name + '.log')}")
        return record

    def run_check(name, check):
        t = time.monotonic()
        try:
            check()
        except Exception as exc:
            (a.output / (name + ".log")).write_text(str(exc) + "\n", encoding="utf-8")
            log_record(dict(name=name, command=[], seconds=round(time.monotonic() - t, 3), exit=1))
            raise
        log_record(dict(name=name, command=[], seconds=round(time.monotonic() - t, 3), exit=0))

    def run_task(task):
        try:
            record = run(task["name"], task["command"], task.get("cwd"), task.get("env"), quiet=True)
            return task["name"], record, None
        except Exception as exc:
            return task["name"], None, exc

    def run_parallel(tasks):
        outcomes = []
        with ThreadPoolExecutor(max_workers=a.test_jobs) as pool:
            futures = [pool.submit(run_task, task) for task in tasks]
            for future in futures:
                outcomes.append(future.result())
        # Report buffered results in submission order, never completion order.
        for name, record, _error in outcomes:
            if record is not None:
                print(f"{name}: {record['exit']} ({record['seconds']}s)", flush=True)
        error = next((error for _name, _record, error in outcomes if error), None)
        if error:
            raise error

    def xmake(name, args, env):
        cmd = ["xmake", *args]
        if platform == "windows" and a.mutex_wrapper:
            encoded = base64.b64encode(json.dumps(list(map(str, args))).encode("utf-8")).decode("ascii")
            cmd = [
                "pwsh",
                "-NoProfile",
                "-File",
                a.mutex_wrapper,
                "-ProjectPath",
                a.engine_source,
                "-Log",
                a.output / (name + "-mutex.log"),
                "-ArgsBase64",
                encoded,
            ]
        run(name, cmd, a.engine_source, env)

    # Runtime assets gate every capture; a broken manifest must fail before the
    # engine compiles, not after the full build.
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
    if not a.skip_build:
        run("pixi-install", ["pixi", "install", "--locked", "--manifest-path", manifest])
        # An isolated config per build directory makes reuse safe: the pixi
        # manifest is not an xmake option, so its fingerprint is the stamp that
        # decides whether the cached configuration may be kept.
        config_dir = a.build_dir / ".xmake-config"
        config_dir.mkdir(parents=True, exist_ok=True)
        xmake_env = {"XMAKE_CONFIGDIR": str(config_dir)}
        try:
            a.build_dir.relative_to(a.engine_source)
        except ValueError:
            pass
        else:
            # Share built source packages across profiles/build dirs in this checkout.
            xmake_env["XMAKE_PKG_LOCALDIR"] = str(a.engine_source / "build/.packages")
        a.ccachedir.mkdir(parents=True, exist_ok=True)
        stamp = {
            "manifest": manifest_fingerprint(manifest),
            "profile": a.profile,
            "build_dir": str(a.build_dir),
            "ccachedir": str(a.ccachedir),
        }
        # Keep the stamp outside the config directory: `xmake f -c` may clear
        # that directory, and the next run must still see whether reuse is safe.
        stamp_path = a.build_dir / "gate-config.json"
        clean_config = a.clean_config
        if not clean_config:
            try:
                clean_config = json.loads(stamp_path.read_text(encoding="utf-8")) != stamp
            except (OSError, ValueError):
                clean_config = True
        config = ["f"]
        if clean_config:
            config.append("-c")
        config += [
            "-P",
            ".",
            "-y",
            "-m",
            a.profile,
            "-o",
            a.build_dir,
            "--ccache=y",
            "--ccachedir=" + str(a.ccachedir),
            "--engrave_tests=y",
            # Pinned inventories describe the harness-off build; a reused config must not keep it on.
            "--engrave_playback_harness=n",
        ]
        if platform == "linux" and (native / "bin/x86_64-conda-linux-gnu-gcc").is_file():
            config += [
                "--cc=" + str(native / "bin/x86_64-conda-linux-gnu-gcc"),
                "--cxx=" + str(native / "bin/x86_64-conda-linux-gnu-g++"),
            ]
        xmake("configure", config, xmake_env)
        stamp_path.write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
        xmake("build", ["build", "-P", ".", "-y", "-j" + str(a.jobs), "engine_tests"], xmake_env)
    forbidden = re.compile(
        r"\bmuse::|\bmu::|\bkors\b|\bnlohmann\b|\bglz::|\bQt\b|\bQ[A-Z]\w*\b|"
        r'#\s*include\s*[<"](?:muse/|framework/|engraving/|nlohmann/|glaze/)'
    )

    def check_headers():
        for header in (a.engine_source / "engrave/include/engrave").rglob("*.h"):
            if forbidden.search(header.read_text(encoding="utf-8")):
                raise RuntimeError(f"public header leak: {header}")

    run_check("header_scan", check_headers)
    run(
        "sine",
        [
            sys.executable,
            REPO_ROOT / "tests/engrave_testing/make_sine_soundfont.py",
            a.output / "sine.sf2",
        ],
    )
    asset = "--assets=" + str(a.assets)
    corpus = "--corpus=" + str(REPO_ROOT / "tests/data/inputs/golden")
    output_args = [
        asset,
        corpus,
        "--corpus=" + str(REPO_ROOT / "tests/data/inputs/audio"),
        "--soundfont=" + str(a.soundfont),
        "--soundfont=" + str(a.output / "sine.sf2"),
        "--native-fonts=" + str(native_fonts),
    ]
    dense_timing_test = "AudioInternal.DenseScoreCancellationLatencyAndRecovery"
    shared_timing_tests = ":".join(
        (
            "AudioInternal.CancellationLatencyDistribution",
            "AudioInternal.FirstRenderCancelThenRepeatedRenderAndTeardown",
            "AudioInternal.WarmGraphRebuildTiming",
        )
    )
    excluded_timing_tests = dense_timing_test + ":" + shared_timing_tests
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
        "engrave_gp_import_test": [asset],
        "engrave_mnx_import_test": [asset, "--mnx-out=" + str(a.output / "mnx-export")],
        "engrave_mnx_model_test": [],
        "engrave_output_test": output_args,
        "diagnostics_test": [],
        "io_boundary_test": [],
    }
    tasks = [
        dict(name="comparator-mutations", command=[sys.executable, REPO_ROOT / "tests/test_inventory.py"]),
        dict(
            name="playback-witness-controls",
            command=[sys.executable, "-O", REPO_ROOT / "tests/test_playback_witness.py"],
        ),
        dict(name="oracle-controls", command=[sys.executable, REPO_ROOT / "tests/integration/test_oracle_harness.py"]),
    ]
    for name, args in suites.items():
        cwd = a.output / name
        cwd.mkdir(exist_ok=True)
        command = [a.build_dir / (name + suffix), *args]
        if name == "engrave_output_test":
            # Keep all latency, teardown, and warm-timing tests isolated in Phase C.
            command.append("--gtest_filter=-" + excluded_timing_tests)
        tasks.append(dict(name=name, command=command, cwd=cwd))
    run_parallel(tasks)
    # Schema validation reads the import suite's export; it starts only after
    # that suite has completed.
    run(
        "mnx_schema",
        [
            sys.executable,
            REPO_ROOT / "tests/engrave_testing/check_mnx_schema.py",
            a.output / "mnx-export",
            a.engine_source / "src/importexport/mnx/model/mnx-schema.json",
        ],
    )
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
            # Keep engraving_playback_tests serial relative to mpe_tests for its 2.5 ms timing check.
            "--jobs",
            "1",
        ],
    )
    # These pinned engine tests still read cwd-relative fixtures and engine data.
    # Each suite gets its own capture directory so a shared engine-cwd can never
    # race between them.
    for name in ("score_edit_test", "score_audio_test", "score_positions_test"):
        engine_cwd = a.output / "engine-cwd" / name
        (engine_cwd / "vendor").mkdir(parents=True, exist_ok=True)
        setup_link(INPUT_ROOT, engine_cwd / "fixtures")
        setup_link(a.engine_source, engine_cwd / "vendor/engrave")
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
            native / "fonts/DejaVuSans.ttf"
            if (native / "fonts/DejaVuSans.ttf").is_file()
            else Path("/usr/share/fonts/TTF/DejaVuSans.ttf")
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
        "asset-installer",
        [sys.executable, "-B", a.engine_source / "tools/test_install_assets.py"],
        cwd=a.engine_source / "tools",
    )
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
    golden_root = REPO_ROOT / "tests/data/golden" / platform
    inventory = validate_baseline(golden_root)
    allow = g.load_allow(REPO_ROOT / "tests/data/golden/allow.json", context="engine-gate-archive")
    counts = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
    dual = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
    matched_allow = set()
    dual_matched = set()
    dual_inventory = validate_baseline(a.dual_capture) if a.dual_capture else None
    probe = a.build_dir / ("engrave_output_probe" + suffix)
    semantic_dump = a.build_dir / ("semantic_dump" + suffix)

    def capture_worker(source: Path):
        raw = a.output / "raw" / source.name
        out = a.output / "actual" / source.name
        raw.mkdir(parents=True, exist_ok=True)
        out.mkdir(parents=True, exist_ok=True)
        produced = []
        for mode in ("export", "render", "audio"):
            produced.append(
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
                    quiet=True,
                )
            )
        with zipfile.ZipFile(raw / "score.mscz") as archive:
            master = next(n for n in archive.namelist() if n.endswith(".mscx") and "/" not in n)
            (raw / "score.mscx").write_bytes(archive.read(master))
        dump = subprocess.check_output(
            [
                str(semantic_dump),
                str(a.assets),
                str(raw / "score.mscz"),
            ],
            timeout=120,
        )
        g.write_json(out / "semantic.json", json.loads(dump))
        # PyMuPDF initializes as single-threaded; PDF normalization runs on the
        # main thread in finalize_capture after the worker pool drains.
        names = (
            ["semantic.json"]
            + describe_render(raw, out, include_pdf=False)
            + describe_audio_file(raw / "score.wav", out)
        )
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
        expected_svg = {p.name for p in (golden_root / source.name).glob("page-*.svg")}
        if {n for n in names if n.endswith(".svg")} != expected_svg:
            raise RuntimeError(f"{source.name}: SVG page coverage changed")
        return source, raw, out, produced, names

    def print_produced(item) -> None:
        _source, _raw, _out, produced, _names = item
        for record in produced:
            print(f"{record['name']}: {record['exit']} ({record['seconds']}s)", flush=True)

    def finalize_capture(item) -> None:
        source, raw, out, _produced, names = item
        names = [*names, *describe_render_pdf(raw, out)]
        validate_actual(source.name, out, names, inventory)
        compare(
            golden_root,
            source.name,
            out,
            names,
            allow,
            platform,
            counts,
            matched_allow,
            inventory=inventory,
        )
        if dual_inventory is not None:
            compare(
                a.dual_capture,
                source.name,
                out,
                names,
                [],
                platform,
                dual,
                dual_matched,
                inventory=dual_inventory,
            )

    run_capture_pipeline(sources, capture_worker, finalize_capture, a.test_jobs, on_captured=print_produced)
    if {p.name for p in (a.output / "actual").iterdir()} != set(inventory):
        raise RuntimeError("gate fixture inventory mismatch")
    run(
        "semantic-cross-check",
        [sys.executable, REPO_ROOT / "tests/engrave_testing/check_semantic_dump.py", a.output / "actual"],
    )
    # Entries are only expected to match on the platforms they pin.
    validate_allow_usage(allow, matched_allow, platform)
    # Exclusive timing lane: every parallel pool above has drained, so the
    # cancellation budgets, teardown timing, and audio render measure a quiet machine.
    if a.quick:
        print(
            "quick mode: skipped output-timing and dense-cancellation timing checks (required at milestones)",
            flush=True,
        )
    else:
        run(
            "output-timing",
            [a.build_dir / ("engrave_output_test" + suffix), *output_args, "--gtest_filter=" + shared_timing_tests],
            cwd=a.output / "engrave_output_test",
        )
        run(
            "dense-cancellation",
            [a.build_dir / ("engrave_output_test" + suffix), *output_args, "--gtest_filter=" + dense_timing_test],
            cwd=a.output / "engrave_output_test",
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
        quick=a.quick,
        seconds=round(time.monotonic() - start, 3),
        engine=subprocess.check_output(["git", "-C", str(a.engine_source), "rev-parse", "HEAD"], text=True).strip(),
    )
    (a.output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if any(c[k] for c in (counts, dual) for k in ("different", "missing-golden")):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
