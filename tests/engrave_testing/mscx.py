"""Shared mscx helpers."""

from __future__ import annotations

import difflib
import re
import xml.etree.ElementTree as ET
from fractions import Fraction
from typing import Any

GRACE_TAGS = (
    "acciaccatura",
    "appoggiatura",
    "grace4",
    "grace16",
    "grace32",
    "grace8after",
    "grace16after",
    "grace32after",
)


def frac_to_ticks(frac_str: str | None, div: int) -> int:
    """Convert an MSCX fraction string ('N/D' or 'N') into ticks."""
    if not frac_str:
        return 0
    return int(Fraction(frac_str.strip()) * 4 * div)


def spanner_target_tick(
    nxt: ET.Element,
    m_idx: int,
    m_start: int,
    v_tick: int,
    div: int,
    meas_ticks: list[tuple[int, int, tuple[int, int]]],
) -> int:
    """Calculate spanner target end tick from next/location measures and fractions."""
    m_off = int(nxt.findtext("measures", "0"))
    f_off = frac_to_ticks(nxt.findtext("fractions", "0"), div)
    target_m = m_idx + m_off
    target_start = meas_ticks[target_m][0] if target_m < len(meas_ticks) else v_tick
    return target_start + (v_tick - m_start) + f_off


def get_spanner_props(sp: ET.Element) -> tuple[tuple[str, Any], ...]:
    """Extract placement, offset, endings, and beginText properties for supported spanners."""
    stype = sp.get("type", "") or sp.tag
    if stype not in {"Pedal", "Ottava", "HairPin", "Volta"}:
        return ()
    inner = sp.find(stype)
    target = inner if inner is not None else sp
    props: dict[str, Any] = {}
    placement = target.findtext("placement")
    if placement:
        props["placement"] = placement.strip()
    off_el = target.find(".//offset")
    if off_el is not None:
        props["offset"] = (off_el.get("x", "0"), off_el.get("y", "0"))
    endings = target.findtext("endings")
    if endings:
        props["endings"] = endings.strip()
    begin_text = target.findtext("beginText")
    if begin_text:
        props["beginText"] = begin_text.strip()
    return tuple(sorted(props.items()))


def normalize_mscx_text(text: str) -> str:
    """Normalize MSCX XML text for MSCZ round-trip text comparison."""
    text = re.sub(r"<eid>[^<]*</eid>", "<eid>0</eid>", text)
    # The writing program's version lines are not score content; dropping them keeps
    # pinned digests (which include hunk line numbers) stable across engine versions.
    text = re.sub(
        r"[ \t]*<program(?:Version|Revision)>[^<]*</program(?:Version|Revision)>\r?\n",
        "",
        text,
    )
    text = re.sub(r"<([a-zA-Z0-9_]+)([^>]*)></\1>", r"<\1\2/>", text)
    return text


PAGE_STYLE_DEFAULTS = (
    ("pageWidth", "8.26772"),
    ("pageHeight", "11.6929"),
    ("pagePrintableWidth", "7.08661"),
    ("pageEvenLeftMargin", "0.590551"),
    ("pageOddLeftMargin", "0.590551"),
    ("pageEvenTopMargin", "0.590551"),
    ("pageEvenBottomMargin", "0.590551"),
    ("pageOddTopMargin", "0.590551"),
    ("pageOddBottomMargin", "0.590551"),
)


def _xml_text(value: str | None, preserve: bool = False) -> str:
    # Pretty-printed MSCX uses newline-bearing whitespace between elements.
    # A space without a newline can be score text (including mixed rich text)
    # and must not be discarded.
    if not value or (not preserve and not value.strip() and ("\n" in value or "\r" in value)):
        return ""
    return value


def _xml_value(element: ET.Element, preserve_tail: bool = False) -> tuple:
    """Represent the complete XML subtree, ignoring indentation only."""
    mixed = element.tag == "text"
    return (
        element.tag,
        tuple(sorted(element.attrib.items())),
        _xml_text(element.text, mixed or not len(element)),
        _xml_text(element.tail, preserve_tail),
        tuple(_xml_value(child, mixed) for child in element),
    )


