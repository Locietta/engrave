"""Playback dump capture, exact comparison and fresh-process measurements."""

import hashlib
import json
import math
import os
import re
import struct
import subprocess

from engrave_testing.cancel_score import generate_cancel_score
from engrave_testing.playback_corpus import FOCUSED, corpus


def pinned_cases():
    return json.loads((FOCUSED / "corpus.json").read_text(encoding="utf-8"))


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def pcm_metadata(path):
    with path.open("rb") as f:
        assert f.read(4) == b"RIFF"
        f.read(4)
        assert f.read(4) == b"WAVE"
        fmt = None
        while header := f.read(8):
            tag, size = struct.unpack("<4sI", header)
            if tag == b"fmt ":
                fmt = struct.unpack("<HHIIHH", f.read(size)[:16])
            elif tag == b"data":
                assert fmt and fmt[0] == 3 and fmt[5] == 32, fmt
                digest = hashlib.sha256()
                remaining = size
                while remaining:
                    data = f.read(min(remaining, 1024 * 1024))
                    assert data, "truncated PCM"
                    digest.update(data)
                    remaining -= len(data)
                assert size % fmt[4] == 0
                return dict(
                    sha256=digest.hexdigest(),
                    rate=fmt[2],
                    channels=fmt[1],
                    frames=size // fmt[4],
                    format="FLOAT32",
                    byte_order="little",
                )
            else:
                f.seek(size, 1)
            if size & 1:
                f.seek(1, 1)
    raise ValueError("no PCM data chunk")


def run_probe(exe, assets, font, score, output, options):
    output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, ENGRAVE_AUDIO_TIMING="1")
    with (output / "stdout.log").open("wb") as out, (output / "stderr.log").open("wb") as err:
        command = [
            str(exe.resolve()),
            str(assets.resolve()),
            str(font.resolve()),
            str(score.resolve()),
            str(output.resolve()),
            *options,
        ]
        result = subprocess.run(command, stdout=out, stderr=err, env=env, timeout=1800)
    if result.returncode:
        raise RuntimeError(f"probe failed ({result.returncode}): {output / 'stderr.log'}")


def capture(exe, assets, font, output, match=None, rates=(44100, 48000)):
    output.mkdir(parents=True, exist_ok=False)
    profiles = {p.relative_to(assets).as_posix(): sha(p) for p in sorted(assets.rglob("*.json"))}
    cases = corpus(output / "generated")
    if match is None and {n for n, _, _ in cases} != set(pinned_cases()):
        raise AssertionError("full capture differs from pinned corpus list")
    selected = [(n, p, o) for n, p, o in cases if match is None or match in n]
    assert selected, "no cases selected"
    manifest = {}
    for name, score, options in selected:
        for rate in rates:
            key = name + f"/{rate}"
            target = output / key
            args = ["--rate", str(rate), *options]
            run_probe(exe, assets, font, score, target, args)
            meta = pcm_metadata(target / "audio.wav")
            meta.update(
                options=args,
                soundfont_sha256=sha(font),
                profiles=profiles,
                build=(target / "build.txt").read_text(encoding="utf-8"),
            )
            write_json(target / "D.json", meta)
            (target / "audio.wav").unlink()
            manifest[key] = dict(score_sha256=sha(score), options=args)
            write_json(output / "manifest.json", manifest)
            print(key, flush=True)
    write_json(output / "provenance.json", dict(executable_sha256=sha(exe), executable=str(exe.resolve())))
    write_json(
        output / "complete.json",
        dict(renders=len(selected) * len(rates), schema=1, diagnostic=match is not None, rates=list(rates)),
    )


def validate_dump_records(root, name):
    for dump, required in (("A.txt", {b"track "}), ("C.txt", {b"track ", b"total_frames "})):
        missing = set(required)
        with (root / name / dump).open("rb") as stream:
            for line in stream:
                missing = {
                    prefix for prefix in missing if not (line.startswith(prefix) and line[len(prefix) :].strip())
                }
                if not missing:
                    break
        if missing:
            raise AssertionError(f"{root / name / dump}: structurally incomplete; missing {sorted(missing)!r}")


PROJECTION_HEADER = b"projection-v1\n"


def projection_present(root, name):
    """True when the capture carries a real dump B (step 4b on); older executables wrote a stub."""
    path = root / name / "B.txt"
    if not path.exists():
        return False
    with path.open("rb") as stream:
        return stream.read(len(PROJECTION_HEADER)) == PROJECTION_HEADER


def validate_projection(root, name):
    """Dump B is an oracle inside one executable: flat records against the independent legacy extractor."""
    flat, legacy = root / name / "B.txt", root / name / "B_legacy.txt"
    for path in (flat, legacy):
        if not path.exists():
            raise AssertionError(f"{path}: dump B is missing")
        with path.open("rb") as stream:
            head = stream.read(len(PROJECTION_HEADER))
            if head != PROJECTION_HEADER:
                raise AssertionError(f"{path}: dump B is deferred or not a projection ({head!r})")
            if not any(line.startswith(b"track ") and line[6:].strip() for line in stream):
                raise AssertionError(f"{path}: structurally incomplete; missing [b'track ']")
    compare_bytes(legacy, flat, f"{name}/B.txt (flat records vs legacy extractor)")


