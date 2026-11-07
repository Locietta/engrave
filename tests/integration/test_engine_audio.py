from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import math
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import traceback
import xml.etree.ElementTree as ET
from collections import Counter
from collections.abc import Callable
from pathlib import Path
from typing import Any
from unittest.mock import patch

import numpy as np
import soundfile as sf
from engrave_testing import golden as golden
from engrave_testing.make_sine_soundfont import generate_sine_soundfont
from engrave_testing.paths import REPO_ROOT, pixi_env


def find_probe_binary(bin_dir: Path | None) -> Path:
    candidates = []
    if bin_dir:
        candidates.extend(
            [
                bin_dir / "engine_audio_probe.exe",
                bin_dir / "engine_audio_probe",
            ]
        )
    candidates.extend(
        [
            REPO_ROOT / "build" / "xmake-windows" / "engine_audio_probe.exe",
            REPO_ROOT / "build" / "xmake-windows" / "engine_audio_probe",
            REPO_ROOT / "build" / "xmake-linux" / "engine_audio_probe",
            REPO_ROOT / "build" / "xmake-linux" / "engine_audio_probe.exe",
            REPO_ROOT / "build" / "windows" / "engine_audio_probe.exe",
            REPO_ROOT / "build" / "linux" / "engine_audio_probe",
        ]
    )
    for c in candidates:
        if c.is_file():
            return c.resolve()
    raise FileNotFoundError(f"engine_audio_probe binary not found in {bin_dir} or standard build dirs")


def probe_env() -> dict[str, str]:
    env = os.environ.copy()
    native_env = pixi_env(REPO_ROOT)
    for extra in (native_env / "Library" / "bin", native_env / "bin"):
        if extra.is_dir():
            env["PATH"] = str(extra.resolve()) + os.pathsep + env.get("PATH", "")
    for lib in (native_env / "Library" / "lib", native_env / "lib"):
        if lib.is_dir():
            env["LD_LIBRARY_PATH"] = str(lib.resolve()) + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    return env


def run_probe(
    probe_bin: Path,
    assets: Path,
    soundfont: Path,
    score: Path,
    out_path: Path,
    extra_args: list[str] | None = None,
) -> dict[str, Any]:
    cmd = [
        str(probe_bin),
        str(assets.resolve()),
        str(soundfont.resolve()),
        str(score.resolve()),
        str(out_path.resolve()),
    ]
    if extra_args:
        cmd.extend(extra_args)

    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=probe_env(),
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"engine_audio_probe exited {proc.returncode}:\nStdout:\n{proc.stdout}\nStderr:\n{proc.stderr}"
        )
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError as err:
        raise RuntimeError(
            f"Failed to parse engine_audio_probe JSON:\nStdout:\n{proc.stdout}\nStderr:\n{proc.stderr}"
        ) from err


def golden_platform() -> str:
    return "windows" if sys.platform.startswith("win") else "linux"


def numeric_values(values: list[Any]) -> list[float]:
    """Flatten scalar lists and per-channel lists (rmsEnvelope is nested)."""
    if values and isinstance(values[0], list):
        return [float(item) for pair in values for item in pair]
    return [float(item) for item in values]


def decode_audio(path: Path) -> tuple[np.ndarray, int]:
    audio, rate = sf.read(str(path), always_2d=True)
    assert audio.size > 0, f"{path} decoded to no audio"
    assert np.isfinite(audio).all(), f"{path} decoded audio contains NaN or Inf"
    assert audio.shape[1] == 2, f"{path} decoded channels != 2: {audio.shape[1]}"
    return audio, int(rate)


def decode_with_ffmpeg(source: Path, destination: Path) -> tuple[np.ndarray, int] | None:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        return None
    subprocess.run(
        [ffmpeg, "-y", "-loglevel", "error", "-i", str(source), str(destination)],
        capture_output=True,
        check=True,
    )
    return decode_audio(destination)


CROSS_PLATFORM_TOLERANCES = {
    "peak": 0.025,
    "rms": 0.002,
    "spectralCentroidHz": 50.0,
    "rmsEnvelope": 0.03,
    "centroidEnvelope": 1100.0,
}

CROSS_PLATFORM_EXACT = ("sampleRate", "channels", "frames")

SINE_ONSET_MAX_FRAMES = 86

SINE_ONSET_REFERENCE_RATE = 44100

SINE_PITCH_MAX_CENTS = 0.80

SINE_LEVEL_VELOCITY = 80

SINE_LEVEL_RMS = 0.1751

SINE_LEVEL_RMS_RELATIVE_TOLERANCE = 0.02

DECODED_RATE = {"wav": 44100, "flac": 44100, "ogg": 48000, "mp3": 44100, "aac": 44100}

DECODED_DURATION_TOLERANCE = {
    "wav": 0.001,
    "flac": 0.001,
    "ogg": 0.05,
    "mp3": 0.15,
    "aac": 0.15,
}


class AudioCheckPending(Exception):
    """A required check cannot yet be completed."""


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audio_fixture_sources() -> dict[str, Path]:
    # The edit scenarios in golden.EDIT_SCENARIOS do not export audio.
    sources = golden.fixture_files() + golden.mnx_fixture_files()
    fixtures = {path.name: path for path in sources}
    assert sources and len(fixtures) == len(sources), "empty or duplicate golden fixture inventory"
    assert all(path.is_file() for path in golden.EXTRA_FIXTURES), "extra golden fixture missing"
    edits = set(golden.EDIT_SCENARIOS)
    assert not edits.intersection(fixtures), "fixture and edit scenario names overlap"
    indexed = set(json.loads((REPO_ROOT / "tests/data/golden" / "windows" / "index.json").read_text(encoding="utf-8")))
    expected = set(fixtures) | edits
    assert expected == indexed, (
        f"golden source inventory differs: missing {sorted(indexed - expected)}, unindexed {sorted(expected - indexed)}"
    )
    return fixtures


def validate_version_proof(path: Path, platform: str, engine_commit: str) -> None:
    root = ET.parse(path).getroot()
    version = root.findtext("programVersion") or ""
    revision = root.findtext("programRevision") or ""
    written_platform = next(
        (tag.text for tag in root.iter("metaTag") if tag.get("name") == "platform"),
        None,
    )
    assert re.fullmatch(r"5\.0\.\d+", version), f"capture is not 5.0: {version!r}"
    assert revision.lower() == engine_commit[:7].lower(), (
        f"capture engine revision {revision!r} != {engine_commit[:7]!r}"
    )
    expected_platform = {"windows": "Microsoft Windows", "linux": "Linux"}[platform]
    assert written_platform == expected_platform, (written_platform, expected_platform)


