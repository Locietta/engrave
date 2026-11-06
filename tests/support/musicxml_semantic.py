"""Semantic parser for partwise MusicXML.

Parses MusicXML into an engine-independent normalized JSON-serializable
semantic representation, ignoring all layout and appearance details.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


import argparse
import json
import pathlib
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction
from typing import Any

STEP_OFFSETS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}

IGNORED_MEASURE_TAGS = {"print"}
HANDLED_MEASURE_TAGS = {
    "attributes",
    "backup",
    "barline",
    "direction",
    "figured-bass",
    "forward",
    "harmony",
    "note",
    "sound",
}

HANDLED_NOTE_TAGS = {
    "accidental",
    "beam",
    "chord",
    "cue",
    "dot",
    "duration",
    "grace",
    "instrument",
    "lyric",
    "notations",
    "notehead",
    "pitch",
    "play",
    "rest",
    "sound",
    "staff",
    "stem",
    "tie",
    "time-modification",
    "type",
    "unpitched",
    "voice",
}


def format_tick(tick: Fraction | int) -> int | str:
    """Format tick value as int when exact, or 'n/d' string if fractional."""
    if isinstance(tick, Fraction):
        if tick.denominator == 1:
            return int(tick.numerator)
        return f"{tick.numerator}/{tick.denominator}"
    return int(tick)


def parse_pitch(pitch_elem: ET.Element) -> dict[str, Any]:
    step = pitch_elem.findtext("step") or ""
    alter_txt = pitch_elem.findtext("alter")
    if alter_txt is not None and alter_txt != "":
        alter = float(alter_txt) if "." in alter_txt else int(alter_txt)
    else:
        alter = 0
    octave_txt = pitch_elem.findtext("octave")
    octave = int(octave_txt) if octave_txt is not None else 4

    base_midi = (octave + 1) * 12 + STEP_OFFSETS.get(step.upper(), 0)
    midi = base_midi + alter
    if isinstance(midi, float) and midi.is_integer():
        midi = int(midi)

    return {
        "alter": alter,
        "midi": midi,
        "octave": octave,
        "step": step,
    }


def parse_notations(notations_elem: ET.Element) -> dict[str, Any]:
    res: dict[str, Any] = {}

    # tied
    tied_list = []
    for t in notations_elem.findall("tied"):
        item: dict[str, Any] = {"type": t.get("type")}
        if t.get("number"):
            item["number"] = t.get("number")
        tied_list.append(item)
    if tied_list:
        res["tied"] = tied_list

    # slur
    slurs = []
    for s in notations_elem.findall("slur"):
        item = {"type": s.get("type")}
        if s.get("number"):
            item["number"] = s.get("number")
        slurs.append(item)
    if slurs:
        res["slurs"] = slurs

    # tuplet
    tuplets = []
    for t in notations_elem.findall("tuplet"):
        item = {"type": t.get("type")}
        if t.get("number"):
            item["number"] = t.get("number")
        tuplets.append(item)
    if tuplets:
        res["tuplets"] = tuplets

    # articulations
    arts = []
    for art_el in notations_elem.findall("articulations"):
        for c in art_el:
            item = {"name": c.tag}
            if c.get("placement"):
                item["placement"] = c.get("placement")
            if c.text and c.text.strip():
                item["text"] = c.text.strip()
            arts.append(item)
    if arts:
        res["articulations"] = arts

    # ornaments
    orns = []
    for orn_el in notations_elem.findall("ornaments"):
        for c in orn_el:
            item = {"name": c.tag}
            if c.tag == "tremolo":
                if c.get("type"):
                    item["tremolo_type"] = c.get("type")
                if c.text and c.text.strip():
                    item["value"] = int(c.text.strip())
            else:
                if c.get("placement"):
                    item["placement"] = c.get("placement")
                if c.text and c.text.strip():
                    item["text"] = c.text.strip()
            orns.append(item)
    if orns:
        res["ornaments"] = orns

    # technical
    techs = []
    for tech_el in notations_elem.findall("technical"):
        for c in tech_el:
            item = {"name": c.tag}
            if c.tag == "fingering" or c.tag == "string" or c.tag == "fret":
                item["text"] = (c.text or "").strip()
            elif c.tag == "bend":
                bend_alt = c.findtext("bend-alter")
                if bend_alt:
                    item["bend_alter"] = float(bend_alt) if "." in bend_alt else int(bend_alt)
                if c.get("first-beat"):
                    item["first_beat"] = c.get("first-beat")
                if c.get("last-beat"):
                    item["last_beat"] = c.get("last-beat")
                if c.find("pre-bend") is not None:
                    item["pre_bend"] = True
                if c.find("release") is not None:
                    item["release"] = True
                if c.find("with-bar") is not None:
                    item["with_bar"] = True
            elif c.tag in ("hammer-on", "pull-off"):
                if c.get("type"):
                    item["action"] = c.get("type")
                if c.get("number"):
                    item["number"] = c.get("number")
                if c.text and c.text.strip():
                    item["text"] = c.text.strip()
            else:
                for k, v in c.attrib.items():
                    item[k] = v
                if c.text and c.text.strip():
                    item["text"] = c.text.strip()
            techs.append(item)
    if techs:
        res["technical"] = techs

    # fermata
    fermatas = []
    for f in notations_elem.findall("fermata"):
        item = {
            "type": f.get("type", "upright"),
        }
        if f.text and f.text.strip():
            item["shape"] = f.text.strip()
        fermatas.append(item)
    if fermatas:
        res["fermatas"] = fermatas

    # arpeggiate
    arps = []
    for a in notations_elem.findall("arpeggiate"):
        item = {}
        if a.get("direction"):
            item["direction"] = a.get("direction")
        if a.get("number"):
            item["number"] = a.get("number")
        arps.append(item)
    if arps:
        res["arpeggiate"] = arps

    # glissando & slide
    gliss = []
    for g in notations_elem.findall("glissando"):
        item = {"kind": "glissando"}
        if g.get("type"):
            item["type"] = g.get("type")
        if g.get("number"):
            item["number"] = g.get("number")
        if g.get("line-type"):
            item["line_type"] = g.get("line-type")
        if g.text and g.text.strip():
            item["text"] = g.text.strip()
        gliss.append(item)
    for s in notations_elem.findall("slide"):
        item = {"kind": "slide"}
        if s.get("type"):
            item["type"] = s.get("type")
        if s.get("number"):
            item["number"] = s.get("number")
        if s.get("line-type"):
            item["line_type"] = s.get("line-type")
        if s.text and s.text.strip():
            item["text"] = s.text.strip()
        gliss.append(item)
    if gliss:
        res["glissando_slides"] = gliss

    # accidental
    acc = notations_elem.find("accidental")
    if acc is not None:
        res["accidental"] = {
            "bracket": acc.get("bracket") == "yes",
            "cautionary": acc.get("cautionary") == "yes",
            "editorial": acc.get("editorial") == "yes",
            "parentheses": acc.get("parentheses") == "yes",
            "value": (acc.text or "").strip(),
        }

    return res


def parse_note(
    note_elem: ET.Element,
    tick: Fraction,
    current_divisions: int,
    unhandled_set: set[str],
) -> tuple[dict[str, Any], Fraction, bool]:
    is_chord = note_elem.find("chord") is not None
    is_grace = note_elem.find("grace") is not None

    dur_elem = note_elem.find("duration")
    dur_val = int(dur_elem.text) if dur_elem is not None and dur_elem.text else 0
    if is_grace:
        dur_ticks = Fraction(0)
    else:
        dur_ticks = Fraction(dur_val * 480, current_divisions) if current_divisions else Fraction(0)

    note_dict: dict[str, Any] = {
        "chord": is_chord,
        "duration_ticks": format_tick(dur_ticks),
        "tick": format_tick(tick),
    }

    # voice
    v = note_elem.findtext("voice")
    if v is not None:
        note_dict["voice"] = int(v) if v.isdigit() else v

    # staff
    st = note_elem.findtext("staff")
    if st is not None:
        note_dict["staff"] = int(st) if st.isdigit() else 1
    else:
        note_dict["staff"] = 1

    # type
    t = note_elem.findtext("type")
    if t is not None:
        note_dict["type"] = t

    # dots
    dots = len(note_elem.findall("dot"))
    if dots > 0:
        note_dict["dots"] = dots

    # cue
    if note_elem.find("cue") is not None:
        note_dict["cue"] = True

    # grace
    grace_el = note_elem.find("grace")
    if grace_el is not None:
        g_info: dict[str, Any] = {}
        if grace_el.get("slash") is not None:
            g_info["slash"] = grace_el.get("slash") == "yes"
        if grace_el.get("steal-time-previous"):
            g_info["steal_time_previous"] = grace_el.get("steal-time-previous")
        if grace_el.get("steal-time-following"):
            g_info["steal_time_following"] = grace_el.get("steal-time-following")
        if grace_el.get("make-time"):
            g_info["make_time"] = grace_el.get("make-time")
        note_dict["grace"] = g_info

    # pitch / unpitched / rest
    pitch_el = note_elem.find("pitch")
    if pitch_el is not None:
        note_dict["pitch"] = parse_pitch(pitch_el)
    elif note_elem.find("unpitched") is not None:
        unp = note_elem.find("unpitched")
        u_obj: dict[str, Any] = {}
        ds = unp.findtext("display-step")
        if ds:
            u_obj["display_step"] = ds
        do = unp.findtext("display-octave")
        if do:
            u_obj["display_octave"] = int(do)
        note_dict["unpitched"] = u_obj
    elif note_elem.find("rest") is not None:
        r_el = note_elem.find("rest")
        r_obj: dict[str, Any] = {"measure": r_el.get("measure") == "yes"}
        ds = r_el.findtext("display-step")
        if ds:
            r_obj["display_step"] = ds
        do = r_el.findtext("display-octave")
        if do:
            r_obj["display_octave"] = int(do)
        note_dict["rest"] = r_obj

    # time-modification
    tm_el = note_elem.find("time-modification")
    if tm_el is not None:
        tm: dict[str, Any] = {
            "actual_notes": int(tm_el.findtext("actual-notes", "1")),
            "normal_notes": int(tm_el.findtext("normal-notes", "1")),
        }
        nt = tm_el.findtext("normal-type")
        if nt:
            tm["normal_type"] = nt
        nd = len(tm_el.findall("normal-dot"))
        if nd > 0:
            tm["normal_dots"] = nd
        note_dict["time_modification"] = tm

    # tie (from <tie>)
    ties = [tie.get("type") for tie in note_elem.findall("tie") if tie.get("type")]
    if ties:
        note_dict["tie"] = sorted(ties)

    # accidental (direct child)
    acc = note_elem.find("accidental")
    if acc is not None:
        note_dict["accidental"] = {
            "bracket": acc.get("bracket") == "yes",
            "cautionary": acc.get("cautionary") == "yes",
            "editorial": acc.get("editorial") == "yes",
            "parentheses": acc.get("parentheses") == "yes",
            "value": (acc.text or "").strip(),
        }

    # notehead
    nh = note_elem.find("notehead")
    if nh is not None:
        nh_obj: dict[str, Any] = {"value": (nh.text or "").strip()}
        if nh.get("filled") is not None:
            nh_obj["filled"] = nh.get("filled") == "yes"
        if nh.get("parentheses") is not None:
            nh_obj["parentheses"] = nh.get("parentheses") == "yes"
        note_dict["notehead"] = nh_obj

    # instrument
    inst = note_elem.find("instrument")
    if inst is not None and inst.get("id"):
        note_dict["instrument_id"] = inst.get("id")

    # play / sound
    play_el = note_elem.find("play")
    snd_el = note_elem.find("sound")
    if play_el is not None or snd_el is not None:
        play_obj: dict[str, Any] = {}
        if play_el is not None:
            for child in play_el:
                play_obj[child.tag] = child.text.strip() if child.text else ""
        if snd_el is not None:
            for k, v in snd_el.attrib.items():
                play_obj[k] = v
        if play_obj:
            note_dict["play"] = play_obj

    # lyrics
    lyrics_list = []
    for ly in note_elem.findall("lyric"):
        ly_num = ly.get("number", "1")
        ly_obj = {
            "number": int(ly_num) if ly_num.isdigit() else ly_num,
            "syllabic": ly.findtext("syllabic"),
            "text": ly.findtext("text"),
        }
        if ly.find("extend") is not None:
            ly_obj["extend"] = True
        lyrics_list.append(ly_obj)
    if lyrics_list:
        note_dict["lyrics"] = lyrics_list

    # notations
    notations_elem = note_elem.find("notations")
    if notations_elem is not None:
        notations = parse_notations(notations_elem)
        if notations:
            note_dict["notations"] = notations

    # check unhandled inside note
    for child in note_elem:
        if child.tag not in HANDLED_NOTE_TAGS:
            unhandled_set.add(f"note/{child.tag}")

    advance = Fraction(0) if (is_chord or is_grace) else dur_ticks
    return note_dict, advance, is_chord


def parse_direction(
    dir_elem: ET.Element,
    cursor_tick: Fraction,
    current_divisions: int,
) -> dict[str, Any]:
    offset_el = dir_elem.find("offset")
    offset_val = int(offset_el.text) if offset_el is not None and offset_el.text else 0
    offset_ticks = Fraction(offset_val * 480, current_divisions) if current_divisions else Fraction(0)

    tick = cursor_tick + offset_ticks
    res: dict[str, Any] = {
        "tick": format_tick(tick),
    }

    if dir_elem.get("placement"):
        res["placement"] = dir_elem.get("placement")

    staff_el = dir_elem.findtext("staff")
    if staff_el:
        res["staff"] = int(staff_el) if staff_el.isdigit() else staff_el

    if offset_ticks != 0:
        res["offset_ticks"] = format_tick(offset_ticks)

    words = []
    dynamics = []
    wedges = []
    octave_shifts = []
    pedals = []
    dashes_list = []
    brackets = []
    rehearsals = []
    segno = False
    coda = False
    metronome = None

    for dt in dir_elem.findall("direction-type"):
        for child in dt:
            if child.tag == "words":
                txt = (child.text or "").strip()
                if txt:
                    words.append(txt)
            elif child.tag == "dynamics":
                for d in child:
                    dynamics.append(d.tag)
            elif child.tag == "metronome":
                m_obj: dict[str, Any] = {}
                bu = child.findtext("beat-unit")
                if bu:
                    m_obj["beat_unit"] = bu
                pm = child.findtext("per-minute")
                if pm:
                    m_obj["per_minute"] = pm
                dots = len(child.findall("beat-unit-dot"))
                if dots > 0:
                    m_obj["beat_unit_dots"] = dots
                metronome = m_obj
            elif child.tag == "wedge":
                w_obj: dict[str, Any] = {"type": child.get("type")}
                if child.get("number"):
                    w_obj["number"] = child.get("number")
                if child.get("spread"):
                    w_obj["spread"] = float(child.get("spread"))
                if child.get("niente"):
                    w_obj["niente"] = child.get("niente") == "yes"
                wedges.append(w_obj)
            elif child.tag == "octave-shift":
                os_obj: dict[str, Any] = {"type": child.get("type")}
                if child.get("number"):
                    os_obj["number"] = child.get("number")
                if child.get("size"):
                    os_obj["size"] = int(child.get("size"))
                octave_shifts.append(os_obj)
            elif child.tag == "pedal":
                p_obj: dict[str, Any] = {"type": child.get("type")}
                if child.get("line"):
                    p_obj["line"] = child.get("line") == "yes"
                if child.get("number"):
                    p_obj["number"] = child.get("number")
                pedals.append(p_obj)
            elif child.tag == "dashes":
                dash_obj: dict[str, Any] = {"type": child.get("type")}
                if child.get("number"):
                    dash_obj["number"] = child.get("number")
                dashes_list.append(dash_obj)
            elif child.tag == "bracket":
                b_obj: dict[str, Any] = {"type": child.get("type")}
                if child.get("number"):
                    b_obj["number"] = child.get("number")
                if child.get("line-end"):
                    b_obj["line_end"] = child.get("line-end")
                brackets.append(b_obj)
            elif child.tag == "rehearsal":
                txt = (child.text or "").strip()
                if txt:
                    rehearsals.append(txt)
            elif child.tag == "segno":
                segno = True
            elif child.tag == "coda":
                coda = True

    if words:
        res["words"] = words
    if dynamics:
        res["dynamics"] = dynamics
    if metronome:
        res["metronome"] = metronome
    if wedges:
        res["wedges"] = wedges
    if octave_shifts:
        res["octave_shifts"] = octave_shifts
    if pedals:
        res["pedals"] = pedals
    if dashes_list:
        res["dashes"] = dashes_list
    if brackets:
        res["brackets"] = brackets
    if rehearsals:
        res["rehearsal"] = rehearsals if len(rehearsals) > 1 else rehearsals[0]
    if segno:
        res["segno"] = True
    if coda:
        res["coda"] = True

    snd = dir_elem.find("sound")
    if snd is not None or dir_elem.get("sound"):
        snd_obj: dict[str, Any] = {}
        target = snd if snd is not None else dir_elem
        for k, v in target.attrib.items():
            if k in (
                "tempo",
                "dynamics",
                "dacapo",
                "fine",
                "segno",
                "coda",
                "divisions",
            ):
                if k == "tempo" or k == "dynamics":
                    snd_obj[k] = float(v) if "." in v else int(v)
                elif k == "dacapo":
                    snd_obj[k] = v == "yes"
                else:
                    snd_obj[k] = v
        if snd_obj:
            res["sound"] = snd_obj

    return res


def parse_attributes(
    attr_elem: ET.Element,
    tick: Fraction,
) -> dict[str, Any]:
    res: dict[str, Any] = {
        "tick": format_tick(tick),
    }

    div_txt = attr_elem.findtext("divisions")
    if div_txt is not None:
        res["divisions"] = int(div_txt)

    key_el = attr_elem.find("key")
    if key_el is not None:
        k_obj: dict[str, Any] = {}
        f_txt = key_el.findtext("fifths")
        if f_txt is not None:
            k_obj["fifths"] = int(f_txt)
        m_txt = key_el.findtext("mode")
        if m_txt:
            k_obj["mode"] = m_txt
        c_txt = key_el.findtext("cancel")
        if c_txt:
            k_obj["cancel"] = int(c_txt)
        res["key"] = k_obj

    time_el = attr_elem.find("time")
    if time_el is not None:
        t_obj: dict[str, Any] = {}
        if time_el.get("symbol"):
            t_obj["symbol"] = time_el.get("symbol")
        beats = time_el.findtext("beats")
        if beats:
            t_obj["beats"] = int(beats) if beats.isdigit() else beats
        beat_type = time_el.findtext("beat-type")
        if beat_type:
            t_obj["beat_type"] = int(beat_type) if beat_type.isdigit() else beat_type
        if time_el.find("senza-misura") is not None:
            t_obj["senza_misura"] = True
        res["time"] = t_obj

    st_txt = attr_elem.findtext("staves")
    if st_txt:
        res["staves"] = int(st_txt)

    clefs = []
    for cl in attr_elem.findall("clef"):
        c_obj: dict[str, Any] = {
            "staff": int(cl.get("number", "1")),
        }
        sign = cl.findtext("sign")
        if sign:
            c_obj["sign"] = sign
        line = cl.findtext("line")
        if line:
            c_obj["line"] = int(line)
        oct_chg = cl.findtext("clef-octave-change")
        if oct_chg:
            c_obj["octave_change"] = int(oct_chg)
        clefs.append(c_obj)
    if clefs:
        res["clefs"] = clefs

    tr_el = attr_elem.find("transpose")
    if tr_el is not None:
        tr_obj: dict[str, Any] = {}
        dia = tr_el.findtext("diatonic")
        if dia:
            tr_obj["diatonic"] = int(dia)
        chrom = tr_el.findtext("chromatic")
        if chrom:
            tr_obj["chromatic"] = int(chrom)
        oct_chg = tr_el.findtext("octave-change")
        if oct_chg:
            tr_obj["octave_change"] = int(oct_chg)
        res["transpose"] = tr_obj

    sd_list = []
    for sd in attr_elem.findall("staff-details"):
        sd_obj: dict[str, Any] = {
            "staff": int(sd.get("number", "1")),
        }
        lines = sd.findtext("staff-lines")
        if lines:
            sd_obj["lines"] = int(lines)
        tunings = []
        for st in sd.findall("staff-tuning"):
            st_obj: dict[str, Any] = {"line": int(st.get("line", "1"))}
            step = st.findtext("tuning-step")
            if step:
                st_obj["step"] = step
            octave = st.findtext("tuning-octave")
            if octave:
                st_obj["octave"] = int(octave)
            alter = st.findtext("tuning-alter")
            if alter:
                st_obj["alter"] = float(alter) if "." in alter else int(alter)
            tunings.append(st_obj)
        if tunings:
            sd_obj["tunings"] = tunings
        sd_list.append(sd_obj)
    if sd_list:
        res["staff_details"] = sd_list

    inst_txt = attr_elem.findtext("instruments")
    if inst_txt:
        res["instruments"] = int(inst_txt) if inst_txt.isdigit() else inst_txt

    ms_el = attr_elem.find("measure-style")
    if ms_el is not None:
        ms_obj: dict[str, Any] = {}
        mr = ms_el.findtext("multiple-rest")
        if mr:
            ms_obj["multiple_rest"] = int(mr)
        rep = ms_el.find("measure-repeat")
        if rep is not None:
            ms_obj["measure_repeat"] = {
                "type": rep.get("type"),
                "count": int(rep.text.strip()) if rep.text and rep.text.strip().isdigit() else rep.text,
            }
        res["measure_style"] = ms_obj

    return res


def parse_barline(b_elem: ET.Element) -> dict[str, Any]:
    bar: dict[str, Any] = {
        "location": b_elem.get("location", "right"),
    }
    style = b_elem.findtext("bar-style")
    if style:
        bar["bar_style"] = style.strip()
    rep = b_elem.find("repeat")
    if rep is not None:
        r_obj: dict[str, Any] = {"direction": rep.get("direction")}
        if rep.get("times"):
            r_obj["times"] = int(rep.get("times"))
        bar["repeat"] = r_obj
    end = b_elem.find("ending")
    if end is not None:
        e_obj: dict[str, Any] = {
            "number": end.get("number"),
            "type": end.get("type"),
        }
        if end.text and end.text.strip():
            e_obj["text"] = end.text.strip()
        bar["ending"] = e_obj
    return bar


def parse_measure_sound(snd_elem: ET.Element, tick: Fraction) -> dict[str, Any]:
    snd_obj: dict[str, Any] = {
        "tick": format_tick(tick),
    }
    for k, v in snd_elem.attrib.items():
        if k in ("tempo", "dynamics", "dacapo", "fine", "segno", "coda", "divisions"):
            if k == "tempo" or k == "dynamics":
                snd_obj[k] = float(v) if "." in v else int(v)
            elif k == "dacapo":
                snd_obj[k] = v == "yes"
            else:
                snd_obj[k] = v
    return snd_obj


def parse_harmony(
    h_elem: ET.Element,
    cursor_tick: Fraction,
    current_divisions: int,
) -> dict[str, Any]:
    offset_el = h_elem.find("offset")
    offset_val = int(offset_el.text) if offset_el is not None and offset_el.text else 0
    offset_ticks = Fraction(offset_val * 480, current_divisions) if current_divisions else Fraction(0)
    tick = cursor_tick + offset_ticks

    h_obj: dict[str, Any] = {
        "tick": format_tick(tick),
    }
    root_el = h_elem.find("root")
    if root_el is not None:
        r_obj: dict[str, Any] = {
            "step": root_el.findtext("root-step", ""),
        }
        alt = root_el.findtext("root-alter")
        if alt:
            r_obj["alter"] = float(alt) if "." in alt else int(alt)
        h_obj["root"] = r_obj
    kind_el = h_elem.find("kind")
    if kind_el is not None:
        k_obj: dict[str, Any] = {
            "value": (kind_el.text or "").strip(),
        }
        if kind_el.get("text"):
            k_obj["text"] = kind_el.get("text")
        h_obj["kind"] = k_obj
    bass_el = h_elem.find("bass")
    if bass_el is not None:
        b_obj: dict[str, Any] = {
            "step": bass_el.findtext("bass-step", ""),
        }
        alt = bass_el.findtext("bass-alter")
        if alt:
            b_obj["alter"] = float(alt) if "." in alt else int(alt)
        h_obj["bass"] = b_obj
    if offset_ticks != 0:
        h_obj["offset_ticks"] = format_tick(offset_ticks)
    return h_obj


def parse_figured_bass(
    fb_elem: ET.Element,
    cursor_tick: Fraction,
    current_divisions: int,
) -> dict[str, Any]:
    offset_el = fb_elem.find("offset")
    offset_val = int(offset_el.text) if offset_el is not None and offset_el.text else 0
    offset_ticks = Fraction(offset_val * 480, current_divisions) if current_divisions else Fraction(0)
    tick = cursor_tick + offset_ticks

    fb_obj: dict[str, Any] = {
        "figures": [],
        "tick": format_tick(tick),
    }
    for fig in fb_elem.findall("figure"):
        f_item: dict[str, Any] = {}
        num = fig.findtext("figure-number")
        if num:
            f_item["number"] = num.strip()
        pfx = fig.findtext("prefix")
        if pfx:
            f_item["prefix"] = pfx.strip()
        sfx = fig.findtext("suffix")
        if sfx:
            f_item["suffix"] = sfx.strip()
        fb_obj["figures"].append(f_item)
    dur = fb_elem.findtext("duration")
    if dur:
        dur_ticks = Fraction(int(dur) * 480, current_divisions) if current_divisions else Fraction(0)
        fb_obj["duration_ticks"] = format_tick(dur_ticks)
    if offset_ticks != 0:
        fb_obj["offset_ticks"] = format_tick(offset_ticks)
    return fb_obj


def musicxml_semantic(text: str) -> dict[str, Any]:
    """Parse partwise MusicXML text into a normalized, JSON-serializable semantic dict."""
    if isinstance(text, str):
        root = ET.fromstring(text.encode("utf-8"))
    else:
        root = ET.fromstring(text)

    # work & movement title
    work_title = None
    work_elem = root.find("work")
    if work_elem is not None:
        wt = work_elem.findtext("work-title")
        if wt:
            work_title = wt.strip()

    movement_title = None
    mt = root.findtext("movement-title")
    if mt:
        movement_title = mt.strip()

    # identification: creators & rights
    creators: dict[str, list[str]] = {}
    rights_list: list[dict[str, str]] = []
    ident_elem = root.find("identification")
    if ident_elem is not None:
        for c in ident_elem.findall("creator"):
            ctype = c.get("type", "composer")
            c_text = (c.text or "").strip()
            if c_text:
                creators.setdefault(ctype, []).append(c_text)
        for r in ident_elem.findall("rights"):
            r_text = (r.text or "").strip()
            if r_text:
                r_obj = {"text": r_text}
                if r.get("type"):
                    r_obj["type"] = r.get("type")
                rights_list.append(r_obj)

    for k in list(creators.keys()):
        creators[k] = sorted(creators[k])

    # part-list
    part_list_elem = root.find("part-list")
    part_groups = []
    part_meta = []
    if part_list_elem is not None:
        for item in part_list_elem:
            if item.tag == "part-group":
                pg: dict[str, Any] = {
                    "number": int(item.get("number")) if item.get("number", "").isdigit() else item.get("number", "1"),
                    "type": item.get("type", "start"),
                }
                gs = item.findtext("group-symbol")
                if gs:
                    pg["symbol"] = gs.strip()
                gb = item.findtext("group-barline")
                if gb:
                    pg["barline"] = gb.strip()
                gn = item.findtext("group-name")
                if gn:
                    pg["name"] = gn.strip()
                part_groups.append(pg)
            elif item.tag == "score-part":
                pid = item.get("id", "")
                p_name = item.findtext("part-name")
                p_abbr = item.findtext("part-abbreviation")

                instruments: dict[str, dict[str, Any]] = {}
                for si in item.findall("score-instrument"):
                    inst_id = si.get("id", "")
                    instruments[inst_id] = {
                        "id": inst_id,
                        "name": si.findtext("instrument-name"),
                        "sound": si.findtext("instrument-sound"),
                    }
                for mi in item.findall("midi-instrument"):
                    inst_id = mi.get("id", "")
                    inst_dict = instruments.setdefault(inst_id, {"id": inst_id})
                    mc = mi.findtext("midi-channel")
                    if mc:
                        inst_dict["midi_channel"] = int(mc)
                    mp = mi.findtext("midi-program")
                    if mp:
                        inst_dict["midi_program"] = int(mp)
                    mu = mi.findtext("midi-unpitched")
                    if mu:
                        inst_dict["midi_unpitched"] = int(mu)
                    vol = mi.findtext("volume")
                    if vol:
                        inst_dict["volume"] = float(vol) if "." in vol else int(vol)
                    pan = mi.findtext("pan")
                    if pan:
                        inst_dict["pan"] = float(pan) if "." in pan else int(pan)
                for md in item.findall("midi-device"):
                    port = md.get("port")
                    if port:
                        inst_id = md.get("id")
                        if inst_id and inst_id in instruments:
                            instruments[inst_id]["midi_device_port"] = port
                        elif instruments:
                            list(instruments.values())[0]["midi_device_port"] = port

                inst_list = [instruments[k] for k in sorted(instruments.keys())]

                p_obj = {
                    "abbreviation": p_abbr.strip() if p_abbr else None,
                    "id": pid,
                    "instruments": inst_list,
                    "name": p_name.strip() if p_name else None,
                }
                part_meta.append(p_obj)

    meta_by_id = {p["id"]: p for p in part_meta}

    # parts & measures
    parts_data = []
    for part_elem in root.findall("part"):
        part_id = part_elem.get("id", "")
        measures_data = []

        current_divisions = 1
        measure_start_tick = Fraction(0)

        measures = part_elem.findall("measure")
        for m_idx, m_elem in enumerate(measures):
            m_num = m_elem.get("number", str(m_idx + 1))
            implicit = m_elem.get("implicit") == "yes"

            cursor_tick = measure_start_tick
            max_tick = measure_start_tick
            last_note_tick = measure_start_tick

            attributes_list = []
            barlines_list = []
            directions_list = []
            harmony_list = []
            figured_bass_list = []
            notes_list = []
            measure_lyrics = []
            unhandled_tags: set[str] = set()

            for child in m_elem:
                tag = child.tag
                if tag in IGNORED_MEASURE_TAGS:
                    continue
                elif tag == "attributes":
                    attr_dict = parse_attributes(child, cursor_tick)
                    if "divisions" in attr_dict:
                        current_divisions = attr_dict["divisions"]
                    attributes_list.append(attr_dict)
                elif tag == "backup":
                    dur_el = child.find("duration")
                    dur_val = int(dur_el.text) if dur_el is not None and dur_el.text else 0
                    dur_ticks = Fraction(dur_val * 480, current_divisions) if current_divisions else Fraction(0)
                    cursor_tick -= dur_ticks
                elif tag == "forward":
                    dur_el = child.find("duration")
                    dur_val = int(dur_el.text) if dur_el is not None and dur_el.text else 0
                    dur_ticks = Fraction(dur_val * 480, current_divisions) if current_divisions else Fraction(0)
                    cursor_tick += dur_ticks
                    max_tick = max(max_tick, cursor_tick)
                elif tag == "barline":
                    barlines_list.append(parse_barline(child))
                elif tag == "direction":
                    directions_list.append(parse_direction(child, cursor_tick, current_divisions))
                elif tag == "sound":
                    directions_list.append({"sound": parse_measure_sound(child, cursor_tick)})
                elif tag == "harmony":
                    harmony_list.append(parse_harmony(child, cursor_tick, current_divisions))
                elif tag == "figured-bass":
                    figured_bass_list.append(parse_figured_bass(child, cursor_tick, current_divisions))
                elif tag == "note":
                    is_chord = child.find("chord") is not None
                    is_grace = child.find("grace") is not None

                    note_start_tick = last_note_tick if is_chord else cursor_tick
                    if not is_chord:
                        last_note_tick = cursor_tick

                    note_dict, advance_duration, _ = parse_note(
                        child, note_start_tick, current_divisions, unhandled_tags
                    )
                    notes_list.append(note_dict)

                    if "lyrics" in note_dict:
                        for ly in note_dict["lyrics"]:
                            m_ly = dict(ly)
                            m_ly["tick"] = note_dict["tick"]
                            m_ly["voice"] = note_dict.get("voice")
                            m_ly["staff"] = note_dict.get("staff")
                            measure_lyrics.append(m_ly)

                    if not is_chord and not is_grace:
                        cursor_tick += advance_duration
                        max_tick = max(max_tick, cursor_tick)
                else:
                    unhandled_tags.add(tag)

            measure_dict = {
                "attributes": attributes_list,
                "barlines": barlines_list,
                "directions": directions_list,
                "end_tick": format_tick(max_tick),
                "figured_bass": figured_bass_list,
                "harmony": harmony_list,
                "implicit": implicit,
                "index": m_idx,
                "lyrics": measure_lyrics,
                "notes": notes_list,
                "number": m_num,
                "start_tick": format_tick(measure_start_tick),
                "unhandled": sorted(list(unhandled_tags)),
            }
            measures_data.append(measure_dict)
            measure_start_tick = max_tick

        part_entry: dict[str, Any] = {
            "id": part_id,
            "measures": measures_data,
        }
        if part_id in meta_by_id:
            part_entry["abbreviation"] = meta_by_id[part_id]["abbreviation"]
            part_entry["instruments"] = meta_by_id[part_id]["instruments"]
            part_entry["name"] = meta_by_id[part_id]["name"]

        parts_data.append(part_entry)

    return {
        "creators": creators,
        "movement_title": movement_title,
        "part_list": {
            "groups": part_groups,
            "parts": part_meta,
        },
        "parts": parts_data,
        "rights": rights_list,
        "work_title": work_title,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Normalize partwise MusicXML to semantic JSON.")
    parser.add_argument("file", nargs="?", help="Input MusicXML file")
    parser.add_argument("--tree", help="Root directory containing score.musicxml files")
    parser.add_argument("--output", help="Output directory for semantic JSON files")

    args = parser.parse_args()

    if args.tree:
        if not args.output:
            parser.error("--output DIR is required when using --tree ROOT")
        root_dir = pathlib.Path(args.tree)
        out_dir = pathlib.Path(args.output)

        pattern1 = list(root_dir.glob("*/score.musicxml"))
        pattern2 = list(root_dir.glob("*/excerpt-*/score.musicxml"))
        files = sorted(set(pattern1 + pattern2))

        total = len(files)
        success = 0
        errors: list[tuple[str, str]] = []

        for f in files:
            rel_path = f.relative_to(root_dir)
            out_path = out_dir / rel_path.with_suffix(".json")
            out_path.parent.mkdir(parents=True, exist_ok=True)
            try:
                text = f.read_text(encoding="utf-8")
                semantic_dict = musicxml_semantic(text)
                out_path.write_text(
                    json.dumps(semantic_dict, indent=2, sort_keys=True),
                    encoding="utf-8",
                )
                success += 1
            except Exception as e:
                errors.append((str(rel_path), str(e)))

        print(f"Summary: processed {total} files, {success} succeeded, {len(errors)} failed.")
        if errors:
            print("Errors encountered:")
            for p, err in errors:
                print(f"  {p}: {err}")
            sys.exit(1)
        return

    if args.file:
        file_path = pathlib.Path(args.file)
        text = file_path.read_text(encoding="utf-8")
        semantic_dict = musicxml_semantic(text)
        print(json.dumps(semantic_dict, indent=2, sort_keys=True))
        return

    parser.print_help()


if __name__ == "__main__":
    main()
