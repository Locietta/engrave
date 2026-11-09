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
        if f.read(4) != b"RIFF":
            raise ValueError(f"not a RIFF file: {path}")
        f.read(4)
        if f.read(4) != b"WAVE":
            raise ValueError(f"not a WAVE file: {path}")
        fmt = None
        while header := f.read(8):
            tag, size = struct.unpack("<4sI", header)
            if tag == b"fmt ":
                fmt = struct.unpack("<HHIIHH", f.read(size)[:16])
            elif tag == b"data":
                if not fmt or fmt[0] != 3 or fmt[5] != 32 or not fmt[4]:
                    raise ValueError(f"not FLOAT32 PCM: {fmt}")
                digest = hashlib.sha256()
                remaining = size
                while remaining:
                    data = f.read(min(remaining, 1024 * 1024))
                    if not data:
                        raise ValueError("truncated PCM")
                    digest.update(data)
                    remaining -= len(data)
                if size % fmt[4] != 0:
                    raise ValueError("partial PCM frame")
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


PROJECTION_HEADER = b"projection-v2\n"
# The step-0..4a probes wrote exactly this stub. A left capture's B must be this stub or a complete projection.
DEFERRED_B_STUB = b"DEFERRED: independent flat-record projection extractors arrive in step 4b\n"
EVENT_KINDS = {b"note": 1, b"text": 2, b"preset": 3, b"syllable": 4}  # PlaybackEvent variant indices, as in dump A

_INT = re.compile(rb"-?[0-9]+")
_UINT = re.compile(rb"[0-9]+")
_HEX16 = re.compile(rb"[0-9a-f]{16}")
_HEX8 = re.compile(rb"[0-9a-f]{8}")
_UNITS = re.compile(rb"([0-9]+):([0-9a-f]*)")