def write_capture_manifest(
    target: Path,
    platform: str,
    source_commit: str,
    engine_commit: str,
    binary_sha: str,
    soundfont_sha: str,
    fixtures: dict[str, Path],
) -> None:
    manifest = {
        "schema": 1,
        "platform": platform,
        "host": socket.gethostname(),
        "superproject_commit": source_commit,
        "engine_commit": engine_commit,
        "binary_sha256": binary_sha,
        "soundfont_sha256": soundfont_sha,
        "extractor_sha256": file_sha256(REPO_ROOT / "tests/engrave_testing/golden.py"),
        "capture_script_sha256": file_sha256(Path(__file__)),
        "fixture_helper_sha256": file_sha256(REPO_ROOT / "tests/engrave_testing/composition_fixture.py"),
        "version_proof_sha256": file_sha256(target / "version.mscx"),
        "fixture_sha256": {name: file_sha256(path) for name, path in fixtures.items()},
        "audio_sha256": {name: file_sha256(target / name / "audio.json") for name in fixtures},
    }
    golden.write_json(target / "manifest.json", manifest)


def capture_destination(capture_root: Path, platform: str) -> Path:
    root = capture_root.resolve()
    golden_root = (REPO_ROOT / "tests/data/golden").resolve()
    assert root != golden_root and golden_root not in root.parents, (
        "capture directory must not be inside tests/data/golden"
    )
    destination = root / platform
    assert not destination.is_symlink(), f"capture destination is a symlink: {destination}"
    if destination.exists():
        manifest = destination / "manifest.json"
        assert manifest.is_file(), f"refusing to replace a directory without a capture manifest: {destination}"
        value = json.loads(manifest.read_text(encoding="utf-8"))
        assert value.get("schema") == 1 and value.get("platform") == platform, (
            f"refusing to replace an unrelated capture: {destination}"
        )
    return destination


def read_audio_capture(
    tree: Path,
    platform: str,
    source_commit: str,
    engine_commit: str,
    fixtures: dict[str, Path],
    soundfont_sha: str,
) -> dict[str, Path]:
    if not tree.is_dir():
        raise AudioCheckPending(f"{platform} 5.0 capture missing: {tree}")
    manifest_path = tree / "manifest.json"
    assert manifest_path.is_file(), f"{platform} capture lacks provenance: {manifest_path}"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest.get("schema") == 1 and manifest.get("platform") == platform, manifest_path
    assert isinstance(manifest.get("host"), str) and manifest["host"].strip(), manifest_path
    assert manifest.get("superproject_commit") == source_commit, f"{platform} capture commit is stale"
    assert manifest.get("engine_commit") == engine_commit, f"{platform} capture engine is stale"
    assert re.fullmatch(r"[0-9a-f]{64}", manifest.get("binary_sha256", "")), manifest_path
    assert manifest.get("soundfont_sha256") == soundfont_sha, f"{platform} soundfont differs"
    assert manifest.get("extractor_sha256") == file_sha256(REPO_ROOT / "tests/engrave_testing/golden.py"), (
        f"{platform} feature extractor is stale"
    )
    assert manifest.get("capture_script_sha256") == file_sha256(Path(__file__)), f"{platform} capture script is stale"
    assert manifest.get("fixture_helper_sha256") == file_sha256(
        REPO_ROOT / "tests/engrave_testing/composition_fixture.py"
    ), f"{platform} fixture helper is stale"
    proof = tree / "version.mscx"
    assert proof.is_file() and manifest.get("version_proof_sha256") == file_sha256(proof), (
        f"{platform} version proof missing or changed"
    )
    validate_version_proof(proof, platform, engine_commit)
    expected_sources = {name: file_sha256(path) for name, path in fixtures.items()}
    assert manifest.get("fixture_sha256") == expected_sources, f"{platform} fixture inputs differ"
    paths = {path.parent.name: path for path in tree.glob("*/audio.json")}
    expected = set(fixtures)
    assert set(paths) == expected, (
        f"{platform} audio inventory: missing {sorted(expected - set(paths))}, extra {sorted(set(paths) - expected)}"
    )
    hashes = manifest.get("audio_sha256")
    assert isinstance(hashes, dict) and set(hashes) == expected, f"{platform} manifest inventory differs"
    for name, path in paths.items():
        assert hashes[name] == file_sha256(path), f"{platform} capture changed: {name}/audio.json"
    return paths


def test_cross_platform_audio_features(
    evidence: dict[str, Any],
    capture_root: Path,
    source_commit: str,
    engine_commit: str,
    fixtures: dict[str, Path],
    soundfont_sha: str,
) -> None:
    print("[cross-platform] Comparing current Windows and Linux 5.0 audio captures...")
    captures = {
        platform: read_audio_capture(
            capture_root / platform,
            platform,
            source_commit,
            engine_commit,
            fixtures,
            soundfont_sha,
        )
        for platform in ("windows", "linux")
    }
    windows, linux = captures["windows"], captures["linux"]

    maxima = {key: 0.0 for key in CROSS_PLATFORM_TOLERANCES}
    worst = {key: "" for key in CROSS_PLATFORM_TOLERANCES}
    for name in sorted(windows):
        left = json.loads(windows[name].read_text(encoding="utf-8"))
        right = json.loads(linux[name].read_text(encoding="utf-8"))
        for data in (left, right):
            assert data["channels"] == 2 and data["sampleRate"] in (44100, 48000), name
            assert data["frames"] > 0, name
            assert len(data["peak"]) == len(data["rms"]) == 2, name
            assert len(data["rmsEnvelope"]) == len(data["centroidEnvelope"]) == len(data["nonzeroSamples"]), name
            window_frames = round(data["sampleRate"] * 0.25)
            assert len(data["centroidEnvelope"]) == (data["frames"] + window_frames - 1) // window_frames, name
            assert all(len(window) == 2 for window in data["rmsEnvelope"]), name
            assert all(len(window) == 2 for window in data["nonzeroSamples"]), name
        for key in CROSS_PLATFORM_EXACT:
            assert left[key] == right[key], f"{name}: {key} differs ({left[key]} vs {right[key]})"
        assert len(left["rmsEnvelope"]) == len(right["rmsEnvelope"]), f"{name}: rmsEnvelope lengths differ"
        assert len(left["centroidEnvelope"]) == len(right["centroidEnvelope"]), (
            f"{name}: centroidEnvelope lengths differ"
        )
        for key, tolerance in CROSS_PLATFORM_TOLERANCES.items():
            left_values = left[key]
            right_values = right[key]
            left_numbers = numeric_values(left_values) if isinstance(left_values, list) else [float(left_values)]
            right_numbers = numeric_values(right_values) if isinstance(right_values, list) else [float(right_values)]
            assert len(left_numbers) == len(right_numbers) and left_numbers, f"{name}: invalid {key}"
            assert all(math.isfinite(value) for value in left_numbers + right_numbers), f"{name}: non-finite {key}"
            delta = max(abs(a - b) for a, b in zip(left_numbers, right_numbers))
            if delta > maxima[key]:
                maxima[key] = delta
                worst[key] = name
            assert delta <= tolerance, f"{name}: {key} differs by {delta:.6g} (tolerance {tolerance})"
    evidence["cross_platform_audio"] = {
        "status": "passed",
        "fixtures": len(windows),
        "superproject_commit": source_commit,
        "engine_commit": engine_commit,
        "captures": {platform: str(capture_root / platform) for platform in captures},
        "tolerances": CROSS_PLATFORM_TOLERANCES,
        "max_abs_diff": {key: round(value, 6) for key, value in maxima.items()},
        "worst_fixture": worst,
    }
    summary = ", ".join(f"{key} {value:.6g}" for key, value in maxima.items())
    print(f"  {len(windows)} fixtures agree across Windows/Linux (max deltas: {summary})")


