"""Golden-output capture and comparison for the native MCP server.

Capture exports a fixed fixture set through the official Python MCP client,
normalizes known-volatile fields, and stores one directory per fixture plus an
``index.json`` manifest of sha256 values. Compare re-captures the same tree and
diffs it against the stored goldens, honoring an explicit allow-list.

Allow entries name exact fixture/output pairs and platform SHA-256 pins for
``different`` or ``extra`` artifacts. Missing artifacts cannot be waived.
Every entry in the comparison context must be exercised; stale entries fail.
Archive MSCX exceptions are restricted to the engine-gate context. ``pin``
prints skeleton entries from an existing report for review; missing-artifact
skeletons are diagnostic only and are rejected by the policy loader.

Volatile fields normalized (observed by running twice):
  * Every MSCX/MSCZ inner ``<eid>`` and EID-reference value (``linkedTo``,
    ``startElement``, ``endElement``, ``NoteEID``, ``mmRestLast``,
    ``startMeasure``, ``endMeasure``, ``sharedPart``) is mapped to a stable
    token (``e1``, ``e2``, ...) in order of first appearance per document (per
    file, after the whole file is read), preserving element identity and
    endpoint relationships across non-deterministic runtime EID allocations.
  * MSCX ``programVersion``/``programRevision`` and metaTag ``creationDate``/
    ``platform``; MusicXML ``encoding-date``/``software`` and the mirrored
    ``miscellaneous-field`` creationDate/platform values.
  * PDF: cairo writes CreationDate/Producer inside a compressed object stream.
    The PDF is opened with PyMuPDF, metadata is cleared, and the re-serialized
    bytes are hashed (garbage=3, deflate, no new id). ``bytes`` is the size of
    that normalized PDF because the raw export size varies by one byte.
    MusicXML also mirrors the MSCX creationDate/platform metaTags as
    ``miscellaneous-field`` values; those are blanked too.
Everything else is compared byte for byte, except ``audio.json`` which uses
the documented numeric tolerances. ``audio.json`` also stores exact silence
evidence per 0.25 s window: ``nonzeroSamples`` counts, per channel, the WAV
samples that are not exactly zero. A spectral centroid is compared (2.0 Hz)
in every window except those whose count is zero on *both* sides, i.e. windows
the PCM proves are exactly all-zero; a rounded-zero RMS envelope alone is not
evidence of silence. A baseline without the field is rejected instead of
falling back to a weaker rule.

All current MNX, edit and excerpt artifacts share the platform index.

``semantic.json`` (the exported MSCZ) and ``semantic-source.json`` (the fixture
file) come from the ``semantic_dump`` dev tool next to the server binary. They
record musical content read from the in-memory DOM, independent of the MSCX
format version, and are the rebase oracle of roadmap-engrave A0/A2.

EID normalization preserves target identity through stable tokens.

Usage:
  golden.py capture --binary B --resources R --soundfont SF --golden DIR \\
                    [--allow-errors]
  golden.py compare --binary B --resources R --soundfont SF --golden DIR \\
                    --allow tests/data/golden/allow.json --output OUTDIR \\
                    [--platform windows|linux]
  golden.py pin --golden DIR --output OUTDIR [--platform windows|linux]
"""

from __future__ import annotations

import sys

from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import argparse

import asyncio

import base64

import difflib

import hashlib

import json

import os

import re

import shutil

import subprocess

import sys

import tempfile

import xml.etree.ElementTree as ET

import zipfile

from fractions import Fraction

from pathlib import Path

from support.composition_fixture import composition_musicxml

from support.musicxml_semantic import musicxml_semantic

from support.paths import REPO_ROOT

ROOT = Path(__file__).resolve().parents[2]

FIXTURE_DIR = ROOT / "tests/data/inputs" / "golden"

EXTRA_FIXTURES = (ROOT / "tests/data/inputs" / "evening.musicxml",)

GENERATE_EXCERPTS = {"chordline_path.mscx"}

MNX_FIXTURES = (
    "engine/two-notes.mnx",
    "project_examples/tupletSimple.mnx",
    "project_examples/percussionKit.mnx",
    "project_examples/graceArps.mnx",
)

AUDIO_TOLERANCES = {
    "peak": 1e-4,
    "rms": 1e-4,
    "rmsEnvelope": 1e-4,
    "spectralCentroidHz": 1.0,
    "centroidEnvelope": 2.0,
}