class _Projection:
    """Strict reader of one dump-B file: every line, token and count, and the end terminator."""

    def __init__(self, path):
        self.path = path
        data = path.read_bytes()
        if not data.startswith(PROJECTION_HEADER):
            raise AssertionError(f"{path}: dump B is deferred or not a projection ({data[:32]!r})")
        if not data.endswith(b"\n"):
            raise AssertionError(f"{path}: truncated dump B (no final newline)")
        self.lines = data[len(PROJECTION_HEADER) : -1].split(b"\n") if len(data) > len(PROJECTION_HEADER) else []
        self.index = 0

    def fail(self, message):
        raise AssertionError(f"{self.path}: line {self.index + 2}: {message}")

    def peek(self):
        if self.index >= len(self.lines):
            self.fail("truncated dump B (missing end-projection)")
        return self.lines[self.index]

    def take(self, kind, count=None):
        tokens = self.peek().split(b" ")
        if tokens[0] != kind:
            self.fail(f"expected {kind!r} record, found {self.peek()[:48]!r}")
        if count is not None and len(tokens) - 1 != count:
            self.fail(f"malformed {kind!r} record ({len(tokens) - 1} fields, expected {count})")
        self.index += 1
        return tokens[1:]

    def match(self, pattern, token, what):
        if not pattern.fullmatch(token):
            self.fail(f"malformed {what} {token[:32]!r}")
        return token

    def number(self, token, what, signed=True):
        return int(self.match(_INT if signed else _UINT, token, what))

    def units(self, token, what):
        m = self.match(_UNITS, token, what)
        m = _UNITS.fullmatch(m)
        if len(m.group(2)) != 4 * int(m.group(1)):
            self.fail(f"{what} length does not match its code units")

    def optional_bytes(self, token, what):
        if token == b"-":
            return
        m = _UNITS.fullmatch(token)
        if not m or len(m.group(2)) != 2 * int(m.group(1)):
            self.fail(f"malformed {what} {token[:32]!r}")

    def counted(self, kind, fields_per_item):
        tokens = self.take(kind)
        if not tokens:
            self.fail(f"malformed {kind!r} record (no count)")
        count = self.number(tokens[0], f"{kind!r} count", signed=False)
        if len(tokens) - 1 != count:
            self.fail(f"{kind!r} count {count} does not match its {len(tokens) - 1} items")
        for item in tokens[1:]:
            parts = item.split(b":")
            if len(parts) != fields_per_item:
                self.fail(f"malformed {kind!r} item {item[:32]!r}")
            for part in parts:
                self.number(part, f"{kind!r} item")

    def event(self):
        kind = self.peek().split(b" ")[0]
        if kind not in EVENT_KINDS:
            self.fail(f"unknown event record {self.peek()[:48]!r}")
        if kind == b"note":
            t = self.take(b"note", 10)
            for token in t[:4] + t[5:7]:
                self.number(token, "note field")
            self.match(_HEX16, t[4], "note bps bits")
            if t[7] != b"-":
                self.match(_HEX8, t[7], "velocity override bits")
            self.number(t[8], "staff layer", signed=False)
            self.number(t[9], "voice layer", signed=False)
            self.counted(b"arts", 3)
            self.counted(b"pitch", 2)
            self.counted(b"expression", 2)
        elif kind == b"preset":
            t = self.take(b"preset", 2)
            self.number(t[0], "preset layer", signed=False)
            self.units(t[1], "preset code")
        else:
            t = self.take(kind, 3)
            self.number(t[0], "event layer", signed=False)
            self.number(t[1], "event flags", signed=False)
            self.units(t[2], "event text")
        return EVENT_KINDS[kind]

    def track(self):
        line = self.peek()
        t = self.take(b"track", 2)
        self.number(t[0], "track part", signed=False)
        self.units(t[1], "track instrument")
        s = self.take(b"setup")
        if len(s) < 6:
            self.fail("malformed setup record")
        self.number(s[0], "setup category")
        if s[1] not in (b"0", b"1"):
            self.fail("malformed setup dynamics flag")
        self.units(s[2], "setup id")
        subs = self.number(s[3], "setup sub-category count", signed=False)
        if len(s) != 6 + subs:
            self.fail("setup sub-category count does not match")
        for token in s[4 : 4 + subs]:
            self.units(token, "setup sub-category")
        self.optional_bytes(s[4 + subs], "setup score id")
        self.optional_bytes(s[5 + subs], "setup MusicXML sound id")
        layers = []
        for _ in range(self.number(self.take(b"dynamics", 1)[0], "dynamics count", signed=False)):
            layer, points = (self.number(v, "layer field", signed=False) for v in self.take(b"layer", 2))
            layers.append((layer, points))
            for _ in range(points):
                p = self.take(b"point")
                if len(p) < 3:
                    self.fail("malformed point record")
                self.number(p[0], "point time")
                if p[1] not in (b"0", b"1") or len(p) != (3 if p[1] == b"0" else 6):
                    self.fail("malformed point record")
                for token in p[2:]:
                    self.match(_HEX16, token, "point bits")
        groups = []
        for _ in range(self.number(self.take(b"groups", 1)[0], "groups count", signed=False)):
            g = self.take(b"group", 2)
            at, count = self.number(g[0], "group key"), self.number(g[1], "group size", signed=False)
            if groups and at <= groups[-1][0]:
                self.fail("group keys are not strictly ascending")
            groups.append((at, [self.event() for _ in range(count)]))
        return (line, layers, groups)

    def read(self):
        tracks = []
        while not self.peek().startswith(b"end-projection"):
            tracks.append(self.track())
        end = self.take(b"end-projection", 1)
        if self.number(end[0], "end-projection count", signed=False) != len(tracks):
            self.fail("end-projection count does not match the tracks")
        if self.index != len(self.lines):
            self.fail("data after end-projection")
        return tracks


def projection_inventory(path):
    """Parses a complete dump B; any truncation, malformed record or count mismatch raises."""
    return _Projection(path).read()


