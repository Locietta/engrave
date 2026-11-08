"""Shared cancel score helpers."""

from pathlib import Path


def generate_cancel_score(score: Path, tempo: int = 300, *, part_count: int = 12, measure_count: int = 256):
    notes = "".join(
        f"<note><pitch><step>{step}</step><octave>4</octave></pitch><duration>120</duration><type>16th</type></note>"
        for step in "CDEFGABC" * 2
    )
    parts = []
    for p in range(1, part_count + 1):
        measures = []
        for m in range(1, measure_count + 1):
            attributes = (
                (
                    "<attributes><divisions>480</divisions><time><beats>4</beats><beat-type>4</beat-type></time>"
                    "<clef><sign>G</sign><line>2</line></clef></attributes>"
                    f"<direction><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>{tempo}</per-minute>"
                    f'</metronome></direction-type><sound tempo="{tempo}"/></direction>'
                )
                if m == 1
                else ""
            )
            measures.append(f'<measure number="{m}">{attributes}{notes}</measure>')
        parts.append(f'<part id="P{p}">{"".join(measures)}</part>')
    part_list = "".join(
        f'<score-part id="P{p}"><part-name>Part {p}</part-name><midi-instrument id="I{p}"><midi-channel>{p}</midi-channel><midi-program>1</midi-program></midi-instrument></score-part>'
        for p in range(1, part_count + 1)
    )
    score.write_text(
        f'<?xml version="1.0" encoding="UTF-8"?><score-partwise version="4.0"><part-list>{part_list}</part-list>{"".join(parts)}</score-partwise>',
        encoding="utf-8",
    )
