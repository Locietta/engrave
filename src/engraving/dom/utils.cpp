/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include <cmath>
#include <map>

#include "accidental.h"
#include "actionicon.h"
#include "arpeggio.h"
#include "barline.h"
#include "chord.h"
#include "chordrest.h"
#include "clef.h"
#include "containers.h"
#include "drumset.h"
#include "fret.h"
#include "harmony.h"
#include "keysig.h"
#include "laissezvib.h"
#include "log.h"
#include "lyrics.h"
#include "marker.h"
#include "masterscore.h"
#include "measure.h"
#include "measurenumber.h"
#include "measurerepeat.h"
#include "note.h"
#include "page.h"
#include "part.h"
#include "partialtie.h"
#include "pitchspelling.h"
#include "playcounttext.h"
#include "repeatlist.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "select.h"
#include "sig.h"
#include "spanner.h"
#include "staff.h"
#include "system.h"
#include "tremolosinglechord.h"
#include "tremolotwochord.h"
#include "tuplet.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   handleRect
//---------------------------------------------------------

RectF handle_rect(const PointF &pos) { return RectF(pos.x() - 4, pos.y() - 4, 8, 8); }

//---------------------------------------------------------
//   tick2measure
//---------------------------------------------------------

Measure *Score::tick2measure(const Fraction &tick) const {
    if (tick == Fraction(-1, 1)) { // special number
        return last_measure();
    }
    if (tick <= Fraction(0, 1)) {
        return first_measure();
    }

    return measures_.measure_by_tick(tick.ticks());
}

//---------------------------------------------------------
//   tick2measureMM
//---------------------------------------------------------

Measure *Score::tick2measure_mm(const Fraction &t) const {
    Fraction tick(t);
    if (tick == Fraction(-1, 1)) {
        return last_measure_mm();
    }
    if (tick < Fraction(0, 1)) {
        tick = Fraction(0, 1);
    }

    Measure *measure = measures_.measure_by_tick(t.ticks());
    if (!measure) {
        LOGD("tick2measureMM %d not found", tick.ticks());
        return nullptr;
    }

    return measure->covering_mm_rest_or_this();
}

MeasureBase *Score::tick2measure_base(const Fraction &tick) const {
    if (tick == Fraction(-1, 1)) { // special number
        return measures_.last();
    }
    if (tick <= Fraction(0, 1)) {
        return measures_.first();
    }

    return measures_.first_measure_base_at_tick(tick.ticks());
}

//---------------------------------------------------------
//   tick2segment
//---------------------------------------------------------

Segment *Score::tick2segment_mm(const Fraction &tick, bool first, SegmentType st) const { return tick2segment(tick, first, st, true); }

Segment *Score::tick2segment_mm(const Fraction &tick) const { return tick2segment(tick, false, SegmentType::ALL, true); }

Segment *Score::tick2segment_mm(const Fraction &tick, bool first) const { return tick2segment(tick, first, SegmentType::ALL, true); }

Segment *Score::tick2segment(const Fraction &t, bool first, SegmentType st, bool use_m_mrest) const {
    Fraction tick(t);
    Measure *m;
    if (use_m_mrest) {
        m = tick2measure_mm(tick);
        // When mmRest force tick to the first segment of mmRest.
        if (m && m->is_mm_rest() && tick != m->end_tick()) {
            tick = m->tick();
        }
    } else {
        m = tick2measure(tick);
    }

    if (m == 0) {
        LOGD() << "no measure for tick " << tick.ticks();
        return 0;
    }
    for (Segment *segment = m->first(st); segment;) {
        Fraction t1 = segment->tick();
        Segment *nsegment = segment->next(st);
        if (tick == t1) {
            if (first) {
                return segment;
            } else {
                if (!nsegment || tick < nsegment->tick()) {
                    return segment;
                }
            }
        }
        segment = nsegment;
    }
    return 0;
}

Segment *Score::tick2segment(const Fraction &tick) const { return tick2segment(tick, false, SegmentType::ALL, false); }

Segment *Score::tick2segment(const Fraction &tick, bool first) const { return tick2segment(tick, first, SegmentType::ALL, false); }

//---------------------------------------------------------
//   tick2leftSegment
/// return the segment at this tick position if any or
/// the first segment *before* this tick position
//---------------------------------------------------------

Segment *Score::tick2left_segment(const Fraction &tick, bool use_m_mrest, SegmentType seg_type) const {
    Measure *m = use_m_mrest ? tick2measure_mm(tick) : tick2measure(tick);
    if (m == 0) {
        LOGD("tick2leftSegment(): not found tick %d", tick.ticks());
        return 0;
    }

    // loop over all segments
    Segment *ps = 0;
    for (Segment *s = m->first(seg_type); s; s = s->next(seg_type)) {
        if (tick < s->tick()) {
            return ps;
        } else if (tick == s->tick()) {
            return s;
        }
        ps = s;
    }
    return ps;
}

//---------------------------------------------------------
//   tick2rightSegment
/// return the segment at this tick position if any or
/// the first segment *after* this tick position
//---------------------------------------------------------

