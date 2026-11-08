"""Cross-check the native semantic dump against the independent Python MSCX parser.

``the engine's engrave/src/score/score_semantic.cpp`` dumps a score's musical content from the in-memory
engraving DOM.  The golden tree stores that dump next to the MSCX the same score
produced (``tests/data/golden/<platform>/<fixture>/semantic.json`` and ``score.mscx``).
This script re-parses the MSCX with ``parse_mscx()`` from ``test_roundtrip.py``
and compares the two representations, so a rebase can tell a dumper change from a
format change.

Only the fields both representations expose are compared:

  parts       count, long name, staff count, staff type groups
  measures    count (first staff), time signature, start/end repeat
  events      per staff and voice: kind, tick, duration, grace kind, notes, tie
  spanners    (type, start tick, end tick) per staff, for fixed supported types
  clefs       (tick, type) per staff
  lyrics      (verse, text) per staff
  dynamics    counts per dynamic subtype, per staff
  tempos      beats per second, per staff
  metadata    meta tags except creationDate/platform
  excerpts    count and names

Grace notes need one projection decision.  MuseScore attaches every grace note to
the *following* chord (``read460/measureread.cpp`` collects them and calls
``chord->add()`` on the next normal chord), so a grace-after's DOM tick is its
parent's tick.  ``parse_mscx`` sees only the MSCX stream and records a grace-after
at the previous main event's tick.  Both placements describe the same notation,
but the ticks are not comparable, so grace events are compared by kind, grace
type, duration and notes; grace-before events (whose tick is shared) keep their
tick.  The dump lists grace-afters in ``Chord::graceNotesAfter()`` (reverse
insertion) order and parse_mscx in file order, so grace events are compared as a
multiset.  Event order within one tick is likewise not a shared field.

Usage:
  python tests/engrave_testing/check_semantic_dump.py [GOLDEN_ROOT] [--max-details N]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any

from engrave_testing.mscx import parse_mscx
from engrave_testing.paths import REPO_ROOT

# Fixed, documented set of spanner types supported across both native semantic dump
# and independent MSCX parser. Ties are compared in check_events via note "tieTo".
SUPPORTED_SPANNERS = frozenset({"HairPin", "Ottava", "Pedal", "Slur", "Volta"})

GRACE_AFTER = {"grace8after", "grace16after", "grace32after"}

# MSCX StaffType@group and Staff::staffType()->group() use the same names for the
# three groups the dumper emits; keep the mapping explicit so an unknown value is
# visible in the report instead of comparing unequal spellings silently.
STAFF_GROUPS = {
    "pitched": "pitched",
    "percussion": "percussion",
    "tablature": "tablature",
}

# parse_mscx rounds the stored <tempo> (beats per second) to four decimals, which
# can move bps*60 by up to 0.003 BPM; 100 BPM stores as 1.6667 and reads back as
# 100.002.  Allow the requested 1e-3 BPM plus that rounding.
TEMPO_TOLERANCE_BPM = 1e-3 + 60 * 0.5e-4

TAG = re.compile(r"<[^>]+>")


class FieldResult:
    """One row of the per-fixture table: a field name and its differences."""

    def __init__(self, name: str, details: list[str] | None = None):
        self.name = name
        self.details = details or []
        self.known = False

    @property
    def matched(self) -> bool:
        return not self.details or self.known


# Differences caused by parse_mscx itself, not by the dumper: {(fixture, field): reason}.
KNOWN_PARSE_LIMITS = {
    (
        "layout_elements.mscx",
        "parts.longName",
    ): "parse_mscx findtext() misses the text after rich-text tags (<longName><font .../>Piano)",
}


def plain_text(text: str) -> str:
    """Strip MuseScore rich-text markup from a name (long names can carry <font> tags)."""
    return TAG.sub("", text or "")


def staff_group(name: str) -> str:
    return STAFF_GROUPS.get(name, name)


def staff_ids(parsed: dict[str, Any]) -> list[str]:
    """MSCX Staff ids in document order; list index == dump staff index."""
    return list(parsed["measures"])


def first_staff_id(parsed: dict[str, Any]) -> str | None:
    ids = staff_ids(parsed)
    return ids[0] if ids else None


def item_label(item: tuple[Any, ...]) -> str:
    return repr(item)


def multiset_details(label: str, dump_items: Counter, parse_items: Counter, limit: int) -> list[str]:
    """Describe the first few items only one side has."""
    details = []
    for item, count in sorted((dump_items - parse_items).items(), key=lambda kv: repr(kv[0])):
        details.append(f"{label} dump only x{count}: {item_label(item)}")
        if len(details) >= limit:
            return details
    for item, count in sorted((parse_items - dump_items).items(), key=lambda kv: repr(kv[0])):
        details.append(f"{label} parse only x{count}: {item_label(item)}")
        if len(details) >= limit:
            return details
    return details


def mapping_details(label: str, dump_map: dict[Any, Any], parse_map: dict[Any, Any], limit: int) -> list[str]:
    details = []
    for key in sorted(set(dump_map) | set(parse_map)):
        if dump_map.get(key) != parse_map.get(key):
            details.append(f"{label} {key}: dump {dump_map.get(key)!r} vs parse {parse_map.get(key)!r}")
            if len(details) >= limit:
                break
    return details


def index_details(label: str, dump_values: list[Any], parse_values: list[Any], limit: int) -> list[str]:
    details = []
    for index, (dump_value, parse_value) in enumerate(zip(dump_values, parse_values)):
        if dump_value != parse_value:
            details.append(f"{label} {index}: dump {dump_value!r} vs parse {parse_value!r}")
            if len(details) >= limit:
                break
    return details


def project_dump_event(event: dict[str, Any]) -> tuple[Any, ...]:
    """Project a dumped chord/rest onto the fields parse_mscx shares."""
    kind = event["kind"]
    grace = event.get("grace", "none") if kind == "chord" else "none"
    notes = tuple(sorted((note["pitch"], note["tpc1"], "tieTo" in note) for note in event.get("notes", [])))
    # A grace-after's tick is its parent chord's tick in the DOM and the previous
    # main event's tick in parse_mscx; it is not a shared field (see module docstring).
    tick = None if grace in GRACE_AFTER else event["tick"]
    return (kind, tick, event["ticks"], grace, notes)


def project_parse_event(event: tuple[Any, ...]) -> tuple[Any, ...]:
    kind, tick, ticks, notes, grace = event[:5]
    projected_notes = tuple(sorted((note[0], note[1], note[2]) for note in notes))
    tick = None if grace in GRACE_AFTER else tick
    return (kind, tick, ticks, grace, projected_notes)


def is_grace(event: tuple[Any, ...]) -> bool:
    return event[3] != "none"


def check_metadata(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    return [
        FieldResult(
            "metadata",
            mapping_details("tag", dump.get("metadata", {}), parsed.get("metadata", {}), limit),
        )
    ]


def check_parts(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    dump_parts = dump.get("parts", [])
    parse_parts = parsed.get("parts", [])
    count = []
    if len(dump_parts) != len(parse_parts):
        count.append(f"dump {len(dump_parts)} vs parse {len(parse_parts)} parts")

    long_names, staff_counts, groups = [], [], []
    for index in range(min(len(dump_parts), len(parse_parts))):
        dump_part, parse_part = dump_parts[index], parse_parts[index]
        dump_name = plain_text(dump_part.get("longName", ""))
        parse_name = parse_part.get("longName", "")
        if dump_name != parse_name:
            long_names.append(
                f"part {index}: dump {dump_part.get('longName', '')!r} (plain {dump_name!r}) vs parse {parse_name!r}"
            )
        dump_staves = len(dump_part.get("staves", []))
        if dump_staves != parse_part.get("staffCount"):
            staff_counts.append(f"part {index}: dump {dump_staves} vs parse {parse_part.get('staffCount')}")
        dump_groups = [staff_group(staff.get("group", "")) for staff in dump_part.get("staves", [])]
        parse_groups = [staff_group(group) for group in parse_part.get("staffTypeGroups", [])]
        if dump_groups != parse_groups:
            groups.append(f"part {index}: dump {dump_groups} vs parse {parse_groups}")
    return [
        FieldResult("parts.count", count),
        FieldResult("parts.longName", long_names[:limit]),
        FieldResult("parts.staffCount", staff_counts[:limit]),
        FieldResult("parts.staffTypeGroups", groups[:limit]),
    ]


def check_measures(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    staff = first_staff_id(parsed)
    dump_measures = dump.get("measures", [])
    parse_measures = parsed["measures"].get(staff, []) if staff is not None else []
    count = []
    if len(dump_measures) != len(parse_measures):
        count.append(f"dump {len(dump_measures)} vs parse {len(parse_measures)} measures")

    # Time signatures: dump signatures at staff 0 vs the TimeSig of each MSCX measure.
    tick_to_measure = {measure["tick"]: index for index, measure in enumerate(dump_measures)}
    dump_signatures: dict[int, str] = {}
    unmapped = []
    for signature in dump.get("signatures", []):
        if signature["type"] != "TimeSig" or signature["staff"] != 0:
            continue
        index = tick_to_measure.get(signature["tick"])
        if index is None:
            unmapped.append(f"TimeSig at tick {signature['tick']} is not a measure start")
        else:
            dump_signatures[index] = signature["value"]
    parse_signatures = {
        index: measure["timesig"] for index, measure in enumerate(parse_measures) if measure.get("timesig")
    }
    timesig = mapping_details("measure", dump_signatures, parse_signatures, limit)
    timesig.extend(unmapped[: max(0, limit - len(timesig))])
    if len(dump_signatures) != len(parse_signatures):
        timesig.insert(
            0,
            f"dump {len(dump_signatures)} vs parse {len(parse_signatures)} time signatures",
        )

    start_repeat = index_details(
        "measure",
        [m["startRepeat"] for m in dump_measures],
        [m["startRepeat"] for m in parse_measures],
        limit,
    )
    end_repeat = index_details(
        "measure",
        [m["endRepeat"] for m in dump_measures],
        [m["endRepeat"] for m in parse_measures],
        limit,
    )
    return [
        FieldResult("measures.count", count),
        FieldResult("measures.timeSignature", timesig),
        FieldResult("measures.startRepeat", start_repeat),
        FieldResult("measures.endRepeat", end_repeat),
    ]


def check_events(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    ids = staff_ids(parsed)
    results = []
    for staff_index, dump_voices in enumerate(dump.get("events", [])):
        details = []
        parse_voices = parsed["events"].get(ids[staff_index], {}) if staff_index < len(ids) else {}
        for voice_index, dump_voice in enumerate(dump_voices):
            # parse_mscx skips <MeasureRepeat> (a Rest subclass in the DOM).
            dump_events = [project_dump_event(event) for event in dump_voice if event["kind"] != "measureRepeat"]
            parse_events = [project_parse_event(event) for event in parse_voices.get(voice_index, [])]
            dump_main = sorted(event for event in dump_events if not is_grace(event))
            parse_main = sorted(event for event in parse_events if not is_grace(event))
            dump_graces = Counter(event for event in dump_events if is_grace(event))
            parse_graces = Counter(event for event in parse_events if is_grace(event))
            label = f"staff {staff_index} voice {voice_index}"
            if dump_main != parse_main:
                details.append(f"{label} main events: dump {len(dump_main)} vs parse {len(parse_main)}")
                if len(details) < limit:
                    details.extend(
                        multiset_details(
                            label,
                            Counter(dump_main),
                            Counter(parse_main),
                            limit - len(details),
                        )
                    )
            if dump_graces != parse_graces:
                details.append(
                    f"{label} grace events: dump {sum(dump_graces.values())} vs parse {sum(parse_graces.values())}"
                )
                if len(details) < limit:
                    details.extend(multiset_details(label, dump_graces, parse_graces, limit - len(details)))
        results.append(FieldResult(f"events[staff {staff_index}]", details[:limit]))
    return results


def check_spanners(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    ids = staff_ids(parsed)
    results = []
    for staff_index in range(len(dump.get("events", []))):
        dump_spanners = Counter(
            (spanner["type"], spanner["tick"], spanner["tick2"])
            for spanner in dump.get("spanners", [])
            if spanner["track"] // 4 == staff_index
        )
        parse_items = parsed["spanners"].get(ids[staff_index], []) if staff_index < len(ids) else []
        # parse_mscx records ties as Spanner entries; the dumper records them per note
        # (note "tieTo"), which the events check already compares.
        parse_spanners = Counter((item[0], item[2], item[3]) for item in parse_items if item[0] != "Tie")
        details: list[str] = []
        dump_unsupported = sorted({item[0] for item in dump_spanners if item[0] not in SUPPORTED_SPANNERS})
        parse_unsupported = sorted({item[0] for item in parse_spanners if item[0] not in SUPPORTED_SPANNERS})
        for u in dump_unsupported:
            if len(details) < limit:
                details.append(f"staff {staff_index}: unsupported dump spanner type {u!r}")
        for u in parse_unsupported:
            if len(details) < limit:
                details.append(f"staff {staff_index}: unsupported parse spanner type {u!r}")
        dump_supported = Counter(
            {item: count for item, count in dump_spanners.items() if item[0] in SUPPORTED_SPANNERS}
        )
        parse_supported = Counter(
            {item: count for item, count in parse_spanners.items() if item[0] in SUPPORTED_SPANNERS}
        )
        if len(details) < limit:
            details.extend(
                multiset_details(
                    f"staff {staff_index}",
                    dump_supported,
                    parse_supported,
                    limit - len(details),
                )
            )
        results.append(FieldResult(f"spanners[staff {staff_index}]", details[:limit]))
    return results


def check_clefs(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    ids = staff_ids(parsed)
    results = []
    for staff_index in range(len(dump.get("events", []))):
        dump_clefs = Counter(
            (signature["tick"], signature["value"])
            for signature in dump.get("signatures", [])
            if signature["type"] == "Clef" and signature["staff"] == staff_index
        )
        parse_clefs = Counter(
            tuple(clef) for clef in (parsed["clefs"].get(ids[staff_index], []) if staff_index < len(ids) else [])
        )
        details = multiset_details(f"staff {staff_index}", dump_clefs, parse_clefs, limit)
        results.append(FieldResult(f"clefs[staff {staff_index}]", details))
    return results


def check_lyrics(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    ids = staff_ids(parsed)
    results = []
    for staff_index, voices in enumerate(dump.get("events", [])):
        dump_lyrics = Counter(
            (verse, text) for voice in voices for event in voice for verse, text in event.get("lyrics", [])
        )
        annotations = parsed["annotations"].get(ids[staff_index], {}) if staff_index < len(ids) else {}
        parse_lyrics = Counter(tuple(item) for item in annotations.get("lyrics", []))
        details = multiset_details(f"staff {staff_index}", dump_lyrics, parse_lyrics, limit)
        results.append(FieldResult(f"lyrics[staff {staff_index}]", details))
    return results


def check_dynamics(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    ids = staff_ids(parsed)
    results = []
    for staff_index in range(len(dump.get("events", []))):
        dump_dynamics = Counter(
            annotation.get("dynamic")
            for annotation in dump.get("annotations", [])
            if annotation["type"] == "Dynamic" and annotation["track"] // 4 == staff_index
        )
        annotations = parsed["annotations"].get(ids[staff_index], {}) if staff_index < len(ids) else {}
        parse_dynamics = Counter(annotations.get("dynamics", {}))
        details = multiset_details(f"staff {staff_index}", dump_dynamics, parse_dynamics, limit)
        results.append(FieldResult(f"dynamics[staff {staff_index}]", details))
    return results


def check_tempos(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    ids = staff_ids(parsed)
    results = []
    for staff_index in range(len(dump.get("events", []))):
        dump_bpm = sorted(
            annotation["bpm"]
            for annotation in dump.get("annotations", [])
            if annotation["type"] == "Tempo" and annotation["track"] // 4 == staff_index and "bpm" in annotation
        )
        annotations = parsed["annotations"].get(ids[staff_index], {}) if staff_index < len(ids) else {}
        # parse_mscx stores beats per second; the dumper stores quarter-note BPM.
        parse_bpm = sorted(round(tempo * 60, 6) for tempo in annotations.get("tempos", []))
        details = []
        if len(dump_bpm) != len(parse_bpm):
            details.append(f"staff {staff_index}: dump {len(dump_bpm)} vs parse {len(parse_bpm)} tempos")
        for dump_value, parse_value in zip(dump_bpm, parse_bpm):
            if abs(dump_value - parse_value) > TEMPO_TOLERANCE_BPM:
                details.append(f"staff {staff_index}: dump {dump_value!r} vs parse {parse_value!r} BPM")
                if len(details) >= limit:
                    break
        results.append(FieldResult(f"tempos[staff {staff_index}]", details[:limit]))
    return results


def check_excerpts(dump: dict[str, Any], parsed: dict[str, Any], limit: int) -> list[FieldResult]:
    dump_names = [excerpt.get("name", "") for excerpt in dump.get("excerpts", [])]
    parse_names = [excerpt.get("title", "") for excerpt in parsed.get("excerpts", [])]
    if not parse_names:
        # The MSCX export holds the master score only (engrave/src/runtime/score_io.cpp); the
        # dump comes from the MSCZ, which also stores the excerpts. Count the captured
        # excerpt-N folders instead; without any, the field has no MSCX reference.
        folders = parsed.get("excerptFolders")
        if not folders:
            return [FieldResult("excerpts", [])]
        if not dump_names and parsed.get("sourceExcerpts") == folders:
            # Generated after the master MSCZ was saved (golden.py GENERATE_EXCERPTS);
            # semantic-source.json is dumped with them.
            return [FieldResult("excerpts", [])]
        if len(dump_names) != folders:
            return [FieldResult("excerpts", [f"dump {len(dump_names)} vs {folders} excerpt folders"])]
        return [FieldResult("excerpts", [])]
    details = []
    if len(dump_names) != len(parse_names):
        details.append(f"dump {len(dump_names)} vs parse {len(parse_names)} excerpts")
    for dump_name, parse_name in zip(dump_names, parse_names):
        if dump_name != parse_name:
            details.append(f"dump {dump_name!r} vs parse {parse_name!r}")
            if len(details) >= limit:
                break
    return [FieldResult("excerpts", details[:limit])]


CHECKS = (
    check_metadata,
    check_parts,
    check_measures,
    check_events,
    check_spanners,
    check_clefs,
    check_lyrics,
    check_dynamics,
    check_tempos,
    check_excerpts,
)

# Columns of the per-fixture summary table.  A column is "ok" only when every
# FieldResult whose name starts with the prefix matched.
COLUMNS = (
    ("parts", "parts"),
    ("measures", "measures"),
    ("events", "events"),
    ("spanners", "spanners"),
    ("clefs", "clefs"),
    ("lyrics", "lyrics"),
    ("dynamics", "dynamics"),
    ("tempos", "tempos"),
    ("metadata", "metadata"),
    ("excerpts", "excerpts"),
)


def check_fixture(fixture: Path, limit: int) -> list[FieldResult]:
    dump = json.loads((fixture / "semantic.json").read_text(encoding="utf-8"))
    parsed = parse_mscx((fixture / "score.mscx").read_text(encoding="utf-8"))
    parsed["excerptFolders"] = sum(1 for path in fixture.glob("excerpt-*") if path.is_dir())
    source = fixture / "semantic-source.json"
    if source.is_file():
        parsed["sourceExcerpts"] = len(json.loads(source.read_text(encoding="utf-8")).get("excerpts", []))
    results = []
    for check in CHECKS:
        results.extend(check(dump, parsed, limit))
    for result in results:
        if not result.matched and (fixture.name, result.name) in KNOWN_PARSE_LIMITS:
            result.details = [f"known: {KNOWN_PARSE_LIMITS[(fixture.name, result.name)]}"]
            result.known = True
    return results


def column_status(results: list[FieldResult], prefix: str) -> str:
    selected = [result for result in results if result.name.startswith(prefix)]
    if not all(result.matched for result in selected):
        return "FAIL"
    return "known" if any(result.known for result in selected) else "ok"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root",
        nargs="?",
        type=Path,
        default=REPO_ROOT / "tests/data/golden/windows",
        help="Golden root containing one directory per fixture",
    )
    parser.add_argument(
        "--max-details",
        type=int,
        default=3,
        help="First differing items shown per field (default 3)",
    )
    args = parser.parse_args()

    root = args.root
    if not root.is_dir():
        print(f"error: {root} is not a directory", file=sys.stderr)
        return 2

    header = f"{'fixture':<28}" + "".join(f"{label:>10}" for _, label in COLUMNS)
    print(header)
    print("-" * len(header))

    checked = skipped = mismatched = 0
    details: list[tuple[str, FieldResult]] = []
    for fixture in sorted(path for path in root.iterdir() if path.is_dir()):
        missing = [name for name in ("score.mscx", "semantic.json") if not (fixture / name).is_file()]
        if missing:
            skipped += 1
            print(f"{fixture.name}: skipped (missing {', '.join(missing)})")
            continue
        checked += 1
        results = check_fixture(fixture, args.max_details)
        failed = [result for result in results if not result.matched]
        mismatched += len(failed)
        print(f"{fixture.name:<28}" + "".join(f"{column_status(results, prefix):>10}" for prefix, _ in COLUMNS))
        details.extend((fixture.name, result) for result in results if result.details)

    if details:
        print("\nDifferences (first items only):")
        for fixture_name, result in details:
            print(f"  {fixture_name} {result.name}:")
            for detail in result.details:
                print(f"      {detail}")

    print(f"\n{checked} fixture(s) checked, {mismatched} field(s) mismatched, {skipped} skipped")
    return 0 if (checked > 0 and mismatched == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