AUDIO_EXACT = {"sampleRate", "channels", "frames"}

SILENCE_EVIDENCE = "nonzeroSamples"

ALLOW_KINDS = ("different", "extra")

DEFAULT_PLATFORM = "windows" if sys.platform.startswith("win") else "linux"

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

class GoldenFormatError(ValueError):
    """A golden file lacks evidence this golden.py requires; recapture it."""

def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")

def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")

def write_json(path: Path, value, indent: int = 1) -> None:
    write_text(
        path,
        json.dumps(value, indent=indent, sort_keys=True, ensure_ascii=False, allow_nan=False) + "\n",
    )

EID_TAGS = (
    "eid",
    "linkedTo",
    "startElement",
    "endElement",
    "NoteEID",
    "mmRestLast",
    "startMeasure",
    "endMeasure",
    "sharedPart",
)

def normalize_mscx(text: str) -> str:
    eid_map: dict[str, str] = {}

    def replace_eid(match: re.Match) -> str:
        tag = match.group(1)
        val = match.group(2).strip()
        if not val:
            return f"<{tag}/>"
        if val not in eid_map:
            eid_map[val] = f"e{len(eid_map) + 1}"
        return f"<{tag}>{eid_map[val]}</{tag}>"

    tag_pattern = rf"<({'|'.join(EID_TAGS)})>([^<]+)</\1>"
    text = re.sub(tag_pattern, replace_eid, text)

    for tag in ("programVersion", "programRevision"):
        text = re.sub(rf"(<{tag}>)[^<]*(</{tag}>)", r"\1\2", text)
    for name in ("creationDate", "platform"):
        text = re.sub(rf'(<metaTag name="{name}">)[^<]*(</metaTag>)', r"\1\2", text)
    return text

def normalize_musicxml(text: str) -> str:
    for tag in ("encoding-date", "software"):
        text = re.sub(rf"(<{tag}>)[^<]*(</{tag}>)", r"\1\2", text)
    for name in ("creationDate", "platform"):
        text = re.sub(
            rf'(<miscellaneous-field name="{name}">)[^<]*(</miscellaneous-field>)',
            r"\1\2",
            text,
        )
    return text

def normalize_svg(text: str) -> str:
    # SVG exports carry no timestamp in the observed engine build; keep as-is.
    return text

def round_json(value):
    if isinstance(value, float):
        return round(value, 4)
    if isinstance(value, dict):
        return {key: round_json(item) for key, item in value.items()}
    if isinstance(value, list):
        return [round_json(item) for item in value]
    return value

def image_size(data: bytes) -> dict:
    import io

    from PIL import Image

    with Image.open(io.BytesIO(data)) as image:
        return {"width": image.width, "height": image.height}

def load_rounded(path: Path):
    return round_json(json.loads(read_text(path)))

def describe_png(path: Path) -> dict:
    from PIL import Image

    with Image.open(path) as image:
        rgba = image.convert("RGBA")
        return {
            "width": image.width,
            "height": image.height,
            "mode": image.mode,
            "sha256": sha256(rgba.tobytes()),
        }

def describe_pdf(path: Path) -> dict:
    import pymupdf  # pinned in pixi.toml; cairo hides the volatile metadata in object streams

    document = pymupdf.open(stream=path.read_bytes(), filetype="pdf")
    try:
        pages = document.page_count
        document.set_metadata(
            {
                key: ""
                for key in (
                    "title",
                    "author",
                    "subject",
                    "keywords",
                    "creator",
                    "producer",
                    "creationDate",
                    "modDate",
                )
            }
        )
        canonical = document.tobytes(garbage=3, deflate=True, no_new_id=True)
    finally:
        document.close()
    canonical = re.sub(rb"/(CreationDate|ModDate|Producer)\s*\([^)]*\)", rb"/\1 ()", canonical)
    # Raw export bytes vary by one with compressed cairo metadata; report the
    # normalized size so the golden is stable.
    return {"pages": pages, "bytes": len(canonical), "sha256": sha256(canonical)}

