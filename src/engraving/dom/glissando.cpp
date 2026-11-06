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

/* TO DO:
- XML export

NICE-TO-HAVE TODO:
- draggable handles of glissando segments
- re-attachable glissando extrema (with [Shift]+arrows, use SlurSegment::edit()
      and SlurSegment::changeAnchor() in slur.cpp as models)
*/

#include "glissando.h"

#include <cmath>
#include <algorithm>

#include "types/typesconv.h"

#include "chord.h"
#include "harppedaldiagram.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle glissando_element_style{{Sid::GLISSANDO_FONT_FACE, Pid::FONT_FACE},
                                                  {Sid::GLISSANDO_FONT_SIZE, Pid::FONT_SIZE},
                                                  {Sid::GLISSANDO_FONT_STYLE, Pid::FONT_STYLE},
                                                  {Sid::GLISSANDO_LINE_WIDTH, Pid::LINE_WIDTH},
                                                  {Sid::GLISSANDO_SHOW_TEXT, Pid::GLISS_SHOW_TEXT},
                                                  {Sid::GLISSANDO_TEXT, Pid::GLISS_TEXT},
                                                  {Sid::GLISSANDO_STYLE, Pid::GLISS_STYLE},
                                                  {Sid::GLISSANDO_LINE_STYLE, Pid::LINE_STYLE},
                                                  {Sid::GLISSANDO_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
                                                  {Sid::GLISSANDO_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
                                                  {Sid::GLISSANDO_TYPE, Pid::GLISS_TYPE}};

//=========================================================
//   GlissandoSegment
//=========================================================

GlissandoSegment::GlissandoSegment(Glissando *sp) : LineSegment(ElementType::GLISSANDO_SEGMENT, sp, ElementFlag::MOVABLE) {}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *GlissandoSegment::property_delegate(Pid pid) const {
    switch (pid) {
        case Pid::GLISS_TYPE:
        case Pid::GLISS_TEXT:
        case Pid::GLISS_SHOW_TEXT:
        case Pid::GLISS_STYLE:
        case Pid::GLISS_SHIFT:
        case Pid::GLISS_EASEIN:
        case Pid::GLISS_EASEOUT:
        case Pid::FONT_FACE:
        case Pid::FONT_SIZE:
        case Pid::FONT_STYLE:
        case Pid::LINE_WIDTH: return glissando();
        default: return LineSegment::property_delegate(pid);
    }
}

//=========================================================
//   Glissando
//=========================================================

Glissando::Glissando(DummyParentOr<EngravingItem> parent) : SLine(ElementType::GLISSANDO, parent, ElementFlag::MOVABLE) {
    init_element_style(&::mu::engraving::glissando_element_style);

    static const std::array<Pid, 4> properties_to_initialise{Pid::GLISS_SHIFT, Pid::GLISS_EASEIN, Pid::GLISS_EASEOUT, Pid::DIAGONAL};

    for (const Pid &pid : properties_to_initialise) {
        reset_property(pid);
    }
}

Glissando::Glissando(const Glissando &g) : SLine(g) {
    text_value = g.text_value;
    font_face_value = g.font_face_value;
    font_size_value = g.font_size_value;
    glissando_type_value = g.glissando_type_value;
    glissando_style_value = g.glissando_style_value;
    glissando_shift_value = g.glissando_shift_value;
    ease_in_value = g.ease_in_value;
    ease_out_value = g.ease_out_value;
    show_text_value = g.show_text_value;
    font_style_value = g.font_style_value;
    is_harp_gliss_ = g.is_harp_gliss_;
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *Glissando::create_line_segment() {
    GlissandoSegment *seg = new GlissandoSegment(this);
    seg->set_track(track());
    seg->set_color(color());
    return seg;
}

Sid Glissando::get_property_style(Pid id) const {
    if (id == Pid::GLISS_STYLE) {
        return is_harp_gliss().value_or(false) ? Sid::GLISSANDO_STYLE_HARP : Sid::GLISSANDO_STYLE;
    }

    return SLine::get_property_style(id);
}

bool Glissando::pitch_steps(const Spanner *spanner, std::vector<int> &pitch_offsets) {
    if (!spanner->end_element()->is_note()) {
        return false;
    }
    const Glissando *glissando = to_glissando(spanner);
    if (!glissando->play_spanner()) {
        return false;
    }
    GlissandoStyle glissando_style = glissando->glissando_style();
    if (glissando_style == GlissandoStyle::PORTAMENTO) {
        return false;
    }
    // only consider glissando connected to NOTE.
    const Note *note_start = to_note(spanner->start_element());
    const Note *note_end = to_note(spanner->end_element());
    int pitch_start = note_start->ppitch();
    int pitch_end = note_end->ppitch();
    if (pitch_end == pitch_start) {
        return false;
    }
    int direction = pitch_end > pitch_start ? 1 : -1;
    pitch_offsets.clear();
    if (glissando_style == GlissandoStyle::DIATONIC) {
        // Obey harp pedal diagrams if on a harp staff
        if (glissando->is_harp_gliss().value_or(false)) {
            HarpPedalDiagram *hd = spanner->origin_part()->current_harp_diagram(spanner->tick());
            std::set<int> playable_tpcs = hd ? hd->playable_tpcs() : std::set<int>({14, 16, 18, 13, 15, 17, 19});
            std::vector<int> playable_pitches;
            playable_pitches.reserve(playable_tpcs.size());
            for (int t : playable_tpcs) {
                playable_pitches.push_back(tpc2pitch(t) % ::mu::engraving::pitch_delta_octave);
            }

            // Push starting note, then check for enharmonic on the next string.  If there is an enharmonic, 0 will be pushed back twice
            pitch_offsets.push_back(0);
            int en = note_start->tpc() + ::mu::engraving::tpc_delta_enharmonic * -direction;
            // Harp pedalling will only have 1 flat or sharp
            if (en >= TPC_F_B && en <= TPC_B_S && playable_tpcs.find(en) != playable_tpcs.end()) {
                pitch_offsets.push_back(0);
            }

            for (int p = pitch_start + direction; p != pitch_end; p += direction) {
                // Count times pitch occurs in harp pedalling - this accounts for enharmonics
                int pitch_occurrences =
                    std::count(playable_pitches.begin(), playable_pitches.end(), p % ::mu::engraving::pitch_delta_octave);
                if (pitch_occurrences > 0) {
                    pitch_offsets.insert(pitch_offsets.end(), pitch_occurrences, p - pitch_start);
                }
            }

            // Check for enharmonic at end, in correct direction
            en = note_end->tpc() + ::mu::engraving::tpc_delta_enharmonic * direction;
            if (en >= TPC_F_B && en <= TPC_B_S && playable_tpcs.find(en) != playable_tpcs.end()) {
                pitch_offsets.push_back(pitch_end - pitch_start);
            }

            return pitch_offsets.size() > 0;
        }

        // Regular diatonic mode
        int line_start = note_start->line();
        // scale obeying accidentals
        for (int line = line_start, pitch = pitch_start; (direction == 1) ? (pitch < pitch_end) : (pitch > pitch_end); line -= direction) {
            int half_steps = chromatic_pitch_steps(note_start, note_end, line_start - line);
            pitch = pitch_start + half_steps;
            if ((direction == 1) ? (pitch < pitch_end) : (pitch > pitch_end)) {
                pitch_offsets.push_back(half_steps);
            }
        }
        return pitch_offsets.size() > 0;
    }
    if (glissando_style == GlissandoStyle::CHROMATIC) {
        for (int pitch = pitch_start; pitch != pitch_end; pitch += direction) {
            pitch_offsets.push_back(pitch - pitch_start);
        }
        return true;
    }
    static const std::vector<bool> white_notes = {true, false, true, false, true, true, false, true, false, true, false, true};
    int cnote = 60; // pitch of middle C
    bool note_pick = glissando_style == GlissandoStyle::WHITE_KEYS;
    for (int pitch = pitch_start; pitch != pitch_end; pitch += direction) {
        int idx = ((pitch - cnote) + 1200) % 12;
        if (white_notes[idx] == note_pick) {
            pitch_offsets.push_back(pitch - pitch_start);
        }
    }
    return true;
}

//---------------------------------------------------------
//   STATIC FUNCTIONS: guessInitialNote
//
//    Used while reading old scores (either 1.x or transitional 2.0) to determine (guess!)
//    the glissando initial note from its final chord. Returns the top note of previous chord
//    of the same instrument, preferring the chord in the same track as chord, if it exists.
//
//    CANNOT be called if the final chord and/or its segment do not exist yet in the score
//
//    Parameter:  chord: the chord this glissando ends into
//    Returns:    the top note in a suitable previous chord or nullptr if none found.
//---------------------------------------------------------

Note *Glissando::guess_initial_note(Chord *chord) {
    switch (chord->note_type()) {
            //            case NoteType::INVALID:
            //                  return 0;
        // for grace notes before, previous chord is previous chord of parent chord
        case NoteType::ACCIACCATURA:
        case NoteType::APPOGGIATURA:
        case NoteType::GRACE4:
        case NoteType::GRACE16:
        case NoteType::GRACE32:
            // move unto parent chord and proceed to standard case
            if (chord->ownership_parent() && chord->ownership_parent()->is_chord()) {
                chord = to_chord(chord->ownership_parent());
            } else {
                return 0;
            }
            break;
        // for grace notes after, return top note of parent chord
        case NoteType::GRACE8_AFTER:
        case NoteType::GRACE16_AFTER:
        case NoteType::GRACE32_AFTER:
            if (chord->ownership_parent() && chord->ownership_parent()->is_chord()) {
                return to_chord(chord->ownership_parent())->up_note();
            } else { // no parent or parent is not a chord?
                return nullptr;
            }
        case NoteType::NORMAL: {
            // if chord has grace notes before, the last one is the previous note
            std::vector<Chord *> graces = chord->grace_notes_before();
            if (graces.size() > 0) {
                return graces.back()->up_note();
            }
        } break; // else process to standard case
        default: break;
    }

    // standard case (NORMAL or grace before chord)

    // if parent not a segment, can't locate a target note
    if (!chord->ownership_parent()->is_segment()) {
        return 0;
    }

    track_idx_t chord_track = chord->track();
    Segment *segm = chord->segment();
    Part *part = chord->part();
    if (segm != nullptr) {
        segm = segm->prev1();
    }
    while (segm) {
        // if previous segment is a ChordRest segment
        if (segm->segment_type() == SegmentType::CHORD_REST) {
            Chord *target = nullptr;
            // look for a Chord in the same track
            if (segm->element(chord_track) && segm->element(chord_track)->is_chord()) {
                target = to_chord(segm->element(chord_track));
            } else { // if no same track, look for other chords in the same instrument
                for (EngravingItem *curr_chord : segm->elist()) {
                    if (curr_chord && curr_chord->is_chord() && to_chord(curr_chord)->part() == part) {
                        target = to_chord(curr_chord);
                        break;
                    }
                }
            }
            // if we found a target previous chord
            if (target) {
                // if chord has grace notes after, the last one is the previous note
                std::vector<Chord *> graces = target->grace_notes_after();
                if (graces.size() > 0) {
                    return graces.back()->up_note();
                }
                return target->up_note(); // if no grace after, return top note
            }
        }
        segm = segm->prev1();
    }
    LOGD("no first note for glissando found");
    return 0;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Glissando::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::GLISS_TYPE: return glissando_type();
        case Pid::GLISS_TEXT: return text();
        case Pid::GLISS_SHOW_TEXT: return show_text();
        case Pid::GLISS_STYLE: return glissando_style();
        case Pid::GLISS_SHIFT: return glissando_shift();
        case Pid::GLISS_EASEIN: return ease_in();
        case Pid::GLISS_EASEOUT: return ease_out();
        case Pid::FONT_FACE: return font_face_value;
        case Pid::FONT_SIZE: return font_size_value;
        case Pid::FONT_STYLE: return int(font_style_value);
        default: break;
    }
    return SLine::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Glissando::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::GLISS_TYPE: set_glissando_type(v.value<GlissandoType>()); break;
        case Pid::GLISS_TEXT: set_text(v.value<String>()); break;
        case Pid::GLISS_SHOW_TEXT: set_show_text(v.to_bool()); break;
        case Pid::GLISS_STYLE: {
            // Make sure harp glisses can only be diatonic and chromatic
            GlissandoStyle gliss_style = v.value<GlissandoStyle>();
            if (is_harp_gliss().value_or(false) && (gliss_style != GlissandoStyle::DIATONIC && gliss_style != GlissandoStyle::CHROMATIC)) {
                gliss_style = GlissandoStyle::DIATONIC;
            }
            set_glissando_style(gliss_style);
            break;
        }
        case Pid::GLISS_SHIFT: set_glissando_shift(v.to_bool()); break;
        case Pid::GLISS_EASEIN: set_ease_in(v.to_int()); break;
        case Pid::GLISS_EASEOUT: set_ease_out(v.to_int()); break;
        case Pid::FONT_FACE: set_font_face(v.value<String>()); break;
        case Pid::FONT_SIZE: set_font_size(v.to_real()); break;
        case Pid::FONT_STYLE: set_font_style(FontStyle(v.to_int())); break;
        default:
            if (!SLine::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Glissando::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::GLISS_TYPE: return style().style_v(Sid::GLISSANDO_TYPE);
        case Pid::GLISS_SHOW_TEXT: return true;
        case Pid::GLISS_STYLE: return style().style_v(get_property_style(property_id));
        case Pid::GLISS_SHIFT: return false;
        case Pid::GLISS_EASEIN:
        case Pid::GLISS_EASEOUT: return 0;
        case Pid::GLISS_TEXT: return style().style_v(Sid::GLISSANDO_TEXT);
        case Pid::DIAGONAL: return true;
        default: break;
    }
    return SLine::property_default(property_id);
}

TranslatableString Glissando::subtype_user_name() const { return TConv::user_name(glissando_type()); }
} // namespace mu::engraving