def dump_a_inventory(path):
    """Track/dynamics/group/event inventory of dump A, independent evidence for dump B."""
    tracks = []
    with path.open("rb") as stream:
        for raw in stream:
            line = raw.rstrip(b"\n")
            if line.startswith(b"track "):
                tracks.append((line, [], []))
            elif line.startswith(b"layer "):
                fields = line.split(b" ")
                tracks[-1][1].append((int(fields[1]), int(fields[2])))
            elif line.startswith(b"group "):
                fields = line.split(b" ")
                tracks[-1][2].append((int(fields[1]), int(fields[2]), []))
            elif line.startswith(b"event "):
                tracks[-1][2][-1][2].append(int(line.split(b" ")[1]))
    for _, _, groups in tracks:
        for at, count, kinds in groups:
            if count != len(kinds):
                raise AssertionError(f"{path}: group {at} lists {len(kinds)} of {count} events")
    return [(line, layers, [(at, kinds) for at, _, kinds in groups]) for line, layers, groups in tracks]


def validate_projection(root, name):
    """Dump B is an oracle inside one executable: flat records against the independent legacy extractor,
    both complete, byte-equal, and matching dump A's track, dynamics, group and event inventory."""
    flat, legacy = root / name / "B.txt", root / name / "B_legacy.txt"
    for path in (flat, legacy):
        if not path.exists():
            raise AssertionError(f"{path}: dump B is missing")
    inventories = [projection_inventory(path) for path in (flat, legacy)]
    compare_bytes(legacy, flat, f"{name}/B.txt (flat records vs legacy extractor)")
    if inventories[0] != dump_a_inventory(root / name / "A.txt"):
        raise AssertionError(f"{root / name}: dump B inventory differs from dump A")


def left_projection(root, name):
    """A left capture carries either the recognized pre-4b stub (False) or a complete projection (True)."""
    path = root / name / "B.txt"
    if not path.exists():
        raise AssertionError(f"{path}: dump B is missing")
    if path.stat().st_size == len(DEFERRED_B_STUB) and path.read_bytes() == DEFERRED_B_STUB:
        return False
    validate_projection(root, name)
    return True


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
        if left_projection(left, name):
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


def mixed_score_witness(exe, assets, font, output, rate=44100):
    """Two different multipart scores: fresh processes versus A/B/A and B/A/B on one Engine."""
    if rate not in (44100, 48000):
        raise ValueError(f"unsupported mixed witness rate: {rate}")
    output.mkdir(parents=True, exist_ok=False)
    scores = [FOCUSED / "mixed_score_a.mscx", FOCUSED / "mixed_score_b.musicxml"]
    rate_options = ["--rate", str(rate)]
    for i, path in enumerate(scores):
        run_probe(exe, assets, font, path, output / f"fresh-{i}", rate_options)
    if (output / "fresh-0/audio.wav").read_bytes() == (output / "fresh-1/audio.wav").read_bytes():
        raise ValueError("mixed witness scores produced identical audio")
    for order in ((0, 1, 0), (1, 0, 1)):
        name = "".join(str(i) for i in order)
        targets = [output / f"{name}-{j}" for j in range(3)]
        options = list(rate_options)
        for i, target in zip(order[1:], targets[1:]):
            options += ["--next", str(scores[i].resolve()), str(target.resolve())]
        run_probe(exe, assets, font, scores[order[0]], targets[0], options)
        for i, target in zip(order, targets):
            fresh = output / f"fresh-{i}"
            for dump in ("A.txt", "C.txt", "audio.wav"):
                if (target / dump).read_bytes() != (fresh / dump).read_bytes():
                    raise ValueError(f"mixed witness mismatch: {name}, {target}, {dump}")
            if (target / "B.txt").read_bytes() != (target / "B_legacy.txt").read_bytes():
                raise ValueError(f"mixed witness projection mismatch: {target}")
    for target in [output / f"fresh-{i}" for i in (0, 1)] + [
        output / f"{order}-{j}" for order in ("010", "101") for j in range(3)
    ]:
        actual = pcm_metadata(target / "audio.wav")["rate"]
        if actual != rate:
            raise ValueError(f"mixed witness rate mismatch: requested {rate}, got {actual}: {target}")
    write_json(
        output / "result.json",
        dict(
            result="PASS",
            orders=["A/B/A", "B/A/B"],
            exports=8,
            rate=rate,
            compared=["A", "C", "PCM"],
            projection="B verified",
        ),
    )