def compare_mscz_xml(before: str, after: str) -> list[tuple]:
    """Compare complete score XML after the narrowly documented page-style insertion.

    All other elements, ordering, attributes and non-whitespace text are compared.
    In particular, the standalone excerpt name and synthesized title size are
    reported as losses, with fixture-specific digests, rather than discarded.
    """
    left = ET.fromstring(normalize_mscx_text(before))
    right = ET.fromstring(normalize_mscx_text(after))
    left_scores, right_scores = list(left.iter("Score")), list(right.iter("Score"))
    if len(left_scores) == len(right_scores):
        tags = {tag for tag, _ in PAGE_STYLE_DEFAULTS}
        for a, b in zip(left_scores, right_scores):
            style_a, style_b = a.find("Style"), b.find("Style")
            if style_a is None or style_b is None:
                continue
            prefix = list(style_b)[: len(PAGE_STYLE_DEFAULTS)]
            if (
                not any(child.tag in tags for child in style_a)
                and [(child.tag, child.text) for child in prefix] == list(PAGE_STYLE_DEFAULTS)
                and not any(child.attrib or len(child) or _xml_text(child.tail) for child in prefix)
                and not any(child.tag in tags for child in list(style_b)[len(prefix) :])
            ):
                for child in prefix:
                    style_b.remove(child)

    differences: list[tuple] = []

    def compare(a: ET.Element, b: ET.Element, path: str, preserve_tail: bool = False) -> None:
        mixed = a.tag == "text" or b.tag == "text"
        for field, av, bv in (
            ("tag", a.tag, b.tag),
            (
                "attributes",
                tuple(sorted(a.attrib.items())),
                tuple(sorted(b.attrib.items())),
            ),
            (
                "text",
                _xml_text(a.text, mixed or not len(a)),
                _xml_text(b.text, mixed or not len(b)),
            ),
            (
                "tail",
                _xml_text(a.tail, preserve_tail),
                _xml_text(b.tail, preserve_tail),
            ),
        ):
            if av != bv:
                differences.append((f"{path}/{field}", av, bv))
        ac, bc = list(a), list(b)
        matcher = difflib.SequenceMatcher(a=[x.tag for x in ac], b=[x.tag for x in bc], autojunk=False)
        for operation, ai, aj, bi, bj in matcher.get_opcodes():
            if operation == "equal":
                for index in range(aj - ai):
                    child = ac[ai + index]
                    compare(
                        child,
                        bc[bi + index],
                        f"{path}/{child.tag}[{ai + index}]",
                        mixed,
                    )
            else:
                differences.append(
                    (
                        f"{path}/children[{ai}:{aj}]",
                        tuple(_xml_value(child, mixed) for child in ac[ai:aj]),
                        tuple(_xml_value(child, mixed) for child in bc[bi:bj]),
                    )
                )

    compare(left, right, f"/{left.tag}")
    return differences