def compare_bytes(a, b, name):
    offset = 0
    with a.open("rb") as af, b.open("rb") as bf:
        while True:
            av, bv = af.read(1024 * 1024), bf.read(1024 * 1024)
            if av != bv:
                at = next((i for i, (x, y) in enumerate(zip(av, bv)) if x != y), min(len(av), len(bv)))
                offset += at
                start = max(0, offset - 24)
                af.seek(start)
                bf.seek(start)
                raise AssertionError(
                    f"{name}: first byte difference at offset {offset}\n< {af.read(64)!r}\n> {bf.read(64)!r}"
                )
            if not av:
                return
            offset += len(av)


def compare(left, right, expect_exe_sha256=None):
    if expect_exe_sha256 is not None and len(expect_exe_sha256) != 2:
        raise AssertionError("expected two executable SHA-256 values")
    for root, expected in zip((left, right), expect_exe_sha256 or (None, None)):
        provenance = json.loads((root / "provenance.json").read_text(encoding="utf-8"))
        actual = provenance.get("executable_sha256", "")
        if not isinstance(actual, str) or not re.fullmatch("[0-9a-fA-F]{64}", actual):
            raise AssertionError(f"{root}: invalid executable SHA-256 provenance")
        if expected is not None and actual.lower() != expected.lower():
            raise AssertionError(f"{root}: executable SHA-256 {actual} != expected {expected}")
    lc = json.loads((left / "complete.json").read_text(encoding="utf-8"))
    rc = json.loads((right / "complete.json").read_text(encoding="utf-8"))
    if lc != rc or lc.get("schema") != 1:
        raise AssertionError("capture completion metadata differs")
    lm = json.loads((left / "manifest.json").read_text(encoding="utf-8"))
    rm = json.loads((right / "manifest.json").read_text(encoding="utf-8"))
    if lm != rm:
        raise AssertionError("corpus manifest differs (input hashes/options/case inventory)")
    if not lm or len(lm) != lc["renders"]:
        raise AssertionError("incomplete capture manifest")
    diagnostic = lc.get("diagnostic", False)
    rates = lc.get("rates", (44100, 48000))
    if not isinstance(diagnostic, bool) or not rates or any(rate not in (44100, 48000) for rate in rates):
        raise AssertionError("invalid capture mode/rates")
    if not diagnostic:
        expected = {f"{name}/{rate}" for name in pinned_cases() for rate in rates}
        if set(lm) != expected:
            raise AssertionError("full capture case set differs from pinned corpus list")
    left_projections = 0
    for name in sorted(lm):
        validate_dump_records(left, name)
        validate_dump_records(right, name)
        for dump in ("A.txt", "C.txt", "D.json"):
            a, b = left / name / dump, right / name / dump
            compare_bytes(a, b, f"{name}/{dump}")
        # B is required of the new (right) executable; an older left one may predate it.
        validate_projection(right, name)
        if projection_present(left, name):
            validate_projection(left, name)
            left_projections += 1
    label = "DIAGNOSTIC PASS" if diagnostic else "PASS"
    print(
        f"{label}: {len(lm)} renders, dumps A/C/D byte-identical; "
        f"B verified in the right executable ({len(lm)}) and the left ({left_projections})"
    )


def metrics(path):
    data = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields[0] == "allocations":
            data.setdefault("allocations", {})[fields[1]] = list(map(int, fields[2:]))
        elif fields[0] == "seconds":
            data.setdefault("seconds", {})[fields[1]] = float(fields[2])
        else:
            data[fields[0]] = float(fields[1])
    return data


def percentiles(values):
    values = sorted(values)
    return {
        name: values[math.ceil(len(values) * quantile) - 1]
        for name, quantile in (("p50", 0.5), ("p95", 0.95), ("max", 1))
    }


def measure(exe, assets, font, output, trials, cancel_trials, phases=range(5)):
    assert trials > 0 and cancel_trials > 0
    output.mkdir(parents=True, exist_ok=False)
    score = output / "dense.musicxml"
    generate_cancel_score(score)
    samples = []
    for phase in phases:
        for trial in range(trials if phase == 0 else cancel_trials):
            args = ["--measure"]
            if phase:
                args += [
                    "--cancel-phase",
                    str(phase),
                    "--cancel-poll",
                    str(2000 + trial * 3000 if phase == 1 else 1 + trial * 3),
                ]
            target = output / f"phase{phase}" / str(trial)
            run_probe(exe, assets, font, score, target, args)
            samples.append(dict(phase=phase, trial=trial, **metrics(target / "metrics.txt")))
            (target / "audio.wav").unlink(missing_ok=True)
            write_json(output / "samples.json", samples)
            print(f"phase={phase} trial={trial}", flush=True)
    summary = {}
    for phase in phases:
        rows = [s for s in samples if s["phase"] == phase]
        summary[str(phase)] = {
            k: percentiles([s[k] for s in rows]) for k in rows[0] if k.endswith("_ms") or k == "peak_working_set_bytes"
        }
    summary["allocation_columns"] = ["new", "delete", "malloc", "realloc", "free", "calloc", "aligned_new"]
    summary["allocation_totals"] = [s["allocations"]["total"] for s in samples if s["phase"] == 0]
    write_json(output / "summary.json", summary)
