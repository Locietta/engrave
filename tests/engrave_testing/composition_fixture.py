"""Translate the saved v1 composition fixture into an equivalent MusicXML input.

This is test data preparation, not a public replacement for v1 score_create.
The supported shapes are the fixed, valid historical acceptance fixtures.
"""

from __future__ import annotations

from xml.sax.saxutils import escape


def composition_musicxml(score: dict) -> str:
    def text(value):
        return escape(str(value), {'"': "&quot;", "'": "&apos;"})

    types = {120: "16th", 240: "eighth", 480: "quarter", 960: "half", 1920: "whole"}
    chromatic = [
        ("C", 0),
        ("C", 1),
        ("D", 0),
        ("D", 1),
        ("E", 0),
        ("F", 0),
        ("F", 1),
        ("G", 0),
        ("G", 1),
        ("A", 0),
        ("A", 1),
        ("B", 0),
    ]
    parts = score["parts"]
    tempo = score.get("tempo", 100)
    xml = [
        '<?xml version="1.0" encoding="UTF-8"?>\n<score-partwise version="4.0">\n',
        f"  <work>\n    <work-title>{text(score.get('title', 'Untitled'))}</work-title>\n  </work>\n",
        "  <part-list>\n",
    ]
    for index, part in enumerate(parts):
        pid = f"P{index + 1}"
        name = text(part["name"])
        channel = index + 1 if index < 9 else index + 2
        xml.append(
            f'<score-part id="{pid}"><part-name>{name}</part-name>'
            f'<score-instrument id="{pid}-I1"><instrument-name>{name}</instrument-name></score-instrument>'
            f'<midi-instrument id="{pid}-I1"><midi-channel>{channel}</midi-channel>'
            f"<midi-program>{part['midiProgram']}</midi-program></midi-instrument></score-part>\n"
        )
    xml.append("</part-list>\n")
    for index, part in enumerate(parts):
        xml.append(f'<part id="P{index + 1}">\n')
        for number, measure in enumerate(part["measures"], 1):
            assert sum(note["ticks"] for note in measure["notes"]) == 1920
            xml.append(f'<measure number="{number}">\n')
            if number == 1:
                xml.append(
                    "<attributes><divisions>480</divisions><time><beats>4</beats><beat-type>4</beat-type></time>"
                    "<clef><sign>G</sign><line>2</line></clef></attributes>"
                )
                if index == 0:
                    xml.append(
                        "<direction><direction-type><metronome><beat-unit>quarter</beat-unit>"
                        f"<per-minute>{tempo}</per-minute></metronome></direction-type>"
                        f'<sound tempo="{tempo}"/></direction>'
                    )
            for note in measure["notes"]:
                pitches, ticks = note["pitches"], note["ticks"]
                assert ticks in types
                for chord_index, pitch in enumerate(pitches or [None]):
                    xml.append("<note>" + ("<chord/>" if chord_index else ""))
                    if pitch is None:
                        xml.append("<rest/>")
                    else:
                        assert 0 <= pitch <= 127
                        step, alter = chromatic[pitch % 12]
                        xml.append(
                            f"<pitch><step>{step}</step>"
                            + ("<alter>1</alter>" if alter else "")
                            + f"<octave>{pitch // 12 - 1}</octave></pitch>"
                        )
                    xml.append(f"<duration>{ticks}</duration><voice>1</voice><type>{types[ticks]}</type></note>\n")
            xml.append("</measure>\n")
        xml.append("</part>\n")
    xml.append("</score-partwise>\n")
    return "".join(xml)