def parse_mscx_500(root: ET.Element) -> dict[str, Any]:
    score = root.find("Score")
    if score is None:
        score = root
    div = int(score.findtext("Division", "480"))

    # Metadata
    metadata: dict[str, str] = {}
    for m in score.findall("metaTag"):
        name = m.get("name")
        if name and name not in ("creationDate", "platform"):
            metadata[name] = (m.text or "").strip()

    # Parts
    parts: list[dict[str, Any]] = []
    for p in score.findall("Part"):
        inst = p.find("Instrument")
        staves_p = p.findall("Staff")
        groups = [s.find("StaffType").get("group", "") for s in staves_p if s.find("StaffType") is not None]
        inst_vis = {
            c.tag: (c.text or "").strip()
            for c in (inst if inst is not None else [])
            if "vis" in c.tag.lower() or "show" in c.tag.lower()
        }

        long_name = ""
        if inst is not None:
            long_name = inst.findtext("InstrumentLabel/longName") or inst.findtext("longName") or ""
        long_name = long_name.strip()

        track_name = p.findtext("trackName", "")
        if not track_name and inst is not None:
            track_name = inst.findtext("trackName", "")
        track_name = track_name.strip()

        inst_id = inst.findtext("instrumentId", "").strip() if inst is not None else ""

        parts.append(
            {
                "trackName": track_name,
                "instrumentId": inst_id,
                "longName": long_name,
                "staffCount": len(staves_p),
                "staffTypeGroups": groups,
                "show": p.findtext("show"),
                "staffInvisible": [s.findtext("invisible") for s in staves_p],
                "instrumentVisibility": inst_vis,
            }
        )

    # Measures per staff
    staves = score.findall("Staff")
    meas_ticks: list[tuple[int, int, tuple[int, int]]] = []
    curr_tick, curr_sig = 0, (4, 4)
    if staves:
        for m in staves[0].findall("Measure"):
            ts = m.find(".//TimeSig")
            if ts is not None:
                curr_sig = (
                    int(ts.findtext("sigN", "4")),
                    int(ts.findtext("sigD", "4")),
                )
            mlen = (
                frac_to_ticks(m.get("len"), div) if m.get("len") else int(Fraction(curr_sig[0], curr_sig[1]) * 4 * div)
            )
            meas_ticks.append((curr_tick, mlen, curr_sig))
            curr_tick += mlen

    staff_measures, staff_events, staff_spanners, staff_clefs, staff_annotations = (
        {},
        {},
        {},
        {},
        {},
    )
    eid_to_info: dict[str, Any] = {}
    ties_to_resolve: list[dict[str, Any]] = []

    for s_idx, staff in enumerate(staves):
        staff_id = staff.get("id", str(s_idx + 1))
        measures_info, events_by_voice, spanners_list, clef_list = [], {}, [], []
        (
            dyn_counts,
            tempo_list,
            lyrics_list,
            artic_counts,
            fingering_list,
            staff_text_list,
        ) = {}, [], [], {}, [], []
        jumps_list, markers_list = [], []
        harmony_count, fermata_count = 0, 0

        m_list = staff.findall("Measure")
        for m_idx, m in enumerate(m_list):
            m_start, mlen, _ = meas_ticks[m_idx] if m_idx < len(meas_ticks) else (0, 1920, (4, 4))
            m_eid = m.findtext("eid")
            if m_eid:
                eid_to_info[m_eid.strip()] = ("measure", staff_id, m_idx, m_start, mlen)

            ts = m.find(".//TimeSig")
            ts_str = f"{ts.findtext('sigN')}/{ts.findtext('sigD')}" if ts is not None else None
            ks = m.find(".//KeySig")
            key_val = (ks.findtext("concertKey") or ks.findtext("accidental")) if ks is not None else None
            measures_info.append(
                {
                    "timesig": ts_str,
                    "len": m.get("len"),
                    "startRepeat": bool(m.find("startRepeat") is not None),
                    "endRepeat": int(m.findtext("endRepeat")) if m.find("endRepeat") is not None else 0,
                    "keySig": key_val,
                }
            )

            for mc in m.findall("Clef"):
                c_type = (mc.findtext("concertClefType") or mc.findtext("transposingClefType") or "").strip()
                clef_list.append((m_start, c_type))
            for j in m.findall("Jump"):
                j_sub = (j.findtext("jumpType") or j.findtext("subtype") or j.findtext("text") or "").strip()
                jumps_list.append((m_idx, j_sub))
            for mk in m.findall("Marker"):
                mk_sub = (
                    mk.findtext("markerType")
                    or mk.findtext("subtype")
                    or mk.findtext("label")
                    or mk.findtext("text")
                    or ""
                ).strip()
                markers_list.append((m_idx, mk_sub))

            for v_idx, voice in enumerate(m.findall("voice")):
                events_by_voice.setdefault(v_idx, [])
                v_tick = m_start
                raw_events = []
                seq = 0
                tuplet_stack = []

                for child in voice:
                    if child.tag == "Tuplet":
                        norm = int(child.findtext("normalNotes", "2"))
                        act = int(child.findtext("actualNotes", "3"))
                        tuplet_stack.append((norm, act))
                    elif child.tag == "endTuplet":
                        if tuplet_stack:
                            tuplet_stack.pop()
                    elif child.tag == "TimeSig":
                        sig_n = child.findtext("sigN")
                        sig_d = child.findtext("sigD")
                        if sig_n and sig_d:
                            curr_sig = (int(sig_n), int(sig_d))
                    elif child.tag == "Clef":
                        c_type = (
                            child.findtext("concertClefType") or child.findtext("transposingClefType") or ""
                        ).strip()
                        clef_list.append((v_tick, c_type))
                    elif child.tag == "Chord":
                        c_eid = child.findtext("eid")
                        grace_kind = "none"
                        for gtag in GRACE_TAGS:
                            if child.find(gtag) is not None:
                                grace_kind = gtag
                                break
                        is_grace = grace_kind != "none"
                        is_grace_after = grace_kind.endswith("after")

                        if is_grace:
                            dur = 0
                            event_tick = v_tick
                            order_rank = 2 if is_grace_after else 0
                        elif child.findtext("duration"):
                            dur = frac_to_ticks(child.findtext("duration"), div)
                            event_tick = v_tick
                            order_rank = 1
                        else:
                            dtype = child.findtext("durationType", "quarter")
                            dots = int(child.findtext("dots", "0"))
                            base_dur = Fraction(DUR_TYPES.get(dtype, 1)) * div * Fraction(2 ** (dots + 1) - 1, 2**dots)
                            ratio = Fraction(1, 1)
                            for norm, act in tuplet_stack:
                                ratio *= Fraction(norm, act)
                            dur = int(base_dur * ratio)
                            event_tick = v_tick
                            order_rank = 1

                        if c_eid:
                            eid_to_info[c_eid.strip()] = (
                                "chord",
                                staff_id,
                                event_tick,
                                dur,
                                child,
                            )

                        stem_dir = (child.findtext("StemDirection") or child.findtext("stemDirection") or "").strip()
                        notes_list = []
                        for n in child.findall("Note"):
                            n_eid = n.findtext("eid")
                            pitch, tpc = (
                                int(n.findtext("pitch", "0")),
                                int(n.findtext("tpc", "0")),
                            )
                            head = (n.findtext("head") or "").strip()
                            head_type = (n.findtext("headType") or "").strip()

                            # Note object representation in list: [pitch, tpc, tie_start, head, head_type]
                            note_obj = [pitch, tpc, False, head, head_type]
                            notes_list.append(note_obj)

                            if n_eid:
                                eid_to_info[n_eid.strip()] = (
                                    "note",
                                    staff_id,
                                    event_tick,
                                    dur,
                                    note_obj,
                                )

                            for f in n.findall("Fingering"):
                                fingering_list.append(f.findtext("text", "").strip())

                            tie = n.find("Tie")
                            if tie is not None:
                                s_el = tie.findtext("startElement")
                                e_el = tie.findtext("endElement")
                                ties_to_resolve.append(
                                    {
                                        "end_note": note_obj,
                                        "end_tick": event_tick,
                                        "staff_id": staff_id,
                                        "voice_idx": v_idx,
                                        "pitch": pitch,
                                        "is_grace_after": is_grace_after,
                                        "start_eid": s_el.strip() if s_el else None,
                                        "end_eid": e_el.strip() if e_el else None,
                                        "raw_index": len(raw_events),
                                        "raw_events": raw_events,
                                        "m_idx": m_idx,
                                    }
                                )

                        evt_container = [
                            "chord",
                            event_tick,
                            dur,
                            notes_list,
                            grace_kind,
                            stem_dir,
                        ]
                        raw_events.append((event_tick, order_rank, seq, evt_container))
                        seq += 1

                        for art in child.findall("Articulation"):
                            sub = art.findtext("subtype") or art.get("name") or ""
                            artic_counts[sub] = artic_counts.get(sub, 0) + 1
                        for lyr in child.findall("Lyrics"):
                            lyrics_list.append(
                                (
                                    int(lyr.findtext("no", "0")),
                                    lyr.findtext("text", "").strip(),
                                )
                            )
                        fermata_count += len(child.findall("Fermata"))
                        v_tick += dur
                    elif child.tag == "Rest":
                        if child.findtext("duration"):
                            dur = frac_to_ticks(child.findtext("duration"), div)
                        elif child.findtext("durationType") == "measure":
                            dur = mlen
                        else:
                            dtype = child.findtext("durationType", "quarter")
                            dots = int(child.findtext("dots", "0"))
                            base_dur = Fraction(DUR_TYPES.get(dtype, 1)) * div * Fraction(2 ** (dots + 1) - 1, 2**dots)
                            ratio = Fraction(1, 1)
                            for norm, act in tuplet_stack:
                                ratio *= Fraction(norm, act)
                            dur = int(base_dur * ratio)

                        event_tick = v_tick
                        order_rank = 1
                        r_eid = child.findtext("eid")
                        evt_container = ["rest", event_tick, dur, (), "none", ""]
                        if r_eid:
                            eid_to_info[r_eid.strip()] = (
                                "rest",
                                staff_id,
                                event_tick,
                                dur,
                                child,
                            )
                        raw_events.append((event_tick, order_rank, seq, evt_container))
                        seq += 1

                        fermata_count += len(child.findall("Fermata"))
                        v_tick += dur
                    elif child.tag == "Dynamic":
                        sub = child.findtext("subtype", "").strip()
                        dyn_counts[sub] = dyn_counts.get(sub, 0) + 1
                    elif child.tag == "Tempo":
                        tempo_list.append(round(float(child.findtext("tempo", "0")), 4))
                    elif child.tag == "Harmony":
                        harmony_count += 1
                    elif child.tag == "StaffText":
                        staff_text_list.append(child.findtext("text", "").strip())
                    elif child.tag == "Jump":
                        j_sub = (
                            child.findtext("jumpType") or child.findtext("subtype") or child.findtext("text") or ""
                        ).strip()
                        jumps_list.append((m_idx, j_sub))
                    elif child.tag == "Marker":
                        mk_sub = (
                            mk.findtext("markerType")
                            or mk.findtext("subtype")
                            or mk.findtext("label")
                            or mk.findtext("text")
                            or ""
                        ).strip()
                        markers_list.append((m_idx, mk_sub))

                raw_events.sort(key=lambda item: (item[0], item[1], item[2]))
                events_by_voice[v_idx].extend(item[3] for item in raw_events)

        staff_measures[staff_id] = measures_info
        staff_events[staff_id] = events_by_voice
        staff_spanners[staff_id] = spanners_list
        staff_clefs[staff_id] = sorted(clef_list)
        staff_annotations[staff_id] = {
            "dynamics": dyn_counts,
            "tempos": tempo_list,
            "harmony": harmony_count,
            "lyrics": lyrics_list,
            "articulations": artic_counts,
            "fingerings": fingering_list,
            "staffTexts": staff_text_list,
            "fermatas": fermata_count,
            "jumps": jumps_list,
            "markers": markers_list,
        }

    # Resolve Ties
    for tie_info in ties_to_resolve:
        staff_id = tie_info["staff_id"]
        v_idx = tie_info["voice_idx"]
        pitch = tie_info["pitch"]
        end_tick = tie_info["end_tick"]
        start_eid = tie_info["start_eid"]
        is_grace_after = tie_info["is_grace_after"]

        start_found = False
        start_tick = 0
        info = None
        if start_eid and start_eid in eid_to_info:
            info = eid_to_info[start_eid]
            if info[0] == "note":
                info[4][2] = True  # tie_start = True
                start_tick = info[2]
                start_found = True

        if not start_found:
            # Fallback when EIDs were stripped
            raw_evts = tie_info["raw_events"]
            raw_idx = tie_info["raw_index"]
            if is_grace_after:
                for item in raw_evts[raw_idx + 1 :]:
                    evt = item[3]
                    if evt[0] == "chord":
                        for n in evt[3]:
                            if n[0] == pitch:
                                n[2] = True
                                start_tick = evt[1]
                                start_found = True
                                break
                    if start_found:
                        break
            else:
                for item in reversed(raw_evts[:raw_idx]):
                    evt = item[3]
                    if evt[0] == "chord":
                        for n in evt[3]:
                            if n[0] == pitch:
                                n[2] = True
                                start_tick = evt[1]
                                start_found = True
                                break
                    if start_found:
                        break
                if not start_found:
                    for evt in reversed(staff_events[staff_id][v_idx]):
                        if evt[0] == "chord" and evt[1] < end_tick:
                            for n in evt[3]:
                                if n[0] == pitch:
                                    n[2] = True
                                    start_tick = evt[1]
                                    start_found = True
                                    break
                        if start_found:
                            break

        tie_staff = info[1] if (start_found and start_eid and start_eid in eid_to_info) else staff_id
        if start_found:
            staff_spanners.setdefault(tie_staff, []).append(("Tie", tie_staff, start_tick, end_tick, ()))

    # Convert notes_list to tuple(sorted(notes)) and evt_container to tuple
    for staff_id, vmap in staff_events.items():
        for v_idx, evts in vmap.items():
            final_evts = []
            for evt in evts:
                if evt[0] == "chord":
                    sorted_notes = tuple(sorted((n[0], n[1], n[2], n[3], n[4]) for n in evt[3]))
                    final_evts.append((evt[0], evt[1], evt[2], sorted_notes, evt[4], evt[5]))
                else:
                    final_evts.append(tuple(evt))
            vmap[v_idx] = final_evts

    # Resolve score-level <SpannerMap>
    spanner_map = score.find("SpannerMap")
    if spanner_map is not None:
        for sp in spanner_map:
            stype = sp.tag
            track = int(sp.findtext("track", "0"))
            s_idx = track // 4
            staff_id = staves[s_idx].get("id", str(s_idx + 1)) if s_idx < len(staves) else str(s_idx + 1)
            props = get_spanner_props(sp)

            start_tick_el = sp.find("startTick")
            if start_tick_el is not None:
                start_tick = frac_to_ticks(start_tick_el.text, div)
                ticks_val = frac_to_ticks(sp.findtext("ticks"), div)
                end_tick = start_tick + ticks_val
                if staff_id in staff_spanners:
                    staff_spanners[staff_id].append((stype, staff_id, start_tick, end_tick, props))
            else:
                s_el = sp.findtext("startElement")
                e_el = sp.findtext("endElement")
                start_eid = s_el.strip() if s_el else None
                end_eid = e_el.strip() if e_el else None
                if start_eid and end_eid:
                    start_info = eid_to_info.get(start_eid)
                    end_info = eid_to_info.get(end_eid)
                    if start_info and end_info:
                        if stype == "Volta":
                            start_m = start_info[2]
                            end_m = end_info[2]
                            start_tick = meas_ticks[start_m][0]
                            end_tick = meas_ticks[end_m][0] + meas_ticks[end_m][1]
                        else:
                            start_tick = start_info[2]
                            end_tick = end_info[2]
                        if staff_id in staff_spanners:
                            staff_spanners[staff_id].append((stype, staff_id, start_tick, end_tick, props))

    for staff_id in staff_spanners:
        staff_spanners[staff_id].sort()

    excerpts = [
        {
            "title": s.findtext("name") or "",
            "parts": [p.get("id") for p in s.findall("Part")],
        }
        for s in score.findall("Score")
    ]
    return {
        "metadata": metadata,
        "parts": parts,
        "measures": staff_measures,
        "events": staff_events,
        "spanners": staff_spanners,
        "clefs": staff_clefs,
        "annotations": staff_annotations,
        "excerpts": excerpts,
    }