def describe_audio(path: Path) -> dict:
    import numpy as np
    import soundfile as sf

    audio, rate = sf.read(path, always_2d=True)
    frames, channels = audio.shape
    peak = [round(float(np.max(np.abs(audio[:, channel]))), 6) for channel in range(channels)]
    rms = [round(float(np.sqrt(np.mean(audio[:, channel] ** 2))), 6) for channel in range(channels)]
    window = max(1, int(round(rate * 0.25)))
    mono = audio.mean(axis=1)
    envelope, centroids, nonzero = [], [], []
    for start in range(0, frames, window):
        chunk = audio[start : start + window]
        envelope.append([round(float(np.sqrt(np.mean(chunk[:, channel] ** 2))), 5) for channel in range(channels)])
        # Exact silence evidence: the stored RMS is rounded, so only a zero sample count
        # proves a window is all-zero PCM (and hence that its centroid means nothing).
        nonzero.append([int(np.count_nonzero(chunk[:, channel])) for channel in range(channels)])
        segment = mono[start : start + window]
        spectrum = np.abs(np.fft.rfft(segment))
        frequencies = np.fft.rfftfreq(len(segment), 1.0 / rate)
        weight = float(spectrum.sum())
        centroids.append(round(float(np.dot(frequencies, spectrum)) / weight, 1) if weight else 0.0)
    block = 1 << 20
    weighted = 0.0
    weight = 0.0
    for start in range(0, len(mono), block):
        chunk = mono[start : start + block]
        spectrum = np.abs(np.fft.rfft(chunk))
        frequencies = np.fft.rfftfreq(len(chunk), 1.0 / rate)
        weighted += float(np.dot(frequencies, spectrum))
        weight += float(spectrum.sum())
    return {
        "sampleRate": int(rate),
        "channels": int(channels),
        "frames": int(frames),
        "peak": peak,
        "rms": rms,
        "rmsEnvelope": envelope,
        "centroidEnvelope": centroids,
        "nonzeroSamples": nonzero,
        "spectralCentroidHz": round(weighted / weight, 1) if weight else 0.0,
    }