def run_audio_checks(
    checks: list[tuple[str, Callable[[], None]]],
    evidence: dict[str, Any],
) -> int:
    results: dict[str, dict[str, str]] = {}
    for name, check in checks:
        try:
            check()
        except AudioCheckPending as err:
            results[name] = {"status": "pending", "reason": str(err)}
            print(f"  {name}: PENDING: {err}")
        except Exception as err:
            results[name] = {
                "status": "failed",
                "reason": f"{type(err).__name__}: {err}",
            }
            print(f"  {name}: FAILED: {type(err).__name__}: {err}")
            traceback.print_exc(file=sys.stdout)
        else:
            results[name] = {"status": "passed"}

    statuses = {item["status"] for item in results.values()}
    status = "failed" if "failed" in statuses else "pending" if "pending" in statuses else "passed"
    evidence["checks"] = results
    evidence["status"] = status
    print("\nengine_audio check summary:")
    for name, result in results.items():
        detail = f": {result['reason']}" if "reason" in result else ""
        print(f"  {name}: {result['status'].upper()}{detail}")
    print(f"  overall: {status.upper()}")
    return {"passed": 0, "failed": 1, "pending": 2}[status]


def test_audio_check_reporting() -> None:
    """Exercise missing, incomplete, stale, mismatching and complete captures."""
    feature = {
        "sampleRate": 44100,
        "channels": 2,
        "frames": 1,
        "peak": [0.0, 0.0],
        "rms": [0.0, 0.0],
        "spectralCentroidHz": 0.0,
        "rmsEnvelope": [[0.0, 0.0]],
        "centroidEnvelope": [0.0],
        "nonzeroSamples": [[0, 0]],
    }
    fixtures = audio_fixture_sources()
    source_commit, engine_commit, soundfont_sha = "a" * 40, "b" * 40, "c" * 64
    with tempfile.TemporaryDirectory(prefix="musescore-audio-check-") as temp_dir:
        captures = Path(temp_dir)
        for forbidden in (REPO_ROOT / "tests/data/golden", captures / "unrelated"):
            if forbidden == captures / "unrelated":
                (forbidden / "windows").mkdir(parents=True)
            try:
                capture_destination(forbidden, "windows")
            except AssertionError:
                pass
            else:
                raise AssertionError(f"Unsafe capture destination accepted: {forbidden}")

        def prepare(platform: str, version: str = "5.0.0", inputs: dict[str, Path] = fixtures) -> None:
            tree = captures / platform
            tree.mkdir(exist_ok=True)
            host_platform = "Microsoft Windows" if platform == "windows" else "Linux"
            (tree / "version.mscx").write_text(
                f"<museScore><programVersion>{version}</programVersion>"
                f"<programRevision>{engine_commit[:7]}</programRevision>"
                f'<Score><metaTag name="platform">{host_platform}</metaTag></Score></museScore>',
                encoding="utf-8",
            )
            for name in inputs:
                golden.write_json(tree / name / "audio.json", feature)
            write_capture_manifest(
                tree,
                platform,
                source_commit,
                engine_commit,
                "d" * 64,
                soundfont_sha,
                inputs,
            )

        prepare("windows")
        calls: list[str] = []

        def run() -> tuple[int, dict[str, Any]]:
            evidence: dict[str, Any] = {}
            checks = [
                (
                    "cross_platform",
                    lambda: test_cross_platform_audio_features(
                        evidence,
                        captures,
                        source_commit,
                        engine_commit,
                        audio_fixture_sources(),
                        soundfont_sha,
                    ),
                ),
                ("local", lambda: calls.append("local")),
            ]
            with contextlib.redirect_stdout(io.StringIO()):
                code = run_audio_checks(checks, evidence)
            return code, evidence

        code, evidence = run()
        assert code == 2 and evidence["checks"]["cross_platform"]["status"] == "pending", evidence
        assert "linux 5.0 capture missing" in evidence["checks"]["cross_platform"]["reason"], evidence
        assert calls == ["local"], calls

        prepare("linux")
        omitted = next(iter(fixtures))
        for platform in ("windows", "linux"):
            (captures / platform / omitted / "audio.json").unlink()
        code, evidence = run()
        assert code == 1 and evidence["checks"]["cross_platform"]["status"] == "failed", evidence
        assert "inventory" in evidence["checks"]["cross_platform"]["reason"], evidence

        # Both captures and their manifests omit the same fixture. Neither a
        # matching pair nor an incomplete source enumeration may hide it.
        omitted = "custom_part_name.mscx"
        incomplete = {name: path for name, path in fixtures.items() if name != omitted}
        for platform in ("windows", "linux"):
            shutil.rmtree(captures / platform / omitted)
            prepare(platform, inputs=incomplete)
        code, evidence = run()
        assert code == 1 and "fixture inputs differ" in evidence["checks"]["cross_platform"]["reason"], evidence
        sources = [path for path in golden.fixture_files() if path.name != omitted]
        with patch.object(golden, "fixture_files", return_value=sources):
            code, evidence = run()
        assert code == 1 and "golden source inventory differs" in evidence["checks"]["cross_platform"]["reason"], (
            evidence
        )
        assert omitted in evidence["checks"]["cross_platform"]["reason"], evidence

        prepare("windows", version="4.7.6")
        prepare("linux", version="4.7.6")
        # Even a complete matching pair of the retained 4.7.6 data, with newly
        # hashed manifests and its version proof, is not a current capture.
        for platform in ("windows", "linux"):
            for name in fixtures:
                shutil.copyfile(
                    REPO_ROOT / "tests/data/golden" / "windows" / name / "audio.json",
                    captures / platform / name / "audio.json",
                )
            write_capture_manifest(
                captures / platform,
                platform,
                source_commit,
                engine_commit,
                "d" * 64,
                soundfont_sha,
                fixtures,
            )
        code, evidence = run()
        assert code == 1 and "not 5.0" in evidence["checks"]["cross_platform"]["reason"], evidence

        prepare("windows")
        prepare("linux")
        changed = captures / "linux" / "coverage.musicxml" / "audio.json"
        golden.write_json(changed, {**feature, "peak": [0.1, 0.1]})
        code, evidence = run()
        assert code == 1 and "capture changed" in evidence["checks"]["cross_platform"]["reason"], evidence

        write_capture_manifest(
            captures / "linux",
            "linux",
            source_commit,
            engine_commit,
            "d" * 64,
            soundfont_sha,
            fixtures,
        )
        code, evidence = run()
        assert code == 1 and "peak differs" in evidence["checks"]["cross_platform"]["reason"], evidence

        prepare("linux")
        code, evidence = run()
        assert code == 0 and evidence["checks"]["cross_platform"]["status"] == "passed", evidence
        assert evidence["cross_platform_audio"]["fixtures"] == len(fixtures), evidence
        assert len(calls) == 8, calls
    print(
        f"  Audio capture regressions PASSED: missing, incomplete, missing source, 4.7.6, tampered, "
        f"mismatching and {len(fixtures)}-fixture matching captures"
    )


