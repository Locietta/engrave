#!/usr/bin/env python3
"""Generate a deterministic dense orchestral MusicXML benchmark score.

The roadmap step 5 performance budget was originally measured on MuseScore's
``share/templates/08-Orchestral/02-Symphony_Orchestra/02-Symphony_Orchestra.mscx``.
That template contains no chords, so the renderer only had to synthesize rests
and the measurement did not exercise note generation or synthesis. This module
writes a real workload instead:

* 24 standard orchestral parts (woodwinds, brass, timpani, strings, harp,
  piano, celesta, glockenspiel and choir);
* 150 measures of 4/4 at 120 BPM = exactly 300 s (5 audio minutes);
* continuous eighth notes, with three-note chords on the strong beats of the
  string, keyboard and harp parts;
* dynamic changes (mf, pp, ff, mp, mf) every 32 measures.

Output is a pure function of the constants below: no randomness, timestamps or
locale-dependent formatting. The default layout is about 4 MiB, below the
8 MiB score limit enforced by ``engine_audio_probe``. Tests generate the file
into a temporary directory and delete it, so no multi-megabyte fixture is
committed.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


import argparse
from pathlib import Path

DIVISIONS = 480  # MIDI ticks per quarter note
MEASURES = 150  # 150 * 4 beats at 120 BPM = 300 s
TEMPO_BPM = 120
EIGHTH = DIVISIONS // 2  # 240 ticks

# (part name, abbreviation, MusicXML midi-program (1-based), base MIDI note, clef sign)
PARTS: tuple[tuple[str, str, int, int, str], ...] = (
    ("Piccolo", "Picc", 73, 84, "G"),
    ("Flute", "Fl", 74, 72, "G"),
    ("Oboe", "Ob", 69, 72, "G"),
    ("English Horn", "EH", 70, 65, "G"),
    ("Clarinet", "Cl", 72, 67, "G"),
    ("Bass Clarinet", "BCl", 72, 50, "F"),
    ("Bassoon", "Bsn", 71, 48, "F"),
    ("Contrabassoon", "Cbsn", 71, 36, "F"),
    ("Horn", "Hn", 61, 53, "G"),
    ("Trumpet", "Tpt", 57, 67, "G"),
    ("Trombone", "Tbn", 58, 48, "F"),
    ("Bass Trombone", "BTbn", 58, 41, "F"),
    ("Tuba", "Tba", 59, 36, "F"),
    ("Timpani", "Timp", 48, 45, "F"),
    ("Violin", "Vln", 41, 76, "G"),
    ("Violin", "Vln", 41, 69, "G"),
    ("Viola", "Vla", 42, 62, "G"),
    ("Violoncello", "Vc", 43, 52, "F"),
    ("Contrabass", "Cb", 44, 40, "F"),
    ("Harp", "Hp", 47, 60, "G"),
    ("Piano", "Pno", 1, 60, "G"),
    ("Celesta", "Cel", 9, 72, "G"),
    ("Glockenspiel", "Glock", 10, 84, "G"),
    ("Choir", "Chr", 53, 60, "G"),
)

# Parts 14..21 (Violin I, Violin II, Viola, Violoncello, Contrabass, Harp,
# Piano, Celesta) play chords on beats 1 and 3.
CHORD_PARTS = frozenset(range(14, 22))

# Scale-degree offsets, in semitones, cycled by (measure, eighth).
PATTERN = (0, 2, 4, 7, 4, 2, 0, -3)

# (first measure, dynamic name, MusicXML sound-dynamics value). MuseScore maps
# these to note velocities/expression in the PlaybackModel.
DYNAMICS = (
    (0, "mf", 80),
    (32, "pp", 33),
    (64, "ff", 96),
    (96, "mp", 64),
    (128, "mf", 80),
)

_CHROMATIC = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")


def _pitch_xml(note: int) -> str:
    octave = note // 12 - 1
    return (
        f"<pitch><step>{_CHROMATIC[note % 12][0]}</step>"
        + ("<alter>1</alter>" if _CHROMATIC[note % 12].endswith("#") else "")
        + f"<octave>{octave}</octave></pitch>"
    )


def _note_xml(note: int, duration: int, chord: bool, note_type: str) -> str:
    return (
        f"<note>{'<chord/>' if chord else ''}{_pitch_xml(note)}"
        f"<duration>{duration}</duration><voice>1</voice>"
        f"<type>{note_type}</type></note>"
    )


def _direction_xml(measure: int, dynamics: list[tuple[int, str, int]]) -> str:
    for first_measure, name, value in dynamics:
        if measure == first_measure:
            break
    else:
        return ""
    return (
        f'<direction placement="below"><direction-type><dynamics><{name}/>'
        f'</dynamics></direction-type><sound dynamics="{value}"/></direction>'
    )


def generate_dense_score(output_path: Path | str) -> dict:
    """Write the score and return its path, size and musical dimensions."""
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" '
        '"http://www.musicxml.org/dtds/partwise.dtd">',
        '<score-partwise version="4.0">',
        "  <work><work-title>Dense Orchestral Benchmark</work-title></work>",
        "  <movement-title>Dense Orchestral Benchmark</movement-title>",
        '  <identification><creator type="composer">musescore-mcp</creator></identification>',
        "  <part-list>",
    ]
    for index, (name, abbreviation, program, _base, _clef) in enumerate(PARTS, start=1):
        instrument = f"P{index}-I1"
        lines.append(
            f'    <score-part id="P{index}"><part-name>{name}</part-name>'
            f"<part-abbreviation>{abbreviation}</part-abbreviation>"
            f'<score-instrument id="{instrument}"><instrument-name>{name}</instrument-name>'
            f"<instrument-abbreviation>{abbreviation}</instrument-abbreviation></score-instrument>"
            f'<midi-instrument id="{instrument}"><midi-channel>{1 + (index - 1) % 16}</midi-channel>'
            f"<midi-program>{program}</midi-program></midi-instrument></score-part>"
        )
    lines.append("  </part-list>")

    note_count = 0
    for index, (name, _abbreviation, _program, base, clef) in enumerate(PARTS, start=1):
        lines.append(f'  <part id="P{index}">')
        for measure in range(MEASURES):
            lines.append(f'    <measure number="{measure + 1}">')
            if measure == 0:
                if index == 1:
                    lines.append(
                        '      <direction placement="above"><direction-type><metronome>'
                        f"<beat-unit>quarter</beat-unit><per-minute>{TEMPO_BPM}</per-minute>"
                        f'</metronome></direction-type><sound tempo="{TEMPO_BPM}"/></direction>'
                    )
                lines.append(
                    "      <attributes>"
                    f"<divisions>{DIVISIONS}</divisions><key><fifths>0</fifths></key>"
                    "<time><beats>4</beats><beat-type>4</beat-type></time>"
                    f"<clef><sign>{clef}</sign><line>{2 if clef == 'G' else 4}</line></clef>"
                    "</attributes>"
                )
            direction = _direction_xml(measure, list(DYNAMICS))
            if direction:
                lines.append(f"      {direction}")
            for eighth in range(8):
                step = PATTERN[(measure * 8 + eighth) % len(PATTERN)]
                sounding = min(108, max(21, base + step))
                if index - 1 in CHORD_PARTS and eighth in (0, 4):
                    for interval in (0, 4, 7):
                        lines.append(
                            "      "
                            + _note_xml(
                                min(108, sounding + interval),
                                EIGHTH,
                                interval != 0,
                                "eighth",
                            )
                        )
                        note_count += 1
                else:
                    lines.append("      " + _note_xml(sounding, EIGHTH, False, "eighth"))
                    note_count += 1
            lines.append("    </measure>")
        lines.append("  </part>")
    lines.append("</score-partwise>")
    text = "\n".join(lines) + "\n"
    output.write_text(text, encoding="utf-8", newline="\n")

    play_time = MEASURES * 4 * 60.0 / TEMPO_BPM
    return {
        "path": str(output),
        "bytes": len(text.encode("utf-8")),
        "parts": len(PARTS),
        "measures": MEASURES,
        "notes": note_count,
        "play_time_secs": play_time,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", type=Path, help="Destination .musicxml path")
    args = parser.parse_args()
    info = generate_dense_score(args.output)
    print(
        f"Wrote {info['path']}: {info['bytes']} bytes, {info['parts']} parts, "
        f"{info['measures']} measures, {info['notes']} notes, "
        f"{info['play_time_secs']:.0f}s of music"
    )


if __name__ == "__main__":
    main()