def describe_mscz(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = sorted(archive.namelist())
        entries, texts = [], {}
        for name in names:
            data = archive.read(name)
            base = name.rsplit("/", 1)[-1].lower()
            if base.endswith(".mscx"):
                normalized = normalize_mscx(data.decode("utf-8"))
                entries.append({"name": name, "sha256": sha256(normalized.encode("utf-8"))})
            elif base in ("audiosettings.json", "viewsettings.json"):
                entries.append({"name": name, "size": len(data)})
                texts[name] = data.decode("utf-8")
            elif base.startswith("thumbnail") and base.endswith((".png", ".jpg", ".jpeg")):
                entries.append({"name": name, **image_size(data)})
            else:
                entries.append({"name": name, "size": len(data)})
    return {"entries": entries, "texts": texts}

def describe_mxl(path: Path) -> dict:
    with zipfile.ZipFile(path) as archive:
        names = sorted(archive.namelist())
        inner = next(
            (
                name
                for name in names
                if name.lower().endswith((".xml", ".musicxml")) and not name.upper().startswith("META-INF/")
            ),
            None,
        )
        document = {"entries": names}
        if inner:
            normalized = normalize_musicxml(archive.read(inner).decode("utf-8"))
            document["musicxml"] = {
                "name": inner,
                "sha256": sha256(normalized.encode("utf-8")),
            }
    return document

def describe_midi(data: bytes) -> dict:
    if not data.startswith(b"MThd"):
        raise ValueError("invalid MIDI header")
    header_len = int.from_bytes(data[4:8], "big")
    midi_format = int.from_bytes(data[8:10], "big")
    track_count = int.from_bytes(data[10:12], "big")
    division = int.from_bytes(data[12:14], "big")
    position = 8 + header_len
    tracks = []
    for _ in range(track_count):
        if data[position : position + 4] != b"MTrk":
            raise ValueError(f"expected MTrk at byte {position}")
        length = int.from_bytes(data[position + 4 : position + 8], "big")
        track = data[position + 8 : position + 8 + length]
        position += 8 + length
        events, cursor, tick, running = [], 0, 0, None
        while cursor < len(track):
            delta = 0
            while cursor < len(track):
                byte = track[cursor]
                cursor += 1
                delta = (delta << 7) | (byte & 0x7F)
                if not byte & 0x80:
                    break
            tick += delta
            if cursor >= len(track):
                break
            lead = track[cursor]
            if lead == 0xFF:
                meta_type = track[cursor + 1]
                cursor += 2
                size = 0
                while cursor < len(track):
                    byte = track[cursor]
                    cursor += 1
                    size = (size << 7) | (byte & 0x7F)
                    if not byte & 0x80:
                        break
                payload = track[cursor : cursor + size]
                cursor += size
                events.append(
                    [
                        tick,
                        {"type": "meta", "metaType": meta_type, "data": payload.hex()},
                    ]
                )
                running = None
                if meta_type == 0x2F:
                    break
                continue
            if lead in (0xF0, 0xF7):
                cursor += 1
                size = 0
                while cursor < len(track):
                    byte = track[cursor]
                    cursor += 1
                    size = (size << 7) | (byte & 0x7F)
                    if not byte & 0x80:
                        break
                events.append(
                    [
                        tick,
                        {"type": "sysex", "data": track[cursor : cursor + size].hex()},
                    ]
                )
                cursor += size
                running = None
                continue
            status = lead if lead & 0x80 else running
            if lead & 0x80:
                cursor += 1
            running = status
            command, channel = status & 0xF0, status & 0x0F
            if command in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                first, second = track[cursor], track[cursor + 1]
                cursor += 2
                if command in (0x80, 0x90) and not (command == 0x90 and second > 0):
                    events.append(
                        [
                            tick,
                            {
                                "type": "note_off",
                                "ch": channel,
                                "pitch": first,
                                "vel": second,
                            },
                        ]
                    )
                elif command == 0x90:
                    events.append(
                        [
                            tick,
                            {
                                "type": "note_on",
                                "ch": channel,
                                "pitch": first,
                                "vel": second,
                            },
                        ]
                    )
                elif command == 0xA0:
                    events.append(
                        [
                            tick,
                            {
                                "type": "poly_aftertouch",
                                "ch": channel,
                                "pitch": first,
                                "pressure": second,
                            },
                        ]
                    )
                elif command == 0xB0:
                    events.append(
                        [
                            tick,
                            {"type": "cc", "ch": channel, "num": first, "val": second},
                        ]
                    )
                else:
                    events.append(
                        [
                            tick,
                            {
                                "type": "pitchbend",
                                "ch": channel,
                                "value": first | (second << 7),
                            },
                        ]
                    )
            elif command in (0xC0, 0xD0):
                first = track[cursor]
                cursor += 1
                if command == 0xC0:
                    events.append([tick, {"type": "program", "ch": channel, "program": first}])
                else:
                    events.append([tick, {"type": "aftertouch", "ch": channel, "pressure": first}])
            else:
                raise ValueError(f"unsupported MIDI status {status}")
        tracks.append(events)
    return {"format": midi_format, "division": division, "tracks": tracks}

def positions(path):
    result = load_rounded(path)
    result["pageSize"] = result.pop("page_size")
    if path.name.startswith("segment"):
        for item in result["elements"]:
            for key in ("x", "y"):
                if not float(item[key]).is_integer():
                    raise ValueError(f"non-integral segment {key}: {item[key]}")
                item[key] = int(item[key])
    return result

def sanitize(text: str, roots: list[Path]) -> str:
    for root in roots:
        text = text.replace(str(root), "<tmp>")
    return text

SEMANTIC_DUMP: dict = {}

def semantic_dump(source: Path, dest: Path, roots, *extra: str) -> None:
    """Run the semantic_dump dev tool next to the server binary; errors become .error.txt."""
    if not source.is_file():
        write_text(dest.with_suffix(".error.txt"), "source export missing\n")
        return
    result = subprocess.run(
        [
            str(SEMANTIC_DUMP["binary"]),
            str(SEMANTIC_DUMP["resources"]),
            str(source),
            *extra,
        ],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        write_text(
            dest.with_suffix(".error.txt"),
            sanitize(result.stderr.decode("utf-8", "replace"), roots) + "\n",
        )
        return
    write_json(dest, json.loads(result.stdout.decode("utf-8")))

EDIT_SCENARIOS = {
    "edit-replace-measures": {
        "source": "created.json",
        "kind": "replace-measures",
        "start_measure": 1,
        "notes": [
            {"pitches": [60, 64, 67], "ticks": 960},
            {"pitches": [62], "ticks": 480},
            {"pitches": [], "ticks": 240},
            {"pitches": [65], "ticks": 240},
        ],
    },
    "edit-set-pitches-coverage": {
        "source": "coverage.musicxml",
        "kind": "set-pitches",
        "part": 0,
        "measures": 2,
        "notes": 4,
        "delta": 2,
    },
    # Generated part excerpts are linked to the master, so the edit must reach them too
    # (linked_parts.mscx is unusable here: every note in it is tied).
    "edit-set-pitches-linked": {
        "source": "chordline_path.mscx",
        "kind": "set-pitches",
        "generate": True,
        "part": 0,
        "measures": 2,
        "notes": 3,
        "delta": 1,
    },
}

def fixture_files() -> list[Path]:
    files = sorted(path for path in FIXTURE_DIR.iterdir() if path.is_file() and path.name != "README.md")
    return files + [path for path in EXTRA_FIXTURES if path.is_file()]

def mnx_fixture_files() -> list[Path]:
    """Engine-owned MNX inputs covered by the platform baseline."""
    engine = Path(os.environ.get("MUSESCORE_ENGINE_SOURCE", ROOT))
    data = engine / "src/importexport/mnx/tests/data"
    files = [data / name for name in MNX_FIXTURES]
    missing = [str(path) for path in files if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"missing MNX fixtures: {missing}")
    return files

def reset_root(root: Path) -> None:
    if root.exists():
        for child in root.iterdir():
            if child.is_dir():
                shutil.rmtree(child)
            else:
                child.unlink()
    root.mkdir(parents=True, exist_ok=True)

def build_index(root: Path) -> dict:
    index = {}
    for folder in sorted(path for path in root.iterdir() if path.is_dir()):
        files = {}
        for path in sorted(folder.rglob("*")):
            if path.is_file():
                files[path.relative_to(folder).as_posix()] = sha256(path.read_bytes())
        index[folder.name] = files
    write_json(root / "index.json", index)
    return index

def load_allow(path: Path, context: str = "host") -> list[dict]:
    if not path.is_file():
        return []
    entries = json.loads(read_text(path)).get("entries", [])
    for entry in entries:
        missing = [key for key in ("fixture", "output", "step", "reason") if key not in entry]
        if missing:
            raise ValueError(f"allow entry {entry!r} is missing {', '.join(missing)}")
        if entry.setdefault("kind", "different") not in ALLOW_KINDS:
            raise ValueError(f"allow entry has invalid kind: {entry['kind']!r}")
        for key in ("fixture", "output", "step", "reason"):
            if not isinstance(entry[key], str) or not entry[key].strip():
                raise ValueError(f"invalid allow {key}")
        if any(c in entry[key] for key in ("fixture", "output") for c in "*?["):
            raise ValueError("allow entries must name exact artifacts")
        if entry.get("context") not in ("host", "engine-gate-archive"):
            raise ValueError("allow entries require an explicit context: host or engine-gate-archive")
        pins = entry.get("sha256")
        if (
            not isinstance(pins, dict)
            or not pins
            or any(p not in ("windows", "linux") or not re.fullmatch(r"[0-9a-f]{64}", str(h)) for p, h in pins.items())
        ):
            raise ValueError("allow entries require explicit platform SHA-256 pins")
    return [e for e in entries if e["context"] == context]

def allow_accept(entries, fixture, output, kind, actual_sha, platform):
    """Return (position, entry, problem) for the first matching allow entry."""
    matches = [
        (position, entry)
        for position, entry in enumerate(entries)
        if fixture == entry["fixture"] and output == entry["output"]
    ]
    problems = []
    for position, entry in matches:
        if entry["kind"] != kind:
            problems.append(f"entry {position}: kind {entry['kind']} != {kind}")
            continue
        if kind not in ALLOW_KINDS:
            continue
        pinned = entry.get("sha256")
        pinned = pinned.get(platform) if isinstance(pinned, dict) else None
        if not pinned:
            problems.append(f"entry {position}: no sha256 for {platform}")
        elif str(pinned).lower() != actual_sha:
            problems.append(f"entry {position}: pinned {pinned} != actual {actual_sha}")
        else:
            return position, entry, None
    return None, None, "; ".join(problems) if problems else None

def verify_golden_index(root: Path, index: dict) -> list[str]:
    on_disk = {path.name for path in root.iterdir() if path.is_dir()}
    issues = [f"unindexed fixture {name}" for name in sorted(on_disk - set(index))]
    issues += [f"missing fixture folder {name}" for name in sorted(set(index) - on_disk)]
    for name in sorted(on_disk & set(index)):
        present = {path.relative_to(root / name).as_posix() for path in (root / name).rglob("*") if path.is_file()}
        issues += [f"missing indexed file {name}/{entry}" for entry in sorted(set(index[name]) - present)]
        issues += [f"unindexed file {name}/{entry}" for entry in sorted(present - set(index[name]))]
        for entry in sorted(present & set(index[name])):
            actual = sha256((root / name / entry).read_bytes())
            if actual != index[name][entry]:
                issues.append(f"indexed file hash mismatch {name}/{entry}: expected {index[name][entry]}, got {actual}")
    return issues

def silence_windows(audio: dict) -> list[bool]:
    """Per-window exact-silence flags from the stored PCM evidence."""
    counts = audio.get(SILENCE_EVIDENCE)
    if counts is None:
        raise GoldenFormatError(
            f"audio.json has no {SILENCE_EVIDENCE!r} silence evidence, so exact silence (and "
            "therefore whether a centroid window may be skipped) cannot be established; "
            "recapture the baseline with the current tests/support/golden.py"
        )
    windows = []
    for window in counts:
        if not isinstance(window, (list, tuple)):
            raise GoldenFormatError(f"audio.json {SILENCE_EVIDENCE!r} values must be per-channel lists")
        try:
            windows.append(all(int(count) == 0 for count in window))
        except (TypeError, ValueError):
            raise GoldenFormatError(f"audio.json {SILENCE_EVIDENCE!r} values must be sample counts") from None
    return windows

def audio_equal(golden, actual, key=None) -> bool:
    if isinstance(golden, dict) and isinstance(actual, dict):
        if "centroidEnvelope" in golden or "centroidEnvelope" in actual:
            # Exact silence evidence is required on both sides before any window is skipped;
            # a baseline without it is rejected, not compared under a weaker rule.
            g_silent = silence_windows(golden)
            a_silent = silence_windows(actual)
        if golden.keys() != actual.keys():
            return False
        has_centroids = "centroidEnvelope" in golden and "centroidEnvelope" in actual
        has_rms = "rmsEnvelope" in golden and "rmsEnvelope" in actual
        for name in golden:
            if name == SILENCE_EVIDENCE:
                # Evidence for the centroid gate below, not a compared tolerance: the count of
                # nonzero samples legitimately differs in reverb/release tails.
                continue
            if name == "centroidEnvelope" and has_centroids and has_rms:
                g_cents = golden["centroidEnvelope"]
                a_cents = actual["centroidEnvelope"]
                if len(g_cents) != len(a_cents) or len(g_cents) != len(g_silent) or len(a_cents) != len(a_silent):
                    return False
                tol = AUDIO_TOLERANCES.get("centroidEnvelope", 2.0)
                for i, (g_c, a_c) in enumerate(zip(g_cents, a_cents)):
                    # Only a window whose PCM samples are exactly all-zero on BOTH sides has no
                    # meaningful spectral centroid. A rounded-zero RMS is not such proof, and
                    # quiet nonzero windows (e.g. RMS 0.00010) are compared like any other.
                    if g_silent[i] and a_silent[i]:
                        continue
                    if abs(g_c - a_c) > tol:
                        return False
            else:
                if not audio_equal(golden[name], actual[name], name):
                    return False
        return True
    if isinstance(golden, list) and isinstance(actual, list):
        return len(golden) == len(actual) and all(audio_equal(left, right, key) for left, right in zip(golden, actual))
    if isinstance(golden, bool) or isinstance(actual, bool):
        return golden == actual
    if isinstance(golden, (int, float)) and isinstance(actual, (int, float)):
        if key in AUDIO_EXACT:
            return golden == actual
        return abs(golden - actual) <= AUDIO_TOLERANCES.get(key, 1e-9)
    return golden == actual

def files_equal(golden: Path, actual: Path, output: str) -> bool:
    if output == "audio.json":
        try:
            return audio_equal(json.loads(read_text(golden)), json.loads(read_text(actual)))
        except (json.JSONDecodeError, KeyError):
            return False
    return golden.read_bytes() == actual.read_bytes()

def write_diff(root: Path, fixture: str, output: str, golden: Path, actual: Path) -> None:
    lines = list(
        difflib.unified_diff(
            read_text(golden).splitlines(keepends=True),
            read_text(actual).splitlines(keepends=True),
            fromfile=f"golden/{fixture}/{output}",
            tofile=f"actual/{fixture}/{output}",
        )
    )
    if len(lines) > 2000:
        lines = lines[:2000] + ["... truncated after 2000 diff lines ...\n"]
    write_text(
        root / fixture / f"{output}.diff",
        "".join(line if line.endswith("\n") else line + "\n" for line in lines),
    )

def verify_input_provenance() -> list[str]:
    """Check current inputs against main's reviewed migration pins."""
    try:
        pins = json.loads(read_text(ROOT / "tests/data/golden/inputs.json"))["fixtures"]
        sources = {source.name: source for source in fixture_files() + mnx_fixture_files()}
        sources.update({name: FIXTURE_DIR / scenario["source"] for name, scenario in EDIT_SCENARIOS.items()})
        if set(pins) != set(sources):
            return ["input fixture inventory differs from reviewed pins"]
        issues = []
        for name, source in sources.items():
            entry = pins[name]
            if entry["fixture_sha256"] != sha256(source.read_bytes()):
                issues.append(f"input fixture changed: {name}")
            if name in EDIT_SCENARIOS:
                scenario = json.dumps(
                    EDIT_SCENARIOS[name], sort_keys=True, ensure_ascii=False, separators=(",", ":")
                ).encode("utf-8")
                if entry.get("scenario_sha256") != sha256(scenario):
                    issues.append(f"edit scenario changed: {name}")
            if source.name == "created.json":
                xml = composition_musicxml(json.loads(read_text(source))).encode("utf-8")
                if entry.get("input_sha256") != sha256(xml) or entry.get("converter_sha256") != sha256(
                    (ROOT / "tests/support/composition_fixture.py").read_bytes()
                ):
                    issues.append(f"created input or converter changed: {name}")
        return issues
    except (OSError, ValueError, KeyError, TypeError) as exc:
        return [f"invalid input provenance: {exc}"]

def compare_trees(
    golden_root: Path,
    actual_root: Path,
    allow: list[dict],
    output: Path,
    platform: str,
) -> int:
    golden_index = json.loads(read_text(golden_root / "index.json"))
    counts = {
        key: 0
        for key in (
            "identical",
            "allowed",
            "different",
            "missing",
            "extra",
            "goldenError",
            "extraFixture",
            "index",
        )
    }
    report = {
        "golden": str(golden_root),
        "actual": str(actual_root),
        "fixtures": {},
        "stale": [],
        "extraFixtures": [],
        "indexIssues": [],
        "counts": counts,
        "status": "pass",
    }
    matched_allow: set[int] = set()
    diff_root = output / "diff"
    if diff_root.exists():
        shutil.rmtree(diff_root)

    report["indexIssues"].extend(verify_golden_index(golden_root, golden_index))
    report["indexIssues"].extend(verify_input_provenance())
    counts["index"] = len(report["indexIssues"])
    for issue in report["indexIssues"]:
        print(f"index    {issue}")
    actual_fixtures = {path.name for path in actual_root.iterdir() if path.is_dir()}
    for fixture in sorted(actual_fixtures - set(golden_index)):
        report["extraFixtures"].append(fixture)
        counts["extraFixture"] += 1
        print(f"extra fixture  {fixture}")

    for fixture in sorted(golden_index):
        expected = golden_index[fixture]
        golden_dir, actual_dir = golden_root / fixture, actual_root / fixture
        actual_files = (
            {path.relative_to(actual_dir).as_posix() for path in actual_dir.rglob("*") if path.is_file()}
            if actual_dir.is_dir()
            else set()
        )
        results = report["fixtures"].setdefault(fixture, {})
        for name in sorted(set(expected) | actual_files):
            golden_file = golden_dir / name
            actual_file = actual_dir / name
            golden_sha = sha256(golden_file.read_bytes()) if golden_file.is_file() else None
            actual_sha = sha256(actual_file.read_bytes()) if actual_file.is_file() else None
            golden_error = golden_sha is not None and name.endswith(".error.txt")
            golden_format = None
            if actual_sha is None:
                kind = "missing"
            elif name not in expected:
                kind = "extra"
            elif golden_sha is None:
                kind = "different"
            else:
                try:
                    same = files_equal(golden_file, actual_file, name)
                except GoldenFormatError as exc:
                    # A baseline missing required evidence is an error, not a difference an
                    # allow entry may cover: it must be recaptured.
                    same, golden_format = False, str(exc)
                kind = None if same and not golden_error else "different"
            if kind is None:
                counts["identical"] += 1
                results[name] = {"status": "identical", "sha256": actual_sha}
                continue
            if golden_format:
                counts["goldenError"] += 1
                results[name] = {
                    "status": "golden-format",
                    "kind": kind,
                    "goldenSha256": golden_sha,
                    "actualSha256": actual_sha,
                    "error": golden_format,
                }
                print(f"golden-format {fixture}/{name}: {golden_format}")
                continue

            position, entry, problem = allow_accept(allow, fixture, name, kind, actual_sha, platform)
            if entry:
                matched_allow.add(position)
                counts["allowed"] += 1
                results[name] = {
                    "status": "allowed",
                    "kind": kind,
                    "step": entry["step"],
                    "reason": entry["reason"],
                    "actualSha256": actual_sha,
                    "goldenSha256": golden_sha,
                }
                print(f"allowed  {fixture}/{name} ({kind}; {entry['step']}: {entry['reason']})")
            else:
                status = "golden-error" if golden_error else kind
                counts["goldenError" if golden_error else kind] += 1
                results[name] = {
                    "status": status,
                    "kind": kind,
                    "goldenSha256": golden_sha,
                    "actualSha256": actual_sha,
                }
                suffix = f" (allow mismatch: {problem})" if problem else ""
                label = "missing" if kind == "missing" else status
                print(f"{label:<12} {fixture}/{name} actual sha256={actual_sha or 'none'}{suffix}")
            if kind == "different" and golden_file.is_file() and actual_sha != golden_sha:
                write_diff(diff_root, fixture, name, golden_file, actual_file)

    for position, entry in enumerate(allow):
        # An entry is only expected to match on the platforms it pins.
        if position not in matched_allow and platform in entry["sha256"]:
            report["stale"].append(entry)
            print(
                f"stale    allow entry fixture={entry['fixture']} output={entry['output']} "
                f"({entry['step']}: {entry['reason']})"
            )

    failing = sum(
        counts[key]
        for key in (
            "different",
            "missing",
            "extra",
            "goldenError",
            "extraFixture",
            "index",
        )
    )
    failing += len(report["stale"])
    report["status"] = "pass" if failing == 0 else "fail"
    write_json(output / "report.json", report, indent=2)
    total = sum(
        counts[key]
        for key in (
            "identical",
            "allowed",
            "different",
            "missing",
            "extra",
            "goldenError",
        )
    )
    print(f"golden compare: {len(golden_index)} fixtures, {total} outputs")
    for key in counts:
        print(f"{key}: {counts[key]}")
    print(f"result: {'PASS' if failing == 0 else 'FAIL'}")
    return 0 if failing == 0 else 1


def command_pin(args) -> int:
    report_path = args.output / "report.json"
    if not report_path.is_file():
        raise SystemExit(f"report does not exist: {report_path}")
    report = json.loads(read_text(report_path))
    if not isinstance(report.get("fixtures"), dict):
        raise SystemExit("invalid golden report: expected fixtures mapping")
    entries = []
    for fixture, results in sorted(report["fixtures"].items()):
        for name, result in sorted(results.items()):
            if result.get("status") not in (
                "different",
                "missing",
                "extra",
                "golden-error",
            ):
                continue
            kind = result.get("kind") or result["status"]
            sha = {} if kind == "missing" else {"sha256": {args.platform: result["actualSha256"]}}
            entries.append(
                {
                    "fixture": fixture,
                    "output": name,
                    "step": "TODO",
                    "reason": "TODO",
                    "kind": kind,
                    **sha,
                }
            )
    print(json.dumps(entries, indent=1, sort_keys=True))
    return int(bool(entries))