def check_sine_onset(onset_idx: int, expected_t: float, sr: int, midi_pitch: int) -> float:
    offset_frames = abs(onset_idx - expected_t * sr)
    diff_ms = offset_frames * 1000.0 / sr
    assert offset_frames * SINE_ONSET_REFERENCE_RATE <= SINE_ONSET_MAX_FRAMES * sr, (
        f"Pitch {midi_pitch} onset error {diff_ms:.9f} ms exceeds "
        f"{SINE_ONSET_MAX_FRAMES}/{SINE_ONSET_REFERENCE_RATE} s "
        f"(detected {onset_idx / sr:.6f}s, expected {expected_t:.6f}s)"
    )
    return diff_ms


def check_sine_pitch(cents_diff: float, midi_pitch: int, detected_freq: float, expected_freq: float) -> None:
    assert cents_diff <= SINE_PITCH_MAX_CENTS, (
        f"Pitch {midi_pitch} frequency error {cents_diff:.9f} cents exceeds "
        f"{SINE_PITCH_MAX_CENTS:.2f} cent tolerance "
        f"(detected {detected_freq:.3f} Hz, expected {expected_freq:.3f} Hz)"
    )


def test_sine_threshold_mutations() -> None:
    # The reference onset and the exact pitch limit must pass; the old 5 ms / 1 cent
    # limits would also have accepted the two mutations, which must now fail.
    check_sine_onset(86, 0.0, 44100, 64)
    check_sine_pitch(0.80, 60, 261.5, 261.6)
    mutations = (
        ("88-sample onset", lambda: check_sine_onset(88, 0.0, 44100, 64)),
        ("0.90-cent pitch", lambda: check_sine_pitch(0.90, 60, 261.5, 261.6)),
    )
    for name, mutation in mutations:
        try:
            mutation()
        except AssertionError:
            pass
        else:
            raise AssertionError(f"Sine threshold mutation was not rejected: {name}")
    print("  Sine threshold mutations PASSED: 88-sample onset and 0.90-cent pitch rejected")


def test_pitch_onset_and_level(
    probe_bin: Path,
    assets: Path,
    out_dir: Path,
    evidence: dict[str, Any],
) -> None:
    print("[2/6] Testing pitch, onset and sine SoundFont output level...")
    sine_sf2 = REPO_ROOT / "tests/data/engine_audio" / "sine.sf2"
    if not sine_sf2.is_file():
        generate_sine_soundfont(sine_sf2)

    score_path = REPO_ROOT / "tests/data/engine_audio" / "pitch_test.musicxml"
    if not score_path.is_file():
        raise FileNotFoundError(f"Missing pitch test score: {score_path}")

    out_wav = out_dir / "pitch_test.wav"
    if out_wav.exists():
        out_wav.unlink()

    result = run_probe(probe_bin, assets, sine_sf2, score_path, out_wav)
    assert result.get("ok") is True, f"Probe render failed: {result}"
    assert out_wav.is_file() and out_wav.stat().st_size > 0, "Rendered wav is missing or empty"

    audio, sr = decode_audio(out_wav)
    mono = audio.mean(axis=1)

    expected_notes = [
        (0.0, 60),  # C4
        (1.0, 64),  # E4
        (2.0, 67),  # G4
        (3.0, 69),  # A4 = 440 Hz
    ]

    onset_results = []
    pitch_results = []

    for expected_t, midi_pitch in expected_notes:
        search_start = max(0, int((expected_t - 0.05) * sr))
        search_end = min(len(mono), int((expected_t + 0.10) * sr))
        onset_idx = None
        for i in range(search_start, search_end):
            if abs(mono[i]) > 0.05:
                onset_idx = i
                break
        assert onset_idx is not None, f"Onset not detected for pitch {midi_pitch} around t={expected_t}s"
        detected_t = onset_idx / sr
        diff_ms = check_sine_onset(onset_idx, expected_t, sr, midi_pitch)
        onset_results.append(
            {
                "pitch": midi_pitch,
                "expected_sec": expected_t,
                "detected_sec": detected_t,
                "onset_frame": onset_idx,
                "sample_rate": sr,
                "diff_ms": diff_ms,
            }
        )

        # Frequency analysis in steady state [expected_t + 0.1, expected_t + 0.4]
        t0 = int((expected_t + 0.10) * sr)
        t1 = int((expected_t + 0.40) * sr)
        segment = mono[t0:t1]
        win = np.hanning(len(segment))
        spec = np.abs(np.fft.rfft(segment * win))
        freqs = np.fft.rfftfreq(len(segment), 1.0 / sr)
        k = int(np.argmax(spec))
        if 0 < k < len(spec) - 1:
            alpha = spec[k - 1]
            beta = spec[k]
            gamma = spec[k + 1]
            p = 0.5 * (alpha - gamma) / (alpha - 2.0 * beta + gamma)
            detected_freq = freqs[k] + p * (freqs[1] - freqs[0])
        else:
            detected_freq = freqs[k]

        expected_freq = 440.0 * (2.0 ** ((midi_pitch - 69) / 12.0))
        cents_diff = float(abs(1200.0 * np.log2(detected_freq / expected_freq)))
        check_sine_pitch(cents_diff, midi_pitch, detected_freq, expected_freq)
        pitch_results.append(
            {
                "pitch": midi_pitch,
                "expected_hz": expected_freq,
                "detected_hz": float(detected_freq),
                "cents_diff": cents_diff,
            }
        )

    # Output level: one explicit mf note at C4. The captured events must show
    # the documented dynamic mapping, then the steady-state RMS must match.
    level_score = REPO_ROOT / "tests/data/engine_audio" / "level_test.musicxml"
    assert level_score.is_file(), f"Missing level test score: {level_score}"
    level_wav = out_dir / "level_test.wav"
    if level_wav.exists():
        level_wav.unlink()
    level_result = run_probe(probe_bin, assets, sine_sf2, level_score, level_wav, ["--midi-events"])
    assert level_result.get("ok") is True, f"Level probe render failed: {level_result}"

    level_events = level_result.get("midiEvents", [])
    note_ons = [event for event in level_events if event.get("opcode") == "Opcode::NoteOn"]
    assert len(note_ons) == 1, f"Expected exactly one NoteOn in level_test, got {note_ons}"
    assert note_ons[0].get("note") == 60, note_ons[0]
    assert note_ons[0].get("velocity") == SINE_LEVEL_VELOCITY, (
        f"mf should map to velocity {SINE_LEVEL_VELOCITY}, got {note_ons[0]}"
    )

    level_audio, level_sr = decode_audio(level_wav)
    level_window = level_audio[int(0.10 * level_sr) : int(0.40 * level_sr)]
    level_rms = float(np.sqrt(np.mean(level_window**2)))
    level_relative = abs(level_rms - SINE_LEVEL_RMS) / SINE_LEVEL_RMS
    assert level_relative <= SINE_LEVEL_RMS_RELATIVE_TOLERANCE, (
        f"Sine mf RMS {level_rms:.5f} differs from measured {SINE_LEVEL_RMS:.5f} by "
        f"{level_relative * 100:.2f}% (tolerance {SINE_LEVEL_RMS_RELATIVE_TOLERANCE * 100:.1f}%)"
    )

    evidence["pitch_and_onset"] = {
        "status": "passed",
        "onset_limit_ms": SINE_ONSET_MAX_FRAMES * 1000.0 / SINE_ONSET_REFERENCE_RATE,
        "pitch_limit_cents": SINE_PITCH_MAX_CENTS,
        "onsets": onset_results,
        "pitches": pitch_results,
    }
    evidence["sine_level"] = {
        "status": "passed",
        "dynamic": "mf",
        "velocity": SINE_LEVEL_VELOCITY,
        "measured_rms": round(level_rms, 5),
        "expected_rms": SINE_LEVEL_RMS,
        "relative_tolerance": SINE_LEVEL_RMS_RELATIVE_TOLERANCE,
    }
    print(
        f"  Pitch and onset tests PASSED: max onset diff = "
        f"{max(r['diff_ms'] for r in onset_results):.9f} ms "
        f"(<= {SINE_ONSET_MAX_FRAMES}/{SINE_ONSET_REFERENCE_RATE} s), max pitch diff = "
        f"{max(r['cents_diff'] for r in pitch_results):.9f} cents (<= {SINE_PITCH_MAX_CENTS:.2f} cent)"
    )
    print(
        f"  Sine level PASSED: mf RMS {level_rms:.5f} vs {SINE_LEVEL_RMS:.5f} "
        f"({level_relative * 100:.2f}% <= {SINE_LEVEL_RMS_RELATIVE_TOLERANCE * 100:.1f}%)"
    )