def parse_mscx(text: str) -> dict[str, Any]:
    """Parse a MuseScore MSCX document into its semantic representation."""
    root = ET.fromstring(text)
    version = root.get("version", "4.70")
    if version.startswith("5."):
        return parse_mscx_500(root)

    score = root.find("Score")
    if score is None:
        score = root
    div = int(score.findtext("Division", "480"))

    # Metadata: metaTag name->value, excluding creationDate and platform.
    metadata: dict[str, str] = {}
    for m in score.findall("metaTag"):
        name = m.get("name")
        if name and name not in ("creationDate", "platform"):
            metadata[name] = (m.text or "").strip()

    # Parts: trackName, instrumentId, longName, staffCount, staffTypeGroups, show, staffInvisible, instrumentVisibility.
    parts: list[dict[str, Any]] = []
    for p in score.findall("Part"):
        inst = p.find("Instrument")
        staves_p = p.findall("Staff")
        groups = [s.find("StaffType").get("group", "") for s in staves_p if s.find("StaffType") is not None]
        inst_vis = {
            c.tag: (c.text or "").strip()
            for c in (inst if inst is not None else [])
            if "vis" in c.tag.lower() or "show" in c.tag.lower()
        }
        parts.append(
            {
                "trackName": p.findtext("trackName", "").strip(),
                "instrumentId": inst.findtext("instrumentId", "").strip() if inst is not None else "",
                "longName": inst.findtext("longName", "").strip() if inst is not None else "",
                "staffCount": len(staves_p),
                "staffTypeGroups": groups,
                "show": p.findtext("show"),
                "staffInvisible": [s.findtext("invisible") for s in staves_p],
                "instrumentVisibility": inst_vis,
            }
        )

    # Measures per staff: count, timesig/len, start/end repeat, key sig changes.
    staves = score.findall("Staff")
    meas_ticks: list[tuple[int, int, tuple[int, int]]] = []
    curr_tick, curr_sig = 0, (4, 4)
    if staves:
        for m in staves[0].findall("Measure"):
            ts = m.find(".//TimeSig")
            if ts is not None:
                curr_sig = (
                    int(ts.findtext("sigN", "4")),
                    int(ts.findtext("sigD", "4")),
                )
            mlen = (
                frac_to_ticks(m.get("len"), div) if m.get("len") else int(Fraction(curr_sig[0], curr_sig[1]) * 4 * div)
            )
            meas_ticks.append((curr_tick, mlen, curr_sig))
            curr_tick += mlen

    staff_measures, staff_events, staff_spanners, staff_annotations, staff_clefs = (
        {},
        {},
        {},
        {},
        {},
    )
    for s_idx, staff in enumerate(staves):
        staff_id = staff.get("id", str(s_idx + 1))
        measures_info, events_by_voice, spanners_list, clef_list = [], {}, [], []
        (
            dyn_counts,
            tempo_list,
            lyrics_list,
            artic_counts,
            fingering_list,
            staff_text_list,
        ) = {}, [], [], {}, [], []
        jumps_list, markers_list = [], []
        harmony_count, fermata_count = 0, 0

        m_list = staff.findall("Measure")
        for m_idx, m in enumerate(m_list):
            m_start, mlen, _ = meas_ticks[m_idx] if m_idx < len(meas_ticks) else (0, 1920, (4, 4))
            ts = m.find(".//TimeSig")
            ts_str = f"{ts.findtext('sigN')}/{ts.findtext('sigD')}" if ts is not None else None
            ks = m.find(".//KeySig")
            key_val = (ks.findtext("concertKey") or ks.findtext("accidental")) if ks is not None else None
            measures_info.append(
                {
                    "timesig": ts_str,
                    "len": m.get("len"),
                    "startRepeat": bool(m.find("startRepeat") is not None),
                    "endRepeat": int(m.findtext("endRepeat")) if m.find("endRepeat") is not None else 0,
                    "keySig": key_val,
                }
            )

            # Check measure-level Clef, Jump, and Marker elements
            for c in m.findall("Clef"):
                c_type = (c.findtext("concertClefType") or c.findtext("transposingClefType") or "").strip()
                clef_list.append((m_start, c_type))
            for j in m.findall("Jump"):
                j_sub = (j.findtext("jumpType") or j.findtext("subtype") or j.findtext("text") or "").strip()
                jumps_list.append((m_idx, j_sub))
            for mk in m.findall("Marker"):
                mk_sub = (
                    mk.findtext("markerType")
                    or mk.findtext("subtype")
                    or mk.findtext("label")
                    or mk.findtext("text")
                    or ""
                ).strip()
                markers_list.append((m_idx, mk_sub))

            # Events per voice: chords and rests with absolute tick & duration ticks
            for v_idx, v in enumerate(m.findall("voice")):
                if v_idx not in events_by_voice:
                    events_by_voice[v_idx] = []
                v_tick = m_start
                last_main_tick = m_start
                tuplet_stack: list[tuple[int, int]] = []
                raw_events: list[tuple[int, int, int, Any]] = []
                seq = 0

                for child in v:
                    if child.tag == "Tuplet":
                        tuplet_stack.append(
                            (
                                int(child.findtext("normalNotes", "2")),
                                int(child.findtext("actualNotes", "3")),
                            )
                        )
                    elif child.tag == "endTuplet" and tuplet_stack:
                        tuplet_stack.pop()
                    elif child.tag == "location":
                        if child.findtext("fractions"):
                            v_tick += frac_to_ticks(child.findtext("fractions"), div)
                    elif child.tag == "Clef":
                        c_type = (
                            child.findtext("concertClefType") or child.findtext("transposingClefType") or ""
                        ).strip()
                        clef_list.append((v_tick, c_type))
                    elif child.tag == "Chord":
                        grace_kind = "none"
                        for gtag in GRACE_TAGS:
                            if child.find(gtag) is not None:
                                grace_kind = gtag
                                break
                        is_grace = grace_kind != "none"
                        is_grace_after = grace_kind.endswith("after")

                        if is_grace:
                            dur = 0
                            event_tick = last_main_tick if is_grace_after else v_tick
                            order_rank = 2 if is_grace_after else 0
                        elif child.findtext("duration"):
                            dur = frac_to_ticks(child.findtext("duration"), div)
                            event_tick = v_tick
                            last_main_tick = v_tick
                            order_rank = 1
                        else:
                            dtype = child.findtext("durationType", "quarter")
                            dots = int(child.findtext("dots", "0"))
                            base_dur = Fraction(DUR_TYPES.get(dtype, 1)) * div * Fraction(2 ** (dots + 1) - 1, 2**dots)
                            ratio = Fraction(1, 1)
                            for norm, act in tuplet_stack:
                                ratio *= Fraction(norm, act)
                            dur = int(base_dur * ratio)
                            event_tick = v_tick
                            last_main_tick = v_tick
                            order_rank = 1

                        stem_dir = (child.findtext("StemDirection") or child.findtext("stemDirection") or "").strip()
                        notes = []
                        for n in child.findall("Note"):
                            pitch, tpc = (
                                int(n.findtext("pitch", "0")),
                                int(n.findtext("tpc", "0")),
                            )
                            tie_start = any(
                                sp.get("type") == "Tie" and sp.find("next") is not None for sp in n.findall("Spanner")
                            ) or (n.find("Tie") is not None and n.find("Tie/prev") is None)
                            head = (n.findtext("head") or "").strip()
                            head_type = (n.findtext("headType") or "").strip()
                            notes.append((pitch, tpc, tie_start, head, head_type))
                            for f in n.findall("Fingering"):
                                fingering_list.append(f.findtext("text", "").strip())
                            for sp in n.findall("Spanner"):
                                if sp.get("type") == "Tie":
                                    nxt = sp.find("next/location")
                                    if nxt is not None:
                                        end_tick = spanner_target_tick(nxt, m_idx, m_start, v_tick, div, meas_ticks)
                                        spanners_list.append(("Tie", staff_id, v_tick, end_tick, ()))

                        evt = (
                            "chord",
                            event_tick,
                            dur,
                            tuple(sorted(notes)),
                            grace_kind,
                            stem_dir,
                        )
                        raw_events.append((event_tick, order_rank, seq, evt))
                        seq += 1

                        for art in child.findall("Articulation"):
                            sub = art.findtext("subtype") or art.get("name") or ""
                            artic_counts[sub] = artic_counts.get(sub, 0) + 1
                        for lyr in child.findall("Lyrics"):
                            lyrics_list.append(
                                (
                                    int(lyr.findtext("no", "0")),
                                    lyr.findtext("text", "").strip(),
                                )
                            )
                        fermata_count += len(child.findall("Fermata"))
                        for sp in child.findall("Spanner"):
                            nxt = sp.find("next/location")
                            if nxt is not None:
                                end_tick = spanner_target_tick(nxt, m_idx, m_start, v_tick, div, meas_ticks)
                                props = get_spanner_props(sp)
                                spanners_list.append(
                                    (
                                        sp.get("type", ""),
                                        staff_id,
                                        v_tick,
                                        end_tick,
                                        props,
                                    )
                                )
                        v_tick += dur
                    elif child.tag == "Rest":
                        if child.findtext("duration"):
                            dur = frac_to_ticks(child.findtext("duration"), div)
                        elif child.findtext("durationType") == "measure":
                            dur = mlen
                        else:
                            dtype = child.findtext("durationType", "quarter")
                            dots = int(child.findtext("dots", "0"))
                            base_dur = Fraction(DUR_TYPES.get(dtype, 1)) * div * Fraction(2 ** (dots + 1) - 1, 2**dots)
                            ratio = Fraction(1, 1)
                            for norm, act in tuplet_stack:
                                ratio *= Fraction(norm, act)
                            dur = int(base_dur * ratio)

                        event_tick = v_tick
                        last_main_tick = v_tick
                        order_rank = 1
                        evt = ("rest", event_tick, dur, (), "none", "")
                        raw_events.append((event_tick, order_rank, seq, evt))
                        seq += 1

                        fermata_count += len(child.findall("Fermata"))
                        v_tick += dur
                    elif child.tag == "Dynamic":
                        sub = child.findtext("subtype", "").strip()
                        dyn_counts[sub] = dyn_counts.get(sub, 0) + 1
                    elif child.tag == "Tempo":
                        tempo_list.append(round(float(child.findtext("tempo", "0")), 4))
                    elif child.tag == "Harmony":
                        harmony_count += 1
                    elif child.tag == "StaffText":
                        staff_text_list.append(child.findtext("text", "").strip())
                    elif child.tag == "Jump":
                        j_sub = (
                            child.findtext("jumpType") or child.findtext("subtype") or child.findtext("text") or ""
                        ).strip()
                        jumps_list.append((m_idx, j_sub))
                    elif child.tag == "Marker":
                        mk_sub = (
                            child.findtext("markerType")
                            or mk.findtext("subtype")
                            or mk.findtext("label")
                            or mk.findtext("text")
                            or ""
                        ).strip()
                        markers_list.append((m_idx, mk_sub))
                    elif child.tag == "Spanner":
                        nxt = child.find("next/location")
                        if nxt is not None:
                            end_tick = spanner_target_tick(nxt, m_idx, m_start, v_tick, div, meas_ticks)
                            props = get_spanner_props(child)
                            spanners_list.append(
                                (
                                    child.get("type", ""),
                                    staff_id,
                                    v_tick,
                                    end_tick,
                                    props,
                                )
                            )

                raw_events.sort(key=lambda item: (item[0], item[1], item[2]))
                events_by_voice[v_idx].extend(item[3] for item in raw_events)

        staff_measures[staff_id] = measures_info
        staff_events[staff_id] = events_by_voice
        staff_spanners[staff_id] = sorted(spanners_list)
        staff_clefs[staff_id] = sorted(clef_list)
        staff_annotations[staff_id] = {
            "dynamics": dyn_counts,
            "tempos": tempo_list,
            "harmony": harmony_count,
            "lyrics": lyrics_list,
            "articulations": artic_counts,
            "fingerings": fingering_list,
            "staffTexts": staff_text_list,
            "fermatas": fermata_count,
            "jumps": jumps_list,
            "markers": markers_list,
        }

    excerpts = [
        {
            "title": s.findtext("name") or "",
            "parts": [p.get("id") for p in s.findall("Part")],
        }
        for s in score.findall("Score")
    ]
    return {
        "metadata": metadata,
        "parts": parts,
        "measures": staff_measures,
        "events": staff_events,
        "spanners": staff_spanners,
        "clefs": staff_clefs,
        "annotations": staff_annotations,
        "excerpts": excerpts,
    }


DUR_TYPES: dict[str, Fraction | int] = {
    "longa": 16,
    "breve": 8,
    "whole": 4,
    "half": 2,
    "quarter": 1,
    "eighth": Fraction(1, 2),
    "16th": Fraction(1, 4),
    "32nd": Fraction(1, 8),
    "64th": Fraction(1, 16),
    "128th": Fraction(1, 32),
    "256th": Fraction(1, 64),
    "512th": Fraction(1, 128),
    "1024th": Fraction(1, 256),
}