Segment *Score::tick2right_segment(const Fraction &tick, bool use_m_mrest, SegmentType seg_type) const {
    Measure *m = use_m_mrest ? tick2measure_mm(tick) : tick2measure(tick);
    if (m == 0) {
        //LOGD("tick2nearestSegment(): not found tick %d", tick.ticks());
        return 0;
    }
    // loop over all segments
    for (Segment *s = m->first(seg_type); s; s = s->next1(seg_type)) {
        if (tick <= s->tick()) {
            return s;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   tick2beatType
//---------------------------------------------------------

BeatType Score::tick2beat_type(const Fraction &tick) const {
    Measure *m = tick2measure(tick);
    Fraction msr_tick = m->tick();
    TimeSigFrac time_sig = sigmap()->timesig(msr_tick).nominal();

    int rtick = (tick - msr_tick).ticks();

    if (m->is_anacrusis()) { // measure is incomplete (anacrusis)
        rtick += time_sig.ticks_per_measure() - m->ticks().ticks();
    }

    return time_sig.rtick2beat_type(rtick);
}

void Score::check_chord_list() { chord_list_.check_chord_list(style()); }

//---------------------------------------------------------
//    next/prevChordNote
//
//    returns the top note of the next/previous chord. If a
//    chord exists in the same track as note,
//    it is used. If not, the topmost existing chord is used.
//    May return nullptr if there is no next/prev note
//---------------------------------------------------------

Note *next_chord_note(Note *note) {
    track_idx_t track = note->track();
    track_idx_t from_track = (track / ::mu::engraving::voices) * ::mu::engraving::voices;
    track_idx_t to_track = from_track + ::mu::engraving::voices;
    // TODO : limit to same instrument, not simply to same staff!
    Segment *seg = note->chord()->segment()->next_cr(track, true);
    while (seg) {
        EngravingItem *target_element = seg->element(track);
        // if a chord exists in the same track, return its top note
        if (target_element && target_element->is_chord()) {
            return to_chord(target_element)->up_note();
        }
        // if not, return topmost chord in track range
        for (track_idx_t i = from_track; i < to_track; i++) {
            target_element = seg->element(i);
            if (target_element && target_element->is_chord()) {
                return to_chord(target_element)->up_note();
            }
        }
        seg = seg->next_cr(track, true);
    }
    return nullptr;
}

Note *prev_chord_note(Note *note) {
    track_idx_t track = note->track();
    track_idx_t from_track = (track / ::mu::engraving::voices) * ::mu::engraving::voices;
    track_idx_t to_track = from_track + ::mu::engraving::voices;
    // TODO : limit to same instrument, not simply to same staff!
    Segment *seg = note->chord()->segment()->prev1();
    while (seg) {
        if (seg->segment_type() == SegmentType::CHORD_REST) {
            EngravingItem *target_element = seg->element(track);
            // if a chord exists in the same track, return its top note
            if (target_element && target_element->is_chord()) {
                return to_chord(target_element)->up_note();
            }
            // if not, return topmost chord in track range
            for (track_idx_t i = from_track; i < to_track; i++) {
                target_element = seg->element(i);
                if (target_element && target_element->is_chord()) {
                    return to_chord(target_element)->up_note();
                }
            }
        }
        seg = seg->prev1();
    }
    return nullptr;
}

//---------------------------------------------------------
//   pitchKeyAdjust
//    change entered note to sounding pitch dependent
//    on key.
//    Example: if F is entered in G-major, a Fis is played
//    key -7 ... +7
//---------------------------------------------------------

int pitch_key_adjust(int step, Key key) {
    static int ptab[15][7] = {
        //             c  d  e  f  g   a  b
        {-1, 1, 3, 4, 6, 8, 10}, // Bes
        {-1, 1, 3, 5, 6, 8, 10}, // Ges
        {0, 1, 3, 5, 6, 8, 10},  // Des
        {0, 1, 3, 5, 7, 8, 10},  // As
        {0, 2, 3, 5, 7, 8, 10},  // Es
        {0, 2, 3, 5, 7, 9, 10},  // B
        {0, 2, 4, 5, 7, 9, 10},  // F
        {0, 2, 4, 5, 7, 9, 11},  // C
        {0, 2, 4, 6, 7, 9, 11},  // G
        {1, 2, 4, 6, 7, 9, 11},  // D
        {1, 2, 4, 6, 8, 9, 11},  // A
        {1, 3, 4, 6, 8, 9, 11},  // E
        {1, 3, 4, 6, 8, 10, 11}, // H
        {1, 3, 5, 6, 8, 10, 11}, // Fis
        {1, 3, 5, 6, 8, 10, 12}, // Cis
    };
    return ptab[int(key) + 7][step];
}

//---------------------------------------------------------
//   y2pitch
//---------------------------------------------------------

int y2pitch(double y, ClefType clef, double spatium) {
    int l = lrint(y / spatium * 2.0);
    return line2pitch(l, clef, Key::C);
}

//---------------------------------------------------------
//   line2pitch
//    key  -7 ... +7
//---------------------------------------------------------

int line2pitch(int line, ClefType clef, Key key) {
    int l = std::max(ClefInfo::pitch_offset(clef) - line, 0);
    int octave = l / ::mu::engraving::step_delta_octave;
    l %= ::mu::engraving::step_delta_octave;

    int pitch = pitch_key_adjust(l, key) + octave * ::mu::engraving::pitch_delta_octave;
    return clamp_pitch(pitch);
}

static const char16_t *val_sharp[] = {u"c", u"c♯", u"d", u"d♯", u"e", u"f", u"f♯", u"g", u"g♯", u"a", u"a♯", u"b"};
static const char16_t *val_flat[] = {u"c", u"d♭", u"d", u"e♭", u"e", u"f", u"g♭", u"g", u"a♭", u"a", u"b♭", u"b"};

/*!
 * Returns the string representation of the given pitch.
 *
 * Returns the latin letter name, accidental, and octave numeral.
 * Uses upper case only for pitches 0-24.
 *
 * @param v
 *  The pitch number of the note.
 *
 * @return
 *  The string representation of the note.
 */
String pitch2string(int v, bool use_flats) {
    if (!pitch_is_valid(v)) {
        return String(u"----");
    }
    int octave = (v / ::mu::engraving::pitch_delta_octave) - 1;
    String o;
    o = String::number(octave);
    int i = v % ::mu::engraving::pitch_delta_octave;

    String pitch_str = use_flats ? val_flat[i] : val_sharp[i];
    if (octave < 0) {
        pitch_str = pitch_str.to_upper();
    }

    return pitch_str + o;
}

/*!
 * Returns the pitch number of the given string.
 *
 * @param s
 *  The string representation of the note.
 *
 * @return
 *  The pitch number of the note.
 */
int string2pitch(const String &s) {
    if (s == String(u"----")) {
        return ::mu::engraving::invalid_pitch;
    }

    String value = s;

    bool negative = s.contains(u'-');
    int octave = String(s[s.size() - 1]).to_int() * (negative ? -1 : 1);
    if (octave < -1 || octave > 9) {
        return -1;
    }

    value = value.mid(0, value.size() - (negative ? 2 : 1));
    value = value.to_lower();

    int pitch_index = -1;
    for (int i = 0; i < ::mu::engraving::pitch_delta_octave; ++i) {
        if (value == val_flat[i] || value == val_sharp[i]) {
            pitch_index = i;
            break;
        }
    }

    if (pitch_index == -1) {
        return -1;
    }

    return (octave + 1) * ::mu::engraving::pitch_delta_octave + pitch_index;
}

String convert_pitch_string_flats_and_sharps_to_unicode(const String &str) {
    if (str.is_empty()) {
        return String();
    }

    String value = String(str[0]);
    for (size_t i = 1; i < str.size(); ++i) {
        Char symbol = str.at(i).to_lower();
        if (symbol == u'b') {
            value.append(u'♭');
        } else if (symbol == u'#') {
            value.append(u'♯');
        } else {
            value.append(symbol);
        }
    }

    return value;
}

//---------------------------------------------------------
//   searchTieNote
//    search Note to tie to "note"
//---------------------------------------------------------

Note *search_tie_note(const Note *note, const Segment *next_segment, const bool disable_over_repeats) {
    if (!note) {
        return nullptr;
    }

    Note *note2 = nullptr;
    Chord *chord = note->chord();
    Segment *seg = chord->segment();

    if (!next_segment) {
        next_segment = seg->next1(SegmentType::CHORD_REST);
        while (next_segment && next_segment->tick() < chord->end_tick()) {
            next_segment = next_segment->next1(SegmentType::CHORD_REST);
        }
    }

    if (!next_segment) {
        return nullptr;
    }

    if (disable_over_repeats && !segments_are_adjacent(seg, next_segment)) {
        return nullptr;
    }

    if (chord->is_grace_before()) {
        chord = to_chord(chord->ownership_parent());

        // try to tie to next grace note

        size_t index = note->chord()->grace_index();
        for (Chord *c : chord->grace_notes()) {
            if (c->grace_index() == index + 1) {
                note2 = c->find_note(note->pitch());
                if (note2) {
                    return note2;
                }
            }
        }

        // try to tie to note in parent chord
        note2 = chord->find_note(note->pitch());
        if (note2) {
            return note2;
        }
    } else if (chord->is_grace_after()) {
        // grace after
        // we will try to tie to note in next normal chord, below
        // meanwhile, set chord to parent chord so the endTick calculation will make sense
        chord = to_chord(chord->ownership_parent());
    } else {
        // normal chord
        // try to tie to grace note after if present
        std::vector<Chord *> gna = chord->grace_notes_after();
        if (!gna.empty()) {
            Chord *gc = gna.front();
            note2 = gc->find_note(note->pitch());
            if (note2) {
                return note2;
            }
        }
    }
    // at this point, chord is a regular chord, not a grace chord
    // and we are looking for a note in the *next* chord (grace or regular)

    int idx1 = note->unison_index();
    Part *part = chord->part();
    const TrackRange track_range = part->track_range();
    for (track_idx_t track = track_range.start_track; track < track_range.end_track; ++track) {
        EngravingItem *e = next_segment->element(track);
        if (!e || !e->is_chord()) {
            continue;
        }
        Chord *c = to_chord(e);
        if (c->v_staff_idx() != chord->v_staff_idx()) {
            // this check is needed as we are iterating over all staves to capture cross-staff chords
            continue;
        }
        // if there are grace notes before, try to tie to first one
        std::vector<Chord *> gnb = c->grace_notes_before();
        if (!gnb.empty()) {
            Chord *gc = gnb.front();
            Note *gn2 = gc->find_note(note->pitch());
            if (gn2) {
                return gn2;
            }
        }
        int idx2 = 0;
        for (Note *n : c->notes()) {
            if (n->pitch() == note->pitch()) {
                if (idx1 == idx2) {
                    if (!note2 || c->track() == chord->track()) {
                        note2 = n;
                        break;
                    }
                } else {
                    ++idx2;
                }
            }
        }
    }
    return note2;
}

//---------------------------------------------------------
//   absStep
///   Compute absolute step.
///   C D E F G A B ....
//---------------------------------------------------------

int abs_step(int tpc, int pitch) {
    int line = tpc2step(tpc) + (pitch / ::mu::engraving::pitch_delta_octave) * ::mu::engraving::step_delta_octave;
    int tpc_pitch = tpc2pitch(tpc);

    if (tpc_pitch < ::mu::engraving::min_pitch) {
        line += ::mu::engraving::step_delta_octave;
    } else {
        line -= (tpc_pitch / ::mu::engraving::pitch_delta_octave) * ::mu::engraving::step_delta_octave;
    }
    return line;
}

int abs_step(int pitch) {
    // TODO - does this need to be key-aware?
    int tpc = pitch2tpc(pitch, Key::C, Prefer::NEAREST);
    return abs_step(tpc, pitch);
}

int abs_step(int line, ClefType clef) { return ClefInfo::pitch_offset(clef) - line; }

//---------------------------------------------------------
//   relStep
///   Compute relative step from absolute step
///   which depends on actual clef. Step 0 starts on the
///   first (top) staff line.
//---------------------------------------------------------

int rel_step(int line, ClefType clef) { return ClefInfo::pitch_offset(clef) - line; }

int rel_step(int pitch, int tpc, ClefType clef) { return rel_step(abs_step(tpc, pitch), clef); }

//---------------------------------------------------------
//   pitch2step
//   returns one of { 0, 1, 2, 3, 4, 5, 6 }
//---------------------------------------------------------

int pitch2step(int pitch) {
    //                            C  C# D  D# E  F  F# G  G# A  A# B
    static const char tab[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    return tab[pitch % 12];
}

//---------------------------------------------------------
//   step2pitch
//   returns one of { 0, 2, 4, 5, 7, 9, 11 }
//---------------------------------------------------------

int step2pitch(int step) {
    static const char tab[7] = {0, 2, 4, 5, 7, 9, 11};
    return tab[step % 7];
}

//---------------------------------------------------------
//   convertLine
// find the line in clefF corresponding to lineL2 in clefR
//---------------------------------------------------------

int convert_line(int line_l2, ClefType clef_l, ClefType clef_r) {
    int line_r2 = line_l2;
    int goalpitch = line2pitch(line_l2, clef_l, Key::C);
    int p;
    while ((p = line2pitch(line_r2, clef_r, Key::C)) > goalpitch && p < ::mu::engraving::max_pitch) {
        line_r2++;
    }
    while ((p = line2pitch(line_r2, clef_r, Key::C)) < goalpitch && p > ::mu::engraving::min_pitch) {
        line_r2--;
    }
    return line_r2;
}

//---------------------------------------------------------
//   convertLine
// find the line in clef for NoteL corresponding to lineL2 in clef for noteR
// for example middle C is line 10 in Treble clef, but is line -2 in Bass clef.
//---------------------------------------------------------

int convert_line(int line_l2, const Note *note_l, const Note *note_r) {
    return convert_line(line_l2, note_l->chord()->staff()->clef(note_l->chord()->tick()),
                        note_r->chord()->staff()->clef(note_r->chord()->tick()));
}

//---------------------------------------------------------
//   chromaticPitchSteps -- an articulation such as a trill, or mordant consists of several notes
// played in succession.  The pitch offsets of each such note in the sequence can be represented either
// as a number of steps in the diatonic scale, or in half steps as on a piano keyboard.
// this function, articulationExcursion, takes deltastep indicating the number of steps in the
// diatonic scale, and calculates (and returns) the number of half steps, taking several things into account.
// E.g., the key signature, a trill from e to f, is to be understood as a trill between E and F# if we are
// in the key of G.
// E.g., if previously (looking backward in time) in the same measure there is another note on the same
// staff line/space, and that note has an accidental (sharp,flat,natural,etc), then we want to match that
// tone exactly.
// E.g., If there are multiple notes on the same line/space, then we only consider the most
// recent one, but avoid looking forward in time after the current note.
// E.g., Also if there is an accidental     // on a note one (or more) octaves above or below we
// observe its accidental as well.
// E.g., Still another case is that if two staves are involved (such as a glissando between two
// notes on different staves) then we have to search both staves for the most recent accidental.
//
// noteL is the note to measure the deltastep from, i.e., ornaments are w.r.t. this note
// noteR is the note to search backward from to find accidentals.
//    for ornament calculation noteL and noteR are the same, but for glissando they are
//     the start and end note of glissando.
// nominalDiatonicSteps is the desired number of diatonic steps between the base note and this articulation step.
//---------------------------------------------------------

int chromatic_pitch_steps(const Note *note_l, const Note *note_r, const int nominal_diatonic_steps) {
    if (0 == nominal_diatonic_steps) {
        return 0;
    }
    Chord *chord_l = note_l->chord();
    Chord *chord_r = note_r->chord();
    int epitch_l = note_l->epitch();
    Fraction tick_l = chord_l->tick();
    // we cannot use staffL = chord->staff() because that won't correspond to the noteL->line()
    //   in the case the user has pressed Shift-Cmd->Up or Shift-Cmd-Down.
    //   Therefore we have to take staffMove() into account using vStaffIdx().
    Staff *staff_l = note_l->score()->staff(chord_l->v_staff_idx());
    ClefType clef_l = staff_l->clef(tick_l);
    // line represents the ledger line of the staff.  0 is the top line, 1, is the space between the top 2 lines,
    //  ... 8 is the bottom line.
    int line_l = note_l->line();
    if (line_l == ::mu::engraving::invalid_line) {
        int abs_line = abs_step(note_l->tpc(), note_l->epitch());
        ClefType clef = note_l->staff()->clef(note_l->tick());
        line_l = rel_step(abs_line, clef);
    }

    // we use line - deltastep, because lines are oriented from top to bottom, while step is oriented from bottom to top.
    int line_l2 = line_l - nominal_diatonic_steps;
    Measure *measure_r = chord_r->segment()->measure();

    Segment *segment = note_l->chord()->segment();
    int line_r2 = convert_line(line_l2, note_l, note_r);
    // is there another note in this segment on the same line?
    // if so, use its pitch exactly.
    int halfsteps = 0;
    staff_idx_t staff_idx = note_l->chord()->staff()->idx(); // cannot use staffL->idx() because of staffMove()
    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    bool done = false;
    for (track_idx_t track = start_track; track < end_track; ++track) {
        EngravingItem *e = segment->element(track);
        if (!e || !e->is_chord()) {
            continue;
        }
        Chord *chord = to_chord(e);
        if (chord->v_staff_idx() != chord_l->v_staff_idx()) {
            continue;
        }
        if (!done) {
            if (staff_l->is_pitched_staff(segment->tick())) {
                bool error = false;
                AccidentalVal acciv2 = measure_r->find_accidental(chord_r->segment(), chord_r->v_staff_idx(), line_r2, error);
                int acci2 = int(acciv2);
                // epitch (effective pitch) is a visible pitch so line2pitch returns exactly that.
                halfsteps = line2pitch(line_l - nominal_diatonic_steps, clef_l, Key::C) + acci2 - epitch_l;
            } else {
                // cannot rely on accidentals or key signatures
                halfsteps = nominal_diatonic_steps;
            }
        }
    }
    return halfsteps;
}

static void note_val_to_effective_pitch_and_tpc(const NoteVal &nval, const Staff *staff, const Fraction &tick, int &epitch, int &tpc) {
    const bool concert_pitch = staff->concert_pitch();

    if (concert_pitch) {
        epitch = nval.pitch;
    } else {
        const int pitch_offset = staff->origin_part(tick)->instrument(tick)->transpose().chromatic;
        epitch = nval.pitch - pitch_offset;
    }

    tpc = nval.tpc(concert_pitch);
    if (tpc == static_cast<int>(mu::engraving::Tpc::TPC_INVALID)) {
        tpc = pitch2tpc(epitch, staff->key(tick), mu::engraving::Prefer::NEAREST);
    }
}

int note_val_to_line(const NoteVal &nval, const Staff *staff, const Fraction &tick) {
    if (staff->is_drum_staff(tick)) {
        const Drumset *drumset = staff->origin_part(tick)->instrument(tick)->drumset();
        if (drumset) {
            return drumset->line(nval.pitch);
        }
    }

    if (nval.is_rest()) {
        return staff->middle_line(tick);
    }

    int epitch = nval.pitch;
    int tpc = static_cast<int>(mu::engraving::Tpc::TPC_INVALID);
    note_val_to_effective_pitch_and_tpc(nval, staff, tick, epitch, tpc);

    return rel_step(epitch, tpc, staff->clef(tick));
}

AccidentalVal note_val_to_accidental_val(const NoteVal &nval, const Staff *staff, const Fraction &tick) {
    if (nval.is_rest()) {
        return AccidentalVal::NATURAL;
    }

    if (staff->is_drum_staff(tick)) {
        return AccidentalVal::NATURAL;
    }

    int epitch = nval.pitch;
    int tpc = static_cast<int>(mu::engraving::Tpc::TPC_INVALID);
    note_val_to_effective_pitch_and_tpc(nval, staff, tick, epitch, tpc);

    return tpc2alter(tpc);
}

int compare_notes_pos(const Note *n1, const Note *n2) {
    if (n1->line() != n2->line() && !(n1->staff_type()->is_tab_staff())) {
        return n2->line() - n1->line();
    } else if (n1->string() != n2->string()) {
        return n2->string() - n1->string();
    } else {
        return n1->pitch() - n2->pitch();
    }
}

//---------------------------------------------------------
//   skipTuplet
//    return segment of rightmost chord/rest in a
//    (possible nested) tuplet
//---------------------------------------------------------

Segment *skip_tuplet(Tuplet *tuplet) {
    DurationElement *nde = tuplet->elements().back();
    while (nde->is_tuplet()) {
        tuplet = to_tuplet(nde);
        nde = tuplet->elements().back();
    }
    return to_chord_rest(nde)->segment();
}

//---------------------------------------------------------
//   timeSigSymIdsFromString
//    replace ascii with bravura symbols
//---------------------------------------------------------

SymIdList time_sig_sym_ids_from_string(const String &string, TimeSigStyle time_sig_style) {
    static const std::map<Char, SymId> dict = {
        {'+', SymId::TIME_SIG_PLUS_SMALL},
        {'0', SymId::TIME_SIG0},
        {'1', SymId::TIME_SIG1},
        {'2', SymId::TIME_SIG2},
        {'3', SymId::TIME_SIG3},
        {'4', SymId::TIME_SIG4},
        {'5', SymId::TIME_SIG5},
        {'6', SymId::TIME_SIG6},
        {'7', SymId::TIME_SIG7},
        {'8', SymId::TIME_SIG8},
        {'9', SymId::TIME_SIG9},
        {'C', SymId::TIME_SIG_COMMON},
        {'(', SymId::TIME_SIG_PARENS_LEFT_SMALL},
        {')', SymId::TIME_SIG_PARENS_RIGHT_SMALL},
        {'[', SymId::TIME_SIG_BRACKET_LEFT_SMALL},
        {']', SymId::TIME_SIG_BRACKET_RIGHT_SMALL},
        {u'¢', SymId::TIME_SIG_CUT_COMMON},
        {u'½', SymId::TIME_SIG_FRACTION_HALF},
        {u'¼', SymId::TIME_SIG_FRACTION_QUARTER},
        {'*', SymId::TIME_SIG_MULTIPLY},
        {'X', SymId::TIME_SIG_MULTIPLY},
        {'x', SymId::TIME_SIG_MULTIPLY},
        {u'×', SymId::TIME_SIG_MULTIPLY},
        {59664, SymId::MENSURAL_PROLATION1},
        {79, SymId::MENSURAL_PROLATION2}, // 'O'
        {59665, SymId::MENSURAL_PROLATION2},
        {216, SymId::MENSURAL_PROLATION3}, // 'Ø'
        {59666, SymId::MENSURAL_PROLATION3},
        {59667, SymId::MENSURAL_PROLATION4},
        {59668, SymId::MENSURAL_PROLATION5},
        {59669, SymId::MENSURAL_PROLATION6},
        {59670, SymId::MENSURAL_PROLATION7},
        {59671, SymId::MENSURAL_PROLATION8},
        {59672, SymId::MENSURAL_PROLATION9},
        {59673, SymId::MENSURAL_PROLATION10},
        {59674, SymId::MENSURAL_PROLATION11},
    };

    static const std::map<Char, SymId> dict_large = {
        {'+', SymId::TIME_SIG_PLUS_SMALL_LARGE},
        {'0', SymId::TIME_SIG0_LARGE},
        {'1', SymId::TIME_SIG1_LARGE},
        {'2', SymId::TIME_SIG2_LARGE},
        {'3', SymId::TIME_SIG3_LARGE},
        {'4', SymId::TIME_SIG4_LARGE},
        {'5', SymId::TIME_SIG5_LARGE},
        {'6', SymId::TIME_SIG6_LARGE},
        {'7', SymId::TIME_SIG7_LARGE},
        {'8', SymId::TIME_SIG8_LARGE},
        {'9', SymId::TIME_SIG9_LARGE},
        {'C', SymId::TIME_SIG_COMMON_LARGE},
        {'(', SymId::TIME_SIG_PARENS_LEFT_SMALL_LARGE},
        {')', SymId::TIME_SIG_PARENS_RIGHT_SMALL_LARGE},
        {'[', SymId::TIME_SIG_BRACKET_LEFT_SMALL_LARGE},
        {']', SymId::TIME_SIG_BRACKET_RIGHT_SMALL_LARGE},
        {u'¢', SymId::TIME_SIG_CUT_COMMON_LARGE},
        {u'½', SymId::TIME_SIG_FRACTION_HALF_LARGE},
        {u'¼', SymId::TIME_SIG_FRACTION_QUARTER_LARGE},
        {'*', SymId::TIME_SIG_MULTIPLY_LARGE},
        {'X', SymId::TIME_SIG_MULTIPLY_LARGE},
        {'x', SymId::TIME_SIG_MULTIPLY_LARGE},
        {u'×', SymId::TIME_SIG_MULTIPLY_LARGE},
    };

    static const std::map<Char, SymId> dict_narrow = {
        {'+', SymId::TIME_SIG_PLUS_SMALL_NARROW},
        {'0', SymId::TIME_SIG0_NARROW},
        {'1', SymId::TIME_SIG1_NARROW},
        {'2', SymId::TIME_SIG2_NARROW},
        {'3', SymId::TIME_SIG3_NARROW},
        {'4', SymId::TIME_SIG4_NARROW},
        {'5', SymId::TIME_SIG5_NARROW},
        {'6', SymId::TIME_SIG6_NARROW},
        {'7', SymId::TIME_SIG7_NARROW},
        {'8', SymId::TIME_SIG8_NARROW},
        {'9', SymId::TIME_SIG9_NARROW},
        {'C', SymId::TIME_SIG_COMMON_NARROW},
        {'(', SymId::TIME_SIG_PARENS_LEFT_SMALL_NARROW},
        {')', SymId::TIME_SIG_PARENS_RIGHT_SMALL_NARROW},
        {'[', SymId::TIME_SIG_BRACKET_LEFT_SMALL_NARROW},
        {']', SymId::TIME_SIG_BRACKET_RIGHT_SMALL_NARROW},
        {u'¢', SymId::TIME_SIG_CUT_COMMON_NARROW},
        {u'½', SymId::TIME_SIG_FRACTION_HALF_NARROW},
        {u'¼', SymId::TIME_SIG_FRACTION_QUARTER_NARROW},
        {'*', SymId::TIME_SIG_MULTIPLY_NARROW},
        {'X', SymId::TIME_SIG_MULTIPLY_NARROW},
        {'X', SymId::TIME_SIG_MULTIPLY_NARROW},
        {u'×', SymId::TIME_SIG_MULTIPLY_NARROW},
    };

    SymIdList list;
    for (size_t i = 0; i < string.size(); ++i) {
        SymId sym = muse::value(time_sig_style == TimeSigStyle::NARROW ? dict_narrow :
                                time_sig_style == TimeSigStyle::LARGE  ? dict_large :
                                                                         dict,
                                string.at(i), SymId::NO_SYM);
        if (sym != SymId::NO_SYM) {
            list.push_back(sym);
        }
    }
    return list;
}

//---------------------------------------------------------
//   actualTicks
//---------------------------------------------------------

Fraction actual_ticks(Fraction duration, Tuplet *tuplet, Fraction time_stretch) {
    Fraction f = duration / time_stretch;
    for (Tuplet *t = tuplet; t; t = t->tuplet()) {
        f /= t->ratio();
    }
    return f;
}

bool drag_position_to_measure(const PointF &pos, const Score *score, Measure **measure, staff_idx_t *staff_idx,
                              const double spacing_factor) {
    const System *preferred_system = (*measure) ? (*measure)->system() : nullptr;

    MeasureBase *mb = score->search_measure_base(pos, /*scanMeasuresOnly*/ true, preferred_system, spacing_factor);
    Measure *m = mb && mb->is_measure() ? to_measure(mb) : nullptr;
    if (!m) {
        return false;
    }

    const System *system = m->system();
    const double y = pos.y() - system->canvas_pos().y();
    const staff_idx_t i = system->search_staff(y, *staff_idx, spacing_factor);
    if (!score->staff(i)) {
        return false;
    }

    *measure = m;
    *staff_idx = i;
    return true;
}

bool drag_position_to_segment(const PointF &pos, const Measure *measure, const staff_idx_t staff_idx, Segment **segment,
                              const double spacing_factor, const bool allow_time_anchor) {
    const track_idx_t strack = staff_idx * ::mu::engraving::voices;
    const track_idx_t etrack = strack + ::mu::engraving::voices;

    const double x = pos.x() - measure->canvas_pos().x();
    const SegmentType st = allow_time_anchor ? SegmentType::DURATION : SegmentType::CHORD_REST;
    Segment *s = measure->search_segment(x, st, strack, etrack, *segment, spacing_factor);
    if (!s) {
        return false;
    }

    *segment = segment_or_chord_rest_segment_at_same_tick(s);
    return true;
}

Segment *segment_or_chord_rest_segment_at_same_tick(Segment *segment) {
    IF_ASSERT_FAILED(segment) { return nullptr; }

    // If TimeTick and ChordRest segments are at the same tick, prefer ChordRest
    if (segment->is_time_tick_type() && segment->measure()) {
        if (Segment *cr_seg_at_same_tick = segment->measure()->find_segment_r(SegmentType::CHORD_REST, segment->rtick())) {
            return cr_seg_at_same_tick;
        }
    }

    return segment;
}

double y_staff_difference(const System *system1, const System *system2, staff_idx_t staff_idx) {
    if (!system1 || !system2) {
        return 0.0;
    }

    const SysStaff *staff1 = system1->staff(staff_idx);
    const SysStaff *staff2 = system2->staff(staff_idx);
    if (!staff1 || !staff2) {
        return 0.0;
    }
    return staff1->y() - staff2->y();
}

bool allow_remove_when_removing_staves(EngravingItem *item, staff_idx_t start_staff, staff_idx_t end_staff) {
    // Sanity checks
    if (!item || item->staff_idx() == muse::nidx || start_staff == muse::nidx || end_staff == muse::nidx) {
        return false;
    }

    Score *score = item->score();
    if (score->nstaves() == 1) {
        return true;
    }

    if (end_staff == 0) { // Default initialized
        end_staff = start_staff + 1;
    }

    staff_idx_t staff_idx = item->staff_idx();
    if (staff_idx < start_staff || staff_idx >= end_staff) {
        return false;
    }

    Staff *next_remaining = score->staff(end_staff);
    bool next_remaining_is_system_object_staff = next_remaining && next_remaining->is_system_object_staff();
    if (item->is_top_system_object() && !next_remaining_is_system_object_staff) {
        return false;
    }

    return true;
}

bool move_down_when_adding_staves(EngravingItem *item, staff_idx_t start_staff, staff_idx_t end_staff) {
    // Sanity checks
    if (!item || item->staff_idx() == muse::nidx || start_staff == muse::nidx || end_staff == muse::nidx) {
        return false;
    }

    if (item->staff_idx() < start_staff) {
        return false;
    }

    if (end_staff == 0) { // Default initialized
        end_staff = start_staff + 1;
    }

    Score *score = item->score();
    Staff *next_after_inserted = score->staff(end_staff);
    bool next_after_inserted_is_system_object_staff = next_after_inserted && next_after_inserted->is_system_object_staff();
    if (item->is_top_system_object() && !next_after_inserted_is_system_object_staff) {
        return false;
    }

    return true;
}

void collect_chords_and_rest(Segment *segment, staff_idx_t staff_idx, std::vector<Chord *> &chords, std::vector<Rest *> &rests) {
    if (!segment) {
        return;
    }

    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;

    for (track_idx_t track = start_track; track < end_track; ++track) {
        EngravingItem *e = segment->element(track);
        if (!e) {
            continue;
        }
        if (e->is_chord() && !to_chord_rest(e)->staff_move()) {
            chords.push_back(to_chord(e));
        } else if (e->is_rest() && !to_chord_rest(e)->staff_move() && !to_rest(e)->is_full_measure_rest()) {
            rests.push_back(to_rest(e));
        }
    }
}

void collect_chords_overlapping_rests(Segment *segment, staff_idx_t staff_idx, std::vector<Chord *> &chords) {
    // Check if previous segments contain chords in other voices
    // whose duration overlaps with rests on this segment

    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;

    std::set<track_idx_t> tracks_to_check;
    for (track_idx_t track = start_track; track < end_track; ++track) {
        EngravingItem *item = segment->element(track);
        if (!item || !item->is_rest()) {
            tracks_to_check.insert(track);
        }
    }

    Fraction cur_tick = segment->rtick();
    for (Segment *prev_seg = segment->prev(); prev_seg; prev_seg = prev_seg->prev()) {
        if (!prev_seg->is_chord_rest_type()) {
            continue;
        }
        Fraction prev_seg_tick = prev_seg->rtick();
        for (track_idx_t track : tracks_to_check) {
            EngravingItem *e = prev_seg->element(track);
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(e);
            Fraction chord_end_tick = prev_seg_tick + chord->actual_ticks();
            if (chord_end_tick <= cur_tick) {
                continue;
            }

            chords.push_back(chord);
        }
    }
}

std::vector<EngravingItem *> collect_system_objects(const Score *score, const std::vector<Staff *> &staves) {

    std::vector<EngravingItem *> result;

    const TimeSigPlacement time_sig_placement = score->style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>();
    const bool is_on_staff_time_sig = time_sig_placement != TimeSigPlacement::NORMAL;

    for (const Measure *measure = score->first_measure(); measure; measure = measure->next_measure()) {
        for (EngravingItem *measure_element : measure->el()) {
            if (!measure_element || !measure_element->system_flag() || measure_element->is_layout_break()) {
                continue;
            }
            if (!staves.empty()) {
                if (muse::contains(staves, measure_element->staff())) {
                    result.push_back(measure_element);
                }
            } else if (measure_element->is_top_system_object()) {
                result.push_back(measure_element);
            }
        }

        for (const Segment &seg : measure->segments()) {
            if (seg.is_type(SegmentType::DURATION | SegmentType::END_BAR_LINE)) {
                for (EngravingItem *annotation : seg.annotations()) {
                    if (!annotation || !annotation->system_flag()) {
                        continue;
                    }

                    if (!staves.empty()) {
                        if (muse::contains(staves, annotation->staff())) {
                            result.push_back(annotation);
                        }
                    } else if (annotation->is_top_system_object()) {
                        result.push_back(annotation);
                    }
                }
            }

            if (is_on_staff_time_sig && seg.is_type(SegmentType::TIME_SIG_TYPES)) {
                for (EngravingItem *item : seg.elist()) {
                    if (!item || !item->is_time_sig()) {
                        continue;
                    }

                    if (!staves.empty()) {
                        if (muse::contains(staves, item->staff())) {
                            result.push_back(item);
                        }
                    } else if (item->staff_idx() == 0) {
                        result.push_back(item);
                    }
                }
            }
        }
    }

    for (const auto &pair : score->spanner()) {
        Spanner *spanner = pair.second;
        if (!spanner->system_flag()) {
            continue;
        }

        if (!staves.empty()) {
            if (muse::contains(staves, spanner->staff())) {
                result.push_back(spanner);
            }
        } else if (spanner->is_top_system_object()) {
            result.push_back(spanner);
        }
    }

    return result;
}

std::unordered_set<EngravingItem *> collect_elements_anchored_to_chord_rest(const ChordRest *cr) {
    std::unordered_set<EngravingItem *> elems;
    for (EngravingItem *lyric : cr->lyrics()) {
        elems.emplace(lyric);
    }
    if (!cr->is_chord()) {
        return elems;
    }
    const Chord *chord = to_chord(cr);
    if (Arpeggio *arp = chord->arpeggio()) {
        elems.emplace(arp);
    }
    if (TremoloTwoChord *trem_two = chord->tremolo_two_chord()) {
        elems.emplace(trem_two);
    }
    if (TremoloSingleChord *trem_sing = chord->tremolo_single_chord()) {
        elems.emplace(trem_sing);
    }
    for (Articulation *art : chord->articulations()) {
        elems.emplace(art);
    }
    // Chord brackets and chord lines
    for (EngravingItem *e : chord->el()) {
        elems.emplace(e);
    }
    for (Chord *grace : chord->grace_notes()) {
        elems.emplace(grace);
        for (Articulation *g_art : grace->articulations()) {
            elems.emplace(g_art);
        }
        if (TremoloSingleChord *g_trem_sing = grace->tremolo_single_chord()) {
            elems.emplace(g_trem_sing);
        }
    }
    return elems;
}

std::unordered_set<EngravingItem *> collect_elements_anchored_to_note(const Note *note, bool include_forward_ties_spanners,
                                                                      bool include_backward_ties_spanners) {
    std::unordered_set<EngravingItem *> elems;
    LaissezVib *lv = note->laissez_vib();
    if (lv && !lv->segments_empty()) {
        elems.emplace(lv);
    }
    PartialTie *ipt = note->incoming_partial_tie();
    if (ipt && !ipt->segments_empty()) {
        elems.emplace(ipt);
    }
    PartialTie *opt = note->outgoing_partial_tie();
    if (opt && !opt->segments_empty()) {
        elems.emplace(opt);
    }
    // The following is a bit of a hack - addressing properly would require a fingering rework...
    for (EngravingItem *elem : note->el()) {
        if (elem->is_fingering()) {
            elems.emplace(elem);
        }
    }
    if (include_forward_ties_spanners) {
        Tie *tie_for = note->tie_for();
        if (tie_for && !tie_for->segments_empty()) {
            elems.emplace(tie_for);
        }
        for (Spanner *sp : note->spanner_for()) {
            if (sp->segments_empty()) {
                continue;
            }
            elems.emplace(sp);
        }
    }
    if (include_backward_ties_spanners) {
        Tie *tie_back = note->tie_back();
        if (tie_back && !tie_back->segments_empty()) {
            elems.emplace(tie_back);
        }
        for (Spanner *sp : note->spanner_back()) {
            if (sp->segments_empty()) {
                continue;
            }
            elems.emplace(sp);
        }
    }
    const NoteParenthesisInfo *note_paren_info = note->parenthesis_info();
    if (note_paren_info && note_paren_info->notes().size()) {
        elems.emplace(note_paren_info->left_paren());
        elems.emplace(note_paren_info->right_paren());
    }
    return elems;
}

bool note_anchored_spanner_is_in_range(const Spanner *spanner, const Fraction &range_start, const Fraction &range_end) {
    IF_ASSERT_FAILED(range_start < range_end) { return false; }
    const EngravingItem *start_element = spanner->start_element();
    const EngravingItem *end_element = spanner->end_element();
    IF_ASSERT_FAILED(start_element && start_element->is_note() && end_element && end_element->is_note()) {
        LOGD() << "Cannot calculate isInRange - might be a partial tie or laissez vibrer";
        return false;
    }
    const Note *start_note = to_note(start_element);
    const Segment *start_seg = start_note->chord()->segment();
    if (start_seg->tick() < range_start) {
        return false;
    }
    const Note *end_note = to_note(end_element);
    const Segment *end_seg = end_note->chord()->segment();
    return !end_seg || end_seg->tick() <= range_end;
}

String format_unique_excerpt_name(const String &base_name, const StringList &all_excerpt_lower_names) {
    String result = base_name;
    int num = 0;

    while (all_excerpt_lower_names.contains(result.to_lower())) {
        result = base_name + String(u" (%1)").arg(++num);
    }

    return result;
}

bool is_first_system_key_sig(const KeySig *ks) {
    if (!ks) {
        return false;
    }
    const System *sys = ks->measure()->system();
    if (!sys) {
        return false;
    }
    return ks->tick() == sys->first_measure()->tick();
}

String bend_amount_to_string(int fulls, int quarts, bool use_fractions) {
    String string = fulls != 0 ? String::number(fulls) : String();

    if (use_fractions) {
        switch (std::abs(quarts)) {
            case 1: string += u"\u00BC"; break;
            case 2: string += u"\u00BD"; break;
            case 3: string += u"\u00BE"; break;
            default: break;
        }
    } else {
        if (!string.empty() && quarts != 0) {
            string += u" ";
        }
        switch (std::abs(quarts)) {
            case 1: string += u"1/4"; break;
            case 2: string += u"1/2"; break;
            case 3: string += u"3/4"; break;
            default: break;
        }
    }

    return string;
}

InstrumentTrackId make_instrument_track_id(const EngravingItem *item) {
    const Part *part = item->part();
    if (!part) {
        return InstrumentTrackId();
    }

    InstrumentTrackId track_id{part->id(), part->instrument_id(item->tick())};

    return track_id;
}

std::vector<Measure *> find_following_repeat_measures(const Measure *measure) {
    const MasterScore *master = measure->master_score();
    const Score *score = measure->score();

    const Measure *master_measure = master->tick2measure_mm(measure->tick());

    const RepeatList &repeat_list = master->repeat_list(true, false);

    std::vector<Measure *> measures;

    for (auto it = repeat_list.begin(); it != repeat_list.end(); it++) {
        const RepeatSegment *rs = *it;
        const auto next_seg_it = std::next(it);
        if (!rs->ends_with_measure(master_measure) || next_seg_it == repeat_list.end()) {
            continue;
        }

        // Get next segment
        const RepeatSegment *next_seg = *next_seg_it;
        const Measure *first_master_measure = next_seg->first_measure();
        Measure *first_measure = first_master_measure ? score->tick2measure_mm(first_master_measure->tick()) : nullptr;
        if (!first_measure) {
            continue;
        }

        measures.push_back(first_measure);
    }

    return measures;
}

std::vector<Measure *> find_previous_repeat_measures(const Measure *measure) {
    const MasterScore *master = measure->master_score();
    const Score *score = measure->score();

    const Measure *master_measure = master->tick2measure_mm(measure->tick());

    const RepeatList &repeat_list = master->repeat_list(true, false);

    std::vector<Measure *> measures;

    if (repeat_list.empty()) {
        return measures;
    }

    for (auto it = repeat_list.begin() + 1; it != repeat_list.end(); it++) {
        const RepeatSegment *rs = *it;
        const auto prev_seg_it = std::prev(it);
        if (!rs->starts_with_measure(master_measure) || prev_seg_it == repeat_list.end()) {
            continue;
        }

        // Get next segment
        const RepeatSegment *prev_seg = *prev_seg_it;
        const Measure *last_master_measure = prev_seg->last_measure();
        Measure *last_measure = last_master_measure ? score->tick2measure_mm(last_master_measure->tick()) : nullptr;
        if (!last_measure) {
            continue;
        }

        measures.push_back(last_measure);
    }

    return measures;
}

bool repeat_has_partial_lyric_line(const Measure *end_repeat_measure) {
    const std::vector<Measure *> measures = find_following_repeat_measures(end_repeat_measure);
    const Score *score = end_repeat_measure->score();

    for (const Measure *measure : measures) {
        const SpannerMap::IntervalList &spanners =
            score->spanner_map().find_overlapping(measure->tick().ticks(), measure->end_tick().ticks());

        for (auto &spanner : spanners) {
            if (spanner.value->is_partial_lyrics_line() && spanner.start == measure->tick().ticks()) {
                return true;
            }
        }
    }

    return false;
}

bool segments_are_adjacent(const Segment *first_seg, const Segment *second_seg) {
    if (!first_seg || !second_seg) {
        return false;
    }
    const MasterScore *master = first_seg->master_score();

    const Measure *first_measure = first_seg->measure();
    const Measure *second_measure = second_seg->measure();

    if (first_measure == second_measure) {
        return true;
    }

    const Measure *first_master_measure = master->tick2measure(first_measure->tick());
    const Measure *second_master_measure = master->tick2measure(second_measure->tick());

    if (first_master_measure) {
        first_master_measure = first_master_measure->covering_mm_rest_or_this();
    }
    if (second_master_measure) {
        second_master_measure = second_master_measure->covering_mm_rest_or_this();
    }

    IF_ASSERT_FAILED(first_master_measure && second_master_measure) { return false; }

    const Score *score = first_seg->score();
    const RepeatList &repeat_list = score->repeat_list(true, false);

    std::vector<const Measure *> measures;

    bool first_measure_segment_found = false;
    bool second_measure_segment_found = false;

    for (auto it = repeat_list.begin(); it != repeat_list.end(); it++) {
        const RepeatSegment *rs = *it;
        const auto next_seg_it = std::next(it);

        // Check if measures are in the same repeat segment
        bool contains_first_measure = rs->contains_measure(first_master_measure);
        bool contains_second_measure = rs->contains_measure(second_master_measure);
        first_measure_segment_found |= contains_first_measure;
        second_measure_segment_found |= contains_second_measure;

        if (contains_first_measure && contains_second_measure) {
            return true;
        }

        // Continue to build list of measures at the start of following repeat segments
        if (!rs->ends_with_measure(first_master_measure) || next_seg_it == repeat_list.end()) {
            continue;
        }

        // Get next segment
        const RepeatSegment *next_seg = *next_seg_it;
        const Measure *next_seg_first_measure = next_seg->first_measure();
        if (!next_seg_first_measure) {
            continue;
        }

        measures.push_back(next_seg_first_measure);
    }

    // Check if second segment is in a following measure in the repeat structure
    for (const Measure *m : measures) {
        if (m == second_master_measure) {
            return true;
        }
    }

    if (!first_measure_segment_found && !second_measure_segment_found) {
        // The measures are outside of the (invalid) repeat structure
        return first_master_measure->next_measure() == second_master_measure;
    }

    return false;
}

bool segments_are_in_different_repeat_segments(const Segment *first_seg, const Segment *second_seg) {
    if (!first_seg || !second_seg) {
        return false;
    }
    const MasterScore *master = first_seg->master_score();

    Measure *first_measure = first_seg->measure();
    Measure *second_measure = second_seg->measure();

    if (first_measure == second_measure) {
        return false;
    }

    const Measure *first_master_measure = master->tick2measure(first_measure->tick());
    const Measure *second_master_measure = master->tick2measure(second_measure->tick());

    Score *score = first_seg->score();

    const RepeatList &repeat_list = score->repeat_list(true, false);

    std::vector<const Measure *> measures;

    for (auto it = repeat_list.begin(); it != repeat_list.end(); it++) {
        const RepeatSegment *rs = *it;

        if (!rs->contains_measure(first_master_measure) || !rs->contains_measure(second_master_measure)) {
            return true;
        }
    }

    return false;
}

bool is_valid_bar_line_for_repeat_section(const Segment *first_seg, const Segment *second_seg) {
    if (!first_seg || !second_seg) {
        return false;
    }
    if (!first_seg->is_type(SegmentType::BAR_LINE_TYPES)) {
        return false;
    }

    const MasterScore *master = first_seg->master_score();

    Measure *first_measure = first_seg->measure();
    Measure *second_measure = second_seg->measure();

    const Measure *first_master_measure = master->tick2measure(first_measure->tick());
    const Measure *second_master_measure = master->tick2measure(second_measure->tick());
    const Measure *adjacent_master_measure = first_master_measure->next_measure();

    Score *score = first_seg->score();

    const RepeatList &repeat_list = score->repeat_list(true, false);

    std::vector<const Measure *> measures;

    bool seg_ends_with_bl = false;
    bool adjacent_and_second_share_segment = false;

    for (auto it = repeat_list.begin(); it != repeat_list.end(); it++) {
        const RepeatSegment *rs = *it;

        if (rs->ends_with_measure(first_master_measure)) {
            seg_ends_with_bl = true;
        }

        if (rs->starts_with_measure(adjacent_master_measure) && rs->contains_measure(second_master_measure)) {
            adjacent_and_second_share_segment = true;
        }
    }

    return seg_ends_with_bl && adjacent_and_second_share_segment;
}

PartialLyricsLine *find_prev_partial_lyrics_line_dash(Lyrics *lyrics) {
    Score *score = lyrics->score();
    for (auto sp : score->spanner_map().find_overlapping(lyrics->tick().ticks(), lyrics->tick().ticks())) {
        if (!sp.value->is_partial_lyrics_line() || sp.value->track() != lyrics->track()) {
            continue;
        }
        PartialLyricsLine *partial_line = to_partial_lyrics_line(sp.value);
        if (partial_line->is_end_melisma() || partial_line->verse() != lyrics->verse() ||
            partial_line->placement() != lyrics->placement()) {
            continue;
        }
        return partial_line;
    }

    return nullptr;
}

MeasureBeat find_beat(const Score *score, int tick) {
    MeasureBeat measure_beat;
    if (!score || !score->check_has_measures()) {
        return measure_beat;
    }

    int ticks = 0;
    int beat_index = 0;
    score->sigmap()->tick_values(tick, &measure_beat.measure_index, &beat_index, &ticks);

    const TimeSigFrac time_sig = score->sigmap()->timesig(Fraction::from_ticks(tick)).timesig();
    const int ticks_b = ticks_beat(time_sig.denominator());

    measure_beat.beat = beat_index + ticks / static_cast<float>(ticks_b);
    measure_beat.max_measure_index = const_cast<Score *>(score)->measures()->size() - 1;
    measure_beat.max_beat_index = time_sig.numerator() - 1;

    return measure_beat;
}

bool is_element_in_fret_box(const EngravingItem *item) {
    if (item->is_harmony()) {
        return to_harmony(item)->is_in_fret_box();
    } else if (item->is_fret_diagram()) {
        return to_fret_diagram(item)->is_in_fret_box();
    }
    return false;
}

std::vector<EngravingItem *> filter_target_elements(const Selection &sel, EngravingItem *drop_element, bool &unique) {
    bool unique_measures = false;
    bool unique_staves = false;

    switch (drop_element->type()) {
        // Brackets have a special logic for range selections.
        // For other selections add only one to each staff:
        case ElementType::BRACKET:
            if (sel.is_range()) {
                unique = true;
                return {sel.start_segment()->first_element_for_navigation(sel.staff_start())};
            }
            unique_staves = true;
            break;

        // Barlines are only added to measures in range selections:
        case ElementType::BAR_LINE: unique_measures = sel.is_range(); break;

        // For multi-measure measure repeats, avoid overlap by adding only one per staff:
        case ElementType::MEASURE_REPEAT:
            unique_staves = true;
            unique_measures = to_measure_repeat(drop_element)->num_measures() == 1;
            break;

        // Add these elements once per measure in list selections, else once total:
        case ElementType::MARKER:
        case ElementType::JUMP:
        case ElementType::SPACER:           //! HACK - these should be applied "per staff" in range selections (see issue #33486)
        case ElementType::STAFFTYPE_CHANGE: //! HACK - these should be applied "per staff" in range selections (see issue #33486)
        case ElementType::VBOX:
        case ElementType::HBOX:
        case ElementType::TBOX:
        case ElementType::FBOX:
        case ElementType::MEASURE:
            if (sel.is_range()) {
                unique = true;
                return {sel.start_segment()->first_element_for_navigation(sel.staff_start())};
            }
            unique_measures = true;
            break;

        // Add these elements once per measure, always:
        case ElementType::MEASURE_NUMBER: unique_measures = true; break;

        case ElementType::ACTION_ICON: {
            const ActionIconType action_type = to_action_icon(drop_element)->action_type();
            switch (action_type) {
                case ActionIconType::
                    STAFF_TYPE_CHANGE: //! HACK - these should be applied "per staff" in range selections (see issue #33486)
                case ActionIconType::VFRAME:
                case ActionIconType::HFRAME:
                case ActionIconType::TFRAME:
                case ActionIconType::FFRAME:
                case ActionIconType::MEASURE:
                    if (sel.is_range()) {
                        unique = true;
                        return {sel.start_segment()->first_element_for_navigation(sel.staff_start())};
                    }
                    break;
                default: break;
            }
            break;
        }
        default: break;
    }

    unique = unique_staves || unique_measures;
    if (!unique) {
        return sel.elements();
    }

    std::vector<EngravingItem *> result;
    if (unique_staves && unique_measures) {
        std::vector<MStaff *> found_m_staves;
        for (EngravingItem *e : sel.elements()) {
            if (Measure *m = e->find_measure()) {
                if (!muse::contains(found_m_staves, m->mstaves().at(e->staff_idx()))) {
                    result.emplace_back(e);
                    found_m_staves.emplace_back(m->mstaves().at(e->staff_idx()));
                }
            }
        }
    } else if (unique_staves) {
        std::vector<staff_idx_t> found_staves;
        for (EngravingItem *e : sel.elements()) {
            if (!muse::contains(found_staves, e->staff_idx())) {
                result.emplace_back(e);
                found_staves.emplace_back(e->staff_idx());
            }
        }
    } else {
        std::vector<MeasureBase *> found_measures;
        for (EngravingItem *e : sel.elements()) {
            if (MeasureBase *mb = e->find_measure_base()) {
                if (!muse::contains(found_measures, mb)) {
                    result.emplace_back(e);
                    found_measures.emplace_back(mb);
                }
            }
        }
    }
    return result;
}

Lyrics *search_next_lyrics(Segment *s, staff_idx_t staff_idx, int verse, PlacementV p) {
    Lyrics *l = nullptr;
    const Segment *original_seg = s;
    while ((s = s->next1(SegmentType::CHORD_REST))) {
        if (!segments_are_adjacent(original_seg, s)) {
            return nullptr;
        }

        track_idx_t strack = staff_idx * ::mu::engraving::voices;
        track_idx_t etrack = strack + ::mu::engraving::voices;
        // search through all tracks of current staff looking for a lyric in specified verse
        for (track_idx_t track = strack; track < etrack; ++track) {
            ChordRest *cr = to_chord_rest(s->element(track));
            if (cr) {
                // cr with lyrics found, but does it have a syllable in specified verse?
                l = cr->lyrics(verse, p);
                if (l) {
                    break;
                }
            }
        }
        if (l) {
            break;
        }
    }
    return l;
}

bool note_is_before(const Note *n1, const Note *n2) {
    const int l1 = n1->line();
    const int l2 = n2->line();
    if (l1 != l2) {
        return l1 > l2;
    }

    const int p1 = n1->pitch();
    const int p2 = n2->pitch();
    if (p1 != p2) {
        return p1 < p2;
    }

    if (n1->tie_back()) {
        if (n2->tie_back() && !n2->incoming_partial_tie()) {
            const Note *sn1 = n1->tie_back()->start_note();
            const Note *sn2 = n2->tie_back()->start_note();
            if (sn1->chord() == sn2->chord()) {
                return sn1->unison_index() < sn2->unison_index();
            }
            return sn1->chord()->is_before(sn2->chord());
        } else {
            return true; // place tied notes before
        }
    }

    return false;
}

void update_percussion_notes(Chord *c, const Drumset *drumset) {
    for (Chord *ch : c->grace_notes()) {
        update_percussion_notes(ch, drumset);
    }
    std::vector<Note *> lnotes(c->notes()); // we need a copy!
    for (Note *note : lnotes) {
        if (!drumset) {
            note->set_line(0);
        } else {
            int pitch = note->pitch();
            if (!drumset->is_valid(pitch)) {
                note->set_line(0);
                //! NOTE May be called too often
                //LOGW("unmapped drum note %d", pitch);
            } else if (!note->fixed()) {
                note->undo_change_property(Pid::HEAD_GROUP, drumset->note_head(pitch));
                note->set_line(drumset->line(pitch));
            }
        }
    }
}
} // namespace mu::engraving