def midi_fixtures() -> list[tuple[str, Path]]:
    return [
        ("evening", REPO_ROOT / "tests/data/inputs" / "evening.musicxml"),
        (
            "hairpins_and_repeats",
            REPO_ROOT / "tests/data/inputs" / "golden" / "hairpins_and_repeats.mscx",
        ),
        ("pedal", REPO_ROOT / "tests/data/inputs" / "golden" / "pedal.mscx"),
        ("bend.gp", REPO_ROOT / "tests/data/inputs" / "golden" / "bend.gp"),
        ("repeat70", REPO_ROOT / "tests/data/inputs" / "golden" / "repeat70.mscx"),
    ]


def midi_golden_file(name: str, golden_dir: Path) -> Path:
    platform_file = golden_dir / golden_platform() / f"{name}.json"
    if platform_file.is_file():
        return platform_file
    return golden_dir / f"{name}.json"


CC_EXPRESSION = 11

NATURAL_EXPRESSION_LEVEL = 64

SND_FALLBACK_VELOCITY = 64

MIDI_ACCEPTED_RULES: dict[str, dict[str, Any]] = {
    "evening": {
        "reference_sha256": "1e81e3a06b59c6505be90e4003dea2eaa92bd5c42248d6990985a8cc60dc2088",
        # 4.7.6: 313 events (140 NoteOn, 140 NoteOff, 3 PitchBend, 24 CC11, 3 CC64,
        # 3 CC123); 5.0: 295 (24 -> 6 CC11, 76 SND NoteOn velocities -> 64).
        "reference_event_total": 313,
        "expected_event_total": 295,
        "expected_class_totals": {
            "NoteOn": 140,
            "NoteOff": 140,
            "PitchBend": 3,
            "CC11": 6,
            "CC64": 3,
            "CC123": 3,
        },
        "snd_channels": (0, 2),
        "no_dynamics_curve_channels": (),
        "cc11_duplicate_collapse": 18,
        "cc11_natural_no_curve": 0,
        "noteon_velocity_changes": 76,
        "noteon_velocity_remap": {(59, 64): 2, (62, 64): 37, (65, 64): 37},
    },
    "hairpins_and_repeats": {
        "reference_sha256": "ad51523b9ed0b349bb86fe9809bae17be6e9e588124d920d85c851ad53941001",
        "reference_event_total": 35,
        "expected_event_total": 35,
        "expected_class_totals": {
            "NoteOn": 16,
            "NoteOff": 16,
            "PitchBend": 1,
            "CC64": 1,
            "CC123": 1,
        },
        "snd_channels": (),
        "no_dynamics_curve_channels": (),
        "cc11_duplicate_collapse": 0,
        "cc11_natural_no_curve": 0,
        "noteon_velocity_changes": 0,
        "noteon_velocity_remap": {},
    },
    "pedal": {
        "reference_sha256": "cd932ed9f7b909d8eb404d48d8eee338df76cb247871cb6b29ccdbea4d40bbd4",
        # 4.7.6: 55 events (18 NoteOn, 18 NoteOff, 1 PitchBend, 8 CC11, 9 CC64,
        # 1 CC123); 5.0: 47 (the 8 natural-level CC11 events of the no-curve piano
        # track are out of band). Velocities and every other event are unchanged.
        "reference_event_total": 55,
        "expected_event_total": 47,
        "expected_class_totals": {
            "NoteOn": 18,
            "NoteOff": 18,
            "PitchBend": 1,
            "CC64": 9,
            "CC123": 1,
        },
        "snd_channels": (),
        "no_dynamics_curve_channels": (1,),
        "cc11_duplicate_collapse": 0,
        "cc11_natural_no_curve": 8,
        "noteon_velocity_changes": 0,
        "noteon_velocity_remap": {},
    },
    "bend.gp": {
        "reference_sha256": "12eb4328d9941b9fe04ab265b6ca351dab147fa27abbf279561e1b1c7cfa43d8",
        "reference_event_total": 373,
        "expected_event_total": 373,
        "expected_class_totals": {
            "NoteOn": 6,
            "NoteOff": 6,
            "PitchBend": 359,
            "CC64": 1,
            "CC123": 1,
        },
        "snd_channels": (),
        "no_dynamics_curve_channels": (),
        "cc11_duplicate_collapse": 0,
        "cc11_natural_no_curve": 0,
        "noteon_velocity_changes": 0,
        "noteon_velocity_remap": {},
    },
    "repeat70": {
        "reference_sha256": "20e63965d78db755fff65e1683c7306e81ae9f801e0d307f44bdfd8ded2f1447",
        "reference_event_total": 15,
        "expected_event_total": 15,
        "expected_class_totals": {
            "NoteOn": 6,
            "NoteOff": 6,
            "PitchBend": 1,
            "CC64": 1,
            "CC123": 1,
        },
        "snd_channels": (),
        "no_dynamics_curve_channels": (),
        "cc11_duplicate_collapse": 0,
        "cc11_natural_no_curve": 0,
        "noteon_velocity_changes": 0,
        "noteon_velocity_remap": {},
    },
}


def midi_event_class(event: dict[str, Any]) -> str:
    opcode = str(event["opcode"]).rsplit("::", 1)[-1]
    if opcode == "ControlChange":
        return f"CC{event['index']}"
    return opcode


def midi_class_totals(events: list[dict[str, Any]]) -> dict[str, int]:
    return dict(sorted(Counter(midi_event_class(event) for event in events).items()))


