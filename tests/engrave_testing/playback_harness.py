"""Playback dump capture, exact comparison and fresh-process measurements."""

import hashlib
import json
import math
import os
import struct
import subprocess

from engrave_testing.cancel_score import generate_cancel_score
from engrave_testing.playback_corpus import corpus


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


def capture(exe, assets, font, output, match=None):
    output.mkdir(parents=True, exist_ok=False)
    profiles = {p.relative_to(assets).as_posix(): sha(p) for p in sorted(assets.rglob("*.json"))}
    cases = corpus(output / "generated")
    selected = [(n, p, o) for n, p, o in cases if match is None or match in n]
    assert selected, "no cases selected"
    manifest = {}
    for name, score, options in selected:
        for rate in (44100, 48000):
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


def compare(left, right):
    lm = json.loads((left / "manifest.json").read_text(encoding="utf-8"))
    rm = json.loads((right / "manifest.json").read_text(encoding="utf-8"))
    if lm != rm:
        raise AssertionError("corpus manifest differs (input hashes/options/case inventory)")
    for name in sorted(lm):
        for dump in ("A.txt", "B.txt", "C.txt", "D.json"):
            a, b = left / name / dump, right / name / dump
            if sha(a) == sha(b):
                continue
            with a.open(encoding="utf-8") as af, b.open(encoding="utf-8") as bf:
                from itertools import zip_longest

                for line, (av, bv) in enumerate(zip_longest(af, bf), 1):
                    if av != bv:
                        raise AssertionError(f"{name}/{dump}:{line}\n< {av!r}\n> {bv!r}")
    print(f"PASS: {len(lm)} renders, dumps A/C/D byte-identical; B deferred")


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


def measure(exe, assets, font, output, trials, cancel_trials):
    assert trials > 0 and cancel_trials > 0
    output.mkdir(parents=True, exist_ok=False)
    score = output / "dense.musicxml"
    generate_cancel_score(score)
    samples = []
    for phase in range(5):
        for trial in range(trials if phase == 0 else cancel_trials):
            args = ["--measure"]
            if phase:
                args += ["--cancel-phase", str(phase), "--cancel-poll", str(1 + trial * 3)]
            target = output / f"phase{phase}" / str(trial)
            run_probe(exe, assets, font, score, target, args)
            samples.append(dict(phase=phase, trial=trial, **metrics(target / "metrics.txt")))
            (target / "audio.wav").unlink(missing_ok=True)
            write_json(output / "samples.json", samples)
            print(f"phase={phase} trial={trial}", flush=True)
    summary = {}
    for phase in range(5):
        rows = [s for s in samples if s["phase"] == phase]
        summary[str(phase)] = {
            k: percentiles([s[k] for s in rows]) for k in rows[0] if k.endswith("_ms") or k == "peak_working_set_bytes"
        }
    summary["allocation_columns"] = ["new", "delete", "malloc", "realloc", "free", "calloc", "aligned_new"]
    summary["allocation_totals"] = [s["allocations"]["total"] for s in samples if s["phase"] == 0]
    write_json(output / "summary.json", summary)
