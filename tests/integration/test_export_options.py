"""Independent engine validation helpers extracted from the host tests."""
from __future__ import annotations

import sys

from pathlib import Path

import argparse

import asyncio

import json

import xml.etree.ElementTree as ET

from fractions import Fraction

from pathlib import Path

from support.paths import REPO_ROOT

def canonical_notes(path_or_xml: Path | str) -> tuple[list, list]:
    """Parse MusicXML and return sorted canonical notes and measure counts."""
    if isinstance(path_or_xml, Path):
        root = ET.parse(path_or_xml).getroot()
    elif "<" in path_or_xml:
        root = ET.fromstring(path_or_xml)
    else:
        root = ET.parse(path_or_xml).getroot()

    events = []
    measures = []
    for part_index, part in enumerate(root.findall("part")):
        divisions = 1
        part_measures = part.findall("measure")
        measures.append(len(part_measures))
        for measure_index, measure in enumerate(part_measures):
            cursor = Fraction(0)
            previous_start = Fraction(0)
            for element in measure:
                if element.tag == "attributes":
                    divisions = int(element.findtext("divisions", str(divisions)))
                elif element.tag in ("backup", "forward"):
                    length = Fraction(int(element.findtext("duration", "0")), divisions)
                    cursor += length if element.tag == "forward" else -length
                elif element.tag == "note":
                    duration = Fraction(int(element.findtext("duration", "0")), divisions)
                    chord = element.find("chord") is not None
                    start = previous_start if chord else cursor
                    if not chord:
                        previous_start = cursor
                        cursor += duration
                    pitch = element.find("pitch")
                    value = (
                        "rest"
                        if pitch is None
                        else ":".join(
                            pitch.findtext(field, default)
                            for field, default in (
                                ("step", ""),
                                ("alter", "0"),
                                ("octave", ""),
                            )
                        )
                    )
                    ties = tuple(sorted(tie.get("type") for tie in element.findall("tie")))
                    events.append(
                        (
                            part_index,
                            measure_index,
                            str(start),
                            str(duration),
                            element.findtext("voice", "1"),
                            element.findtext("staff", "1"),
                            value,
                            ties,
                        )
                    )
    if not events:
        raise ValueError("MusicXML contains no notes")
    return sorted(events), measures

def parse_midi_file(data: bytes) -> dict:
    """Parse Standard MIDI File (format 0/1) tracks and events."""
    if not data.startswith(b"MThd"):
        raise ValueError("Invalid MIDI header")
    header_len = int.from_bytes(data[4:8], "big")
    midi_format = int.from_bytes(data[8:10], "big")
    num_tracks = int.from_bytes(data[10:12], "big")
    division = int.from_bytes(data[12:14], "big")

    pos = 8 + header_len
    tracks = []

    for _ in range(num_tracks):
        if data[pos : pos + 4] != b"MTrk":
            raise ValueError(f"Expected MTrk at byte {pos}")
        track_len = int.from_bytes(data[pos + 4 : pos + 8], "big")
        track_end = pos + 8 + track_len
        track_data = data[pos + 8 : track_end]
        pos = track_end

        events = []
        tpos = 0
        current_tick = 0
        running_status = None

        while tpos < len(track_data):
            # VLQ delta
            delta = 0
            while tpos < len(track_data):
                b = track_data[tpos]
                tpos += 1
                delta = (delta << 7) | (b & 0x7F)
                if not (b & 0x80):
                    break
            current_tick += delta

            if tpos >= len(track_data):
                break

            lead_byte = track_data[tpos]
            if lead_byte == 0xFF:
                tpos += 1
                meta_type = track_data[tpos]
                tpos += 1
                meta_len = 0
                while tpos < len(track_data):
                    b = track_data[tpos]
                    tpos += 1
                    meta_len = (meta_len << 7) | (b & 0x7F)
                    if not (b & 0x80):
                        break
                meta_data = track_data[tpos : tpos + meta_len]
                tpos += meta_len
                events.append(
                    {
                        "type": "meta",
                        "tick": current_tick,
                        "meta_type": meta_type,
                        "data": meta_data,
                    }
                )
                running_status = None
                if meta_type == 0x2F:
                    break
            elif lead_byte in (0xF0, 0xF7):
                tpos += 1
                sysex_len = 0
                while tpos < len(track_data):
                    b = track_data[tpos]
                    tpos += 1
                    sysex_len = (sysex_len << 7) | (b & 0x7F)
                    if not (b & 0x80):
                        break
                tpos += sysex_len
                running_status = None
            else:
                if lead_byte & 0x80:
                    status = lead_byte
                    tpos += 1
                else:
                    status = running_status
                running_status = status

                cmd = status & 0xF0
                channel = status & 0x0F

                if cmd in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                    d1 = track_data[tpos]
                    d2 = track_data[tpos + 1]
                    tpos += 2
                    if cmd == 0x90:
                        ev_type = "note_on" if d2 > 0 else "note_off"
                        events.append(
                            {
                                "type": ev_type,
                                "tick": current_tick,
                                "channel": channel,
                                "pitch": d1,
                                "velocity": d2,
                            }
                        )
                    elif cmd == 0x80:
                        events.append(
                            {
                                "type": "note_off",
                                "tick": current_tick,
                                "channel": channel,
                                "pitch": d1,
                                "velocity": d2,
                            }
                        )
                    elif cmd == 0xB0:
                        events.append(
                            {
                                "type": "controller",
                                "tick": current_tick,
                                "channel": channel,
                                "controller": d1,
                                "value": d2,
                            }
                        )
                    elif cmd == 0xE0:
                        events.append(
                            {
                                "type": "pitch_bend",
                                "tick": current_tick,
                                "channel": channel,
                                "value": d1 | (d2 << 7),
                            }
                        )
                elif cmd in (0xC0, 0xD0):
                    d1 = track_data[tpos]
                    tpos += 1
                    if cmd == 0xC0:
                        events.append(
                            {
                                "type": "program",
                                "tick": current_tick,
                                "channel": channel,
                                "program": d1,
                            }
                        )
                    elif cmd == 0xD0:
                        events.append(
                            {
                                "type": "channel_aftertouch",
                                "tick": current_tick,
                                "channel": channel,
                                "pressure": d1,
                            }
                        )
                else:
                    raise ValueError(f"Unsupported MIDI status: {status}")

        tracks.append(events)

    return {"format": midi_format, "division": division, "tracks": tracks}

def count_note_on_events(midi_info: dict) -> tuple[int, list[dict]]:
    """Count note-on events with velocity > 0 across all tracks."""
    note_ons = []
    for track in midi_info["tracks"]:
        for ev in track:
            if ev["type"] == "note_on" and ev["velocity"] > 0:
                note_ons.append(ev)
    return len(note_ons), note_ons