def predict_50_midi_events(
    name: str, reference_events: list[dict[str, Any]]
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    """Derive the expected 5.0 stream from the 4.7.6 reference (rules above)."""
    rule = MIDI_ACCEPTED_RULES[name]
    seen_cc11: dict[tuple[Any, Any, Any], dict[str, Any]] = {}
    predicted: list[dict[str, Any]] = []
    duplicates = 0
    natural_level = 0
    velocity_remap: Counter[tuple[Any, Any]] = Counter()

    for reference_event in reference_events:
        event = dict(reference_event)
        event_class = midi_event_class(event)
        if event_class == f"CC{CC_EXPRESSION}":
            if event["channel"] in rule["no_dynamics_curve_channels"]:
                assert event["frame"] == 0 and event["data"] == NATURAL_EXPRESSION_LEVEL, (
                    f"{name}: pinned no-dynamics channel {event['channel']} has a non-natural "
                    f"CC11 event {reference_event}; the accepted rule only covers the initial "
                    "natural level, so this fixture needs a new reviewed rule"
                )
                natural_level += 1
                continue
            key = (event["frame"], event["channel"], event["data"])
            if key in seen_cc11:
                assert event == seen_cc11[key], (
                    f"{name}: CC11 events at {key} differ in other fields; only exact duplicates may be collapsed"
                )
                duplicates += 1
                continue
            seen_cc11[key] = event
        elif event_class == "NoteOn" and event["channel"] in rule["snd_channels"]:
            if event["velocity"] != SND_FALLBACK_VELOCITY:
                velocity_remap[(event["velocity"], SND_FALLBACK_VELOCITY)] += 1
                event["velocity"] = SND_FALLBACK_VELOCITY
        predicted.append(event)

    stats = {
        "cc11_duplicate_collapse": duplicates,
        "cc11_natural_no_curve": natural_level,
        "noteon_velocity_changes": sum(velocity_remap.values()),
        "noteon_velocity_remap": dict(velocity_remap),
    }
    return predicted, stats


def describe_event_difference(actual: list[dict[str, Any]], expected: list[dict[str, Any]]) -> str:
    for index, (actual_event, expected_event) in enumerate(zip(actual, expected)):
        if actual_event != expected_event:
            return f"event {index} is {actual_event}, expected {expected_event}"
    if len(actual) > len(expected):
        return f"unexpected extra event {len(expected)}: {actual[len(expected)]}"
    if len(actual) < len(expected):
        return f"missing event {len(actual)}: {expected[len(actual)]}"
    return "no difference"


def check_midi_stream_against_reference(
    name: str,
    golden: dict[str, Any],
    actual_events: list[dict[str, Any]],
    actual_total: Any,
    truncated: Any,
) -> dict[str, Any]:
    """Assert the 5.0 stream is the 4.7.6 reference plus only the pinned differences."""
    rule = MIDI_ACCEPTED_RULES.get(name)
    assert rule is not None, (
        f"No documented 5.0 accepted-difference rule for MIDI fixture {name}; add a reviewed "
        "entry to MIDI_ACCEPTED_RULES instead of re-baselining the 4.7.6 reference"
    )
    reference_events = golden["events"]
    assert golden["eventTotal"] == len(reference_events) == rule["reference_event_total"], (
        f"MIDI golden for {name} has eventTotal {golden['eventTotal']} and "
        f"{len(reference_events)} stored events, but the audio baseline pins the 4.7.6 reference at "
        f"{rule['reference_event_total']}. Do not re-baseline this file on 5.0 output; "
        "document accepted 5.0 changes in MIDI_ACCEPTED_RULES with an upstream cause"
    )

    predicted_events, stats = predict_50_midi_events(name, reference_events)
    for key in (
        "cc11_duplicate_collapse",
        "cc11_natural_no_curve",
        "noteon_velocity_changes",
    ):
        assert stats[key] == rule[key], (
            f"{name}: accepted-difference count {key} is {stats[key]}, pinned {rule[key]}; "
            "an unexpected playback difference must be reviewed against its upstream cause "
            "before the pin is changed"
        )
    assert stats["noteon_velocity_remap"] == rule["noteon_velocity_remap"], (
        f"{name}: SND velocity remap is {stats['noteon_velocity_remap']}, pinned {rule['noteon_velocity_remap']}"
    )

    expected_total = len(predicted_events)
    assert expected_total == rule["expected_event_total"], (
        f"{name}: accepted rules predict {expected_total} events, pinned {rule['expected_event_total']}"
    )
    class_totals = midi_class_totals(predicted_events)
    assert class_totals == rule["expected_class_totals"], (
        f"{name}: expected per-class totals {class_totals}, pinned {rule['expected_class_totals']}"
    )

    assert isinstance(actual_total, int), f"Probe did not report midiEventTotal for {name}"
    assert actual_total > 0, f"MIDI golden for {name} has no events"
    assert actual_total == expected_total, (
        f"MIDI event total mismatch for {name}: got {actual_total}, expected {expected_total} "
        f"(4.7.6 reference {len(reference_events)} minus the pinned accepted differences)"
    )
    assert truncated is False, f"Probe truncated {name} events"
    assert actual_total == len(actual_events), (
        f"MIDI event count mismatch for {name}: total {actual_total}, printed {len(actual_events)}"
    )
    if actual_events != predicted_events:
        raise AssertionError(
            f"MIDI events (including render-frame offsets) mismatch for {name}: "
            f"{describe_event_difference(actual_events, predicted_events)}. Only the documented "
            "5.0 CC11 and SND-velocity differences are accepted; every other field must match "
            "the 4.7.6 reference exactly"
        )

    return {
        "event_total": actual_total,
        "reference_event_total": len(reference_events),
        "class_totals": class_totals,
        "accepted_differences": {
            "cc11_duplicate_collapse": stats["cc11_duplicate_collapse"],
            "cc11_natural_no_curve": stats["cc11_natural_no_curve"],
            "noteon_velocity_changes": stats["noteon_velocity_changes"],
            "noteon_velocity_remap": {
                f"{old}->{new}": count for (old, new), count in sorted(stats["noteon_velocity_remap"].items())
            },
        },
        "matches_reference_with_accepted_differences": True,
    }


def midi_oracle_negative_controls(name: str, golden: dict[str, Any], actual_events: list[dict[str, Any]]) -> list[str]:
    """Exercise the same comparison used by the lane with deliberately bad streams."""
    rejected = []

    def reject(label: str, mutated: list[dict[str, Any]]) -> None:
        try:
            check_midi_stream_against_reference(name, golden, mutated, len(mutated), False)
        except AssertionError:
            rejected.append(label)
        else:
            raise AssertionError(f"MIDI oracle for {name} accepted mutation {label}")

    for event_class, field in (
        ("NoteOn", "frame"),
        ("NoteOn", "note"),
        ("NoteOn", "velocity"),
        ("NoteOff", "frame"),
        ("PitchBend", "pitchBend"),
        ("CC64", "data"),
    ):
        index = next(
            (i for i, event in enumerate(actual_events) if midi_event_class(event) == event_class),
            None,
        )
        if index is None:
            continue
        mutated = [dict(event) for event in actual_events]
        mutated[index][field] += 1
        reject(f"{event_class}.{field}+1", mutated)

    # In evening the oracle allows exactly 18 duplicate CC11 removals. Remove one
    # further surviving CC11 from a formerly duplicated triple (19 removals), even
    # though it is in the accepted event class: the pinned count must reject it.
    if MIDI_ACCEPTED_RULES[name]["cc11_duplicate_collapse"]:
        reference_cc11 = Counter(
            (event["frame"], event["channel"], event["data"])
            for event in golden["events"]
            if midi_event_class(event) == "CC11"
        )
        index = next(
            i
            for i, event in enumerate(actual_events)
            if midi_event_class(event) == "CC11"
            and reference_cc11[(event["frame"], event["channel"], event["data"])] > 1
        )
        mutated = [dict(event) for event in actual_events]
        mutated.pop(index)
        reject("CC11.extra_removal_beyond_pinned_count", mutated)
    return rejected


def test_midi_event_goldens(
    probe_bin: Path,
    assets: Path,
    soundfont: Path,
    out_dir: Path,
    evidence: dict[str, Any],
) -> None:
    print("[3/6] Testing the 4.7.6 MIDI event reference plus the accepted 5.0 differences...")
    fixtures = midi_fixtures()
    golden_dir = REPO_ROOT / "tests/data/engine_audio" / "midi"
    golden_results = {}

    for name, fix_path in fixtures:
        assert fix_path.is_file(), f"Fixture missing: {fix_path}"
        golden_file = midi_golden_file(name, golden_dir)
        assert golden_file.is_file(), f"MIDI golden file missing: {golden_file}"

        tmp_wav = out_dir / f"midi_{name}.wav"
        if tmp_wav.exists():
            tmp_wav.unlink()

        res = run_probe(probe_bin, assets, soundfont, fix_path, tmp_wav, ["--midi-events"])
        if tmp_wav.exists():
            tmp_wav.unlink()

        golden = json.loads(golden_file.read_text(encoding="utf-8"))
        assert res.get("ok") is True, f"Probe failed for {name}: {res}"
        assert isinstance(golden, dict) and "events" in golden and "eventTotal" in golden, (
            f"{golden_file} is not a 4.7.6 reference MIDI golden with an exact total"
        )
        assert golden.get("fixture") == name, (
            f"MIDI golden {golden_file} is for fixture {golden.get('fixture')!r}, not {name!r}"
        )
        reference = json.dumps(
            {"eventTotal": golden["eventTotal"], "events": golden["events"]},
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
        reference_sha256 = hashlib.sha256(reference).hexdigest()
        assert reference_sha256 == MIDI_ACCEPTED_RULES[name]["reference_sha256"], (
            f"MIDI reference {golden_file} has event SHA-256 {reference_sha256}; expected "
            f"the pinned 4.7.6 events {MIDI_ACCEPTED_RULES[name]['reference_sha256']}"
        )
        events = res.get("midiEvents", [])
        result = check_midi_stream_against_reference(
            name,
            golden,
            events,
            res.get("midiEventTotal"),
            res.get("midiEventsTruncated"),
        )
        result["golden"] = str(golden_file.relative_to(REPO_ROOT))
        result["reference_sha256"] = reference_sha256
        result["negative_controls_rejected"] = midi_oracle_negative_controls(name, golden, events)
        golden_results[name] = result

    evidence["midi_goldens"] = {
        "status": "passed",
        "reference": "4.7.6 events (commit 9a74eb1), not re-baselined for 5.0",
        "accepted_classes": [
            "CC11 duplicates collapsed (a1)",
            "natural-level CC11 on tracks without a dynamics curve removed (a2)",
            "SND NoteOn velocity fallback 64 (b)",
        ],
        "fixtures": golden_results,
    }
    reference_total = sum(item["reference_event_total"] for item in golden_results.values())
    total = sum(item["event_total"] for item in golden_results.values())
    accepted = {
        key: sum(item["accepted_differences"][key] for item in golden_results.values())
        for key in (
            "cc11_duplicate_collapse",
            "cc11_natural_no_curve",
            "noteon_velocity_changes",
        )
    }
    controls = sum(len(item["negative_controls_rejected"]) for item in golden_results.values())
    print(
        f"  MIDI reference PASSED across {len(fixtures)} fixtures: 4.7.6 {reference_total} "
        f"events -> 5.0 {total} events, accepted differences exactly "
        f"{accepted['cc11_duplicate_collapse']} duplicate CC11 + "
        f"{accepted['cc11_natural_no_curve']} natural-level CC11 + "
        f"{accepted['noteon_velocity_changes']} SND velocities; all other fields exact; "
        f"{controls} deliberately mutated events rejected"
    )
    print("  evening negative controls rejected: " + ", ".join(golden_results["evening"]["negative_controls_rejected"]))


def test_determinism_and_decoding(
    probe_bin: Path,
    assets: Path,
    soundfont: Path,
    out_dir: Path,
    evidence: dict[str, Any],
) -> None:
    print("[4/6] Testing determinism, decoded durations and non-ASCII paths across wav, flac, ogg, mp3, aac...")
    score_path = REPO_ROOT / "tests/data/inputs" / "golden" / "hairpins_and_repeats.mscx"
    formats = ["wav", "flac", "ogg", "mp3", "aac"]
    det_results = {}

    for fmt in formats:
        out1 = out_dir / f"det_run1.{fmt}"
        out2 = out_dir / f"det_run2.{fmt}"
        for out in (out1, out2):
            if out.exists():
                out.unlink()

        res1 = run_probe(probe_bin, assets, soundfont, score_path, out1)
        res2 = run_probe(probe_bin, assets, soundfont, score_path, out2)

        data1 = out1.read_bytes()
        data2 = out2.read_bytes()
        assert data1 == data2, f"Format {fmt} repeated exports are not byte-identical!"
        assert res1.get("frames") == res2.get("frames"), f"Format {fmt} frame counts differ"

        expected_dur = res1.get("playTime", 0.0) + 3.0
        dec_info: dict[str, Any] = {
            "byte_identical": True,
            "size": len(data1),
            "sha256": hashlib.sha256(data1).hexdigest(),
            "expected_duration": round(expected_dur, 6),
        }

        if fmt == "aac":
            dec_wav = out_dir / "aac_decoded.wav"
            decoded = decode_with_ffmpeg(out1, dec_wav)
            if dec_wav.exists():
                dec_wav.unlink()
            if decoded is None:
                # ADTS syncword check: 12 bits 0xFFF (0xFF followed by 0xF0..0xFF)
                assert len(data1) > 7, "AAC file too small for ADTS header"
                assert data1[0] == 0xFF and (data1[1] & 0xF0) == 0xF0, "Invalid ADTS header in AAC export"
                dec_info["decoder"] = "adts_header_validated"
                det_results[fmt] = dec_info
                continue
            audio, sr = decoded
            dec_info["decoder"] = "ffmpeg"
        else:
            audio, sr = decode_audio(out1)

        dec_info["decoded_sr"] = sr
        assert sr == DECODED_RATE[fmt], f"Format {fmt} decoded sample rate {sr} != {DECODED_RATE[fmt]}"
        decoded_dur = len(audio) / sr
        tolerance = DECODED_DURATION_TOLERANCE[fmt]
        assert abs(decoded_dur - expected_dur) <= tolerance, (
            f"Decoded {fmt} duration {decoded_dur:.6f} != play time {res1.get('playTime')} + 3 s "
            f"(tolerance {tolerance} s)"
        )
        if fmt in ("wav", "flac"):
            assert len(audio) == res1.get("frames"), (
                f"Decoded {fmt} frames {len(audio)} != probe frames {res1.get('frames')}"
            )
        dec_info["decoded_frames"] = len(audio)
        dec_info["decoded_duration"] = round(decoded_dur, 6)
        dec_info["duration_tolerance"] = tolerance
        det_results[fmt] = dec_info

    # Non-ASCII SoundFont copy and output paths for every audio format.
    sine_sf2 = REPO_ROOT / "tests/data/engine_audio" / "sine.sf2"
    if not sine_sf2.is_file():
        generate_sine_soundfont(sine_sf2)
    nonascii_sf = out_dir / "非ASCII 音色" / "正弦 音色.sf2"
    nonascii_sf.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(sine_sf2, nonascii_sf)
    pitch_score = REPO_ROOT / "tests/data/engine_audio" / "pitch_test.musicxml"
    nonascii_dir = out_dir / "输出 音频"
    nonascii_dir.mkdir(parents=True, exist_ok=True)
    nonascii_results = {}
    for fmt in formats:
        out_path = nonascii_dir / f"输出 音频.{fmt}"
        if out_path.exists():
            out_path.unlink()
        res = run_probe(probe_bin, assets, nonascii_sf, pitch_score, out_path)
        assert res.get("ok") is True, f"Non-ASCII {fmt} export failed: {res}"
        assert res.get("soundfont", {}).get("uriRoundTrip") is True, (
            f"Non-ASCII SoundFont URI did not round-trip for {fmt}: {res.get('soundfont')}"
        )
        assert out_path.is_file() and out_path.stat().st_size > 0, f"Missing non-ASCII {fmt} output"
        expected_dur = res.get("playTime", 0.0) + 3.0
        if fmt == "aac":
            dec_wav = out_dir / "nonascii_aac_decoded.wav"
            decoded = decode_with_ffmpeg(out_path, dec_wav)
            if dec_wav.exists():
                dec_wav.unlink()
            if decoded is None:
                data = out_path.read_bytes()
                assert len(data) > 7 and data[0] == 0xFF and (data[1] & 0xF0) == 0xF0
                nonascii_results[fmt] = {
                    "bytes": len(data),
                    "decoder": "adts_header_validated",
                }
                continue
            audio, sr = decoded
        else:
            audio, sr = decode_audio(out_path)
        decoded_dur = len(audio) / sr
        assert abs(decoded_dur - expected_dur) <= DECODED_DURATION_TOLERANCE[fmt], (
            f"Non-ASCII {fmt} decoded duration {decoded_dur:.6f} != {expected_dur:.6f}"
        )
        nonascii_results[fmt] = {
            "bytes": out_path.stat().st_size,
            "decoded_sr": sr,
            "decoded_duration": round(decoded_dur, 6),
            "soundfont_uri_round_trip": True,
        }

    evidence["determinism_and_codecs"] = {
        "status": "passed",
        "formats": det_results,
    }
    evidence["non_ascii_paths"] = {
        "status": "passed",
        "soundfont": str(nonascii_sf.relative_to(out_dir)),
        "soundfont_uri_round_trip": True,
        "formats": nonascii_results,
    }
    print(f"  Determinism and decoded-duration checks PASSED for {', '.join(formats)}")
    print(f"  Non-ASCII SoundFont and output paths PASSED for {', '.join(formats)}")


def test_excerpt_rendering_parity(
    probe_bin: Path,
    assets: Path,
    soundfont: Path,
    out_dir: Path,
    evidence: dict[str, Any],
) -> None:
    print("[6/6] Testing excerpt render identity against master with other parts muted...")
    score_path = REPO_ROOT / "tests/data/inputs" / "evening.musicxml"
    out_exc = out_dir / "evening_excerpt0.wav"
    out_muted = out_dir / "evening_master_muted12.wav"

    for out in (out_exc, out_muted):
        if out.exists():
            out.unlink()

    # Excerpt 0 in evening.musicxml corresponds to part 0; parts 1 and 2 are absent/invisible.
    # The engine implements excerpt rendering by muting those parts on the master score
    # (masternotation.cpp:748-770), so this assertion proves the muting path is applied
    # consistently; it is not an independent excerpt renderer.
    run_probe(probe_bin, assets, soundfont, score_path, out_exc, ["--excerpt", "0"])
    run_probe(
        probe_bin,
        assets,
        soundfont,
        score_path,
        out_muted,
        ["--mute-part", "1", "--mute-part", "2"],
    )

    audio_exc, sr_exc = decode_audio(out_exc)
    audio_muted, sr_muted = decode_audio(out_muted)

    assert sr_exc == sr_muted, f"Sample rates differ: {sr_exc} vs {sr_muted}"
    assert audio_exc.shape == audio_muted.shape, f"Shapes differ: {audio_exc.shape} vs {audio_muted.shape}"

    max_diff = float(np.max(np.abs(audio_exc - audio_muted)))
    assert max_diff == 0.0, f"Excerpt 0 audio differs from muted master render! Max diff = {max_diff}"

    for out in (out_exc, out_muted):
        if out.exists():
            out.unlink()

    evidence["excerpt_parity"] = {
        "status": "passed",
        "max_sample_difference": max_diff,
        "identity_by_construction": True,
    }
    print(
        f"  Excerpt render sample identity PASSED: max diff = {max_diff} "
        "(identity by construction: excerpt = master with other parts muted)"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Standalone engine audio checks")
    parser.add_argument("--engine-only", action="store_true", help="accepted for host wrapper compatibility")
    parser.add_argument("--bin-dir", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--soundfont", type=Path, required=True)
    parser.add_argument("--engine-source", type=Path, default=REPO_ROOT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    probe_bin = find_probe_binary(args.bin_dir)
    out_dir = args.output.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    evidence = {}
    checks = [
        ("reporting_regressions", test_audio_check_reporting),
        ("sine_threshold_mutations", test_sine_threshold_mutations),
        ("pitch_onset_level", lambda: test_pitch_onset_and_level(probe_bin, args.resources, out_dir, evidence)),
        ("midi_goldens", lambda: test_midi_event_goldens(probe_bin, args.resources, args.soundfont, out_dir, evidence)),
        (
            "determinism_codecs",
            lambda: test_determinism_and_decoding(probe_bin, args.resources, args.soundfont, out_dir, evidence),
        ),
        (
            "excerpt_parity",
            lambda: test_excerpt_rendering_parity(probe_bin, args.resources, args.soundfont, out_dir, evidence),
        ),
    ]
    code = run_audio_checks(checks, evidence)
    (out_dir / "evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    return code


if __name__ == "__main__":
    sys.exit(main())
