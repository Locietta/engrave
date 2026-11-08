"""Deterministic playback identity corpus; generators stay independent of scripts."""

from pathlib import Path

from engrave_testing import golden
from engrave_testing.cancel_score import generate_cancel_score
from engrave_testing.paths import REPO_ROOT

FOCUSED = REPO_ROOT / "tests/data/playback_dod"


def note(markings="", prefix="", chord=False):
    return (
        "<note>"
        + prefix
        + ("<chord/>" if chord else "")
        + "<pitch><step>C</step><octave>4</octave></pitch>"
        + ("" if "<grace" in prefix else "<duration>480</duration>")
        + "<type>quarter</type><notations>"
        + markings
        + "</notations></note>"
    )


def score(body, program=1):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n<score-partwise version="4.0">'
        '<part-list><score-part id="P1"><part-name>Playback case</part-name>'
        '<score-instrument id="I1"><instrument-name>Playback case</instrument-name></score-instrument>'
        f'<midi-instrument id="I1"><midi-channel>1</midi-channel><midi-program>{program}'
        '</midi-program></midi-instrument></score-part></part-list><part id="P1">'
        '<measure number="1"><attributes><divisions>480</divisions><time><beats>4</beats>'
        "<beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>"
        + body
        + "</measure></part></score-partwise>\n"
    )


def generate_focused(destination=FOCUSED):
    destination.mkdir(parents=True, exist_ok=True)
    cases = {
        "graces": note(prefix='<grace slash="yes"/>')
        + note(prefix="<grace/>")
        + note()
        + note(prefix='<grace steal-time-previous="20"/>')
        + note(),
        "arpeggio_ornament": note("<arpeggiate/><ornaments><trill-mark/></ornaments>")
        + note(chord=True)
        + note(chord=True),
        "tremolo_ornament": note('<ornaments><tremolo type="single">3</tremolo><trill-mark/></ornaments>'),
        "harmonic_distortion": "<direction><direction-type><words>distortion</words></direction-type></direction>"
        + note("<technical><harmonic><natural/></harmonic></technical>"),
        "mute_pizzicato": "<direction><direction-type><words>con sord., pizz.</words></direction-type></direction>"
        + note(),
        "pedals": '<direction><direction-type><pedal type="start" number="1"/></direction-type></direction>'
        '<direction><direction-type><pedal type="sostenuto" number="2"/></direction-type></direction>'
        + note()
        + '<direction><direction-type><pedal type="stop" number="1"/></direction-type></direction>',
        "glissando": note('<glissando type="start" number="1"/>') + note('<glissando type="stop" number="1"/>'),
        "repeat_tie": '<barline location="left"><repeat direction="forward"/></barline>'
        + note('<tied type="start"/>')
        + note('<tied type="stop"/>')
        + '<barline location="right"><repeat direction="backward"/></barline>',
        "cold_lyrics": note().replace("<notations>", "<lyric><text>la</text></lyric><notations>"),
        "huge_bucket": note() + note(chord=True) * 2047,
    }
    for name, body in cases.items():
        program = 27 if name == "harmonic_distortion" else 41 if name == "mute_pizzicato" else 1
        (destination / f"{name}.musicxml").write_text(score(body, program), encoding="utf-8", newline="\n")

    # Twenty staff layers in one instrument cross the production 16-channel limit.
    body = "<backup><duration>480</duration></backup>".join(
        note().replace("<notations>", f"<staff>{staff}</staff><notations>") for staff in range(1, 21)
    )
    xml = score(body).replace("<clef>", "<staves>20</staves><clef>", 1)
    (destination / "channel_exhaustion.musicxml").write_text(xml, encoding="utf-8", newline="\n")


def corpus(generated: Path):
    generated.mkdir(parents=True, exist_ok=True)
    dense = generated / "dense.musicxml"
    generate_cancel_score(dense)
    huge = generated / "huge_track.musicxml"
    huge.write_text(
        score(note().replace("<duration>480</duration>", "<duration>1</duration>") * 32768),
        encoding="utf-8",
        newline="\n",
    )
    sources = golden.fixture_files() + golden.mnx_fixture_files()
    cases = [("audio/" + p.name, p, []) for p in sources]
    playback = sorted((REPO_ROOT / "src/engraving/tests/playback").rglob("*.mscx"))
    cases += [("playback/" + p.parent.name + "/" + p.stem, p, []) for p in playback]
    cases += [("focused/" + p.stem, p, []) for p in sorted(FOCUSED.glob("*.musicxml"))]
    cases += [("dense", dense, []), ("huge_track", huge, [])]
    options_score = FOCUSED / "repeat_tie.musicxml"
    cases += [
        ("options/" + name, options_score, args)
        for name, args in (
            ("no_repeats", ["--no-repeats"]),
            ("no_reverb", ["--no-reverb"]),
            ("muted", ["--mute-part", "0"]),
            ("excerpt", ["--excerpt", "0"]),
        )
    ]
    return cases
