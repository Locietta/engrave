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

#include "ambitus.h"

#include "accidental.h"
#include "chord.h"
#include "factory.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "translation.h"
#include "utils.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   Ambitus
//---------------------------------------------------------

Ambitus::Ambitus(DummyParentOr<Segment> parent) : EngravingItem(ElementType::AMBITUS, parent, ElementFlag::ON_STAFF) {
    note_head_group_ = ::mu::engraving::Ambitus::noteheadgroup_default;
    note_head_type_ = ::mu::engraving::Ambitus::noteheadtype_default;
    direction_ = ::mu::engraving::Ambitus::direction_default;
    has_line_ = ::mu::engraving::Ambitus::hasline_default;
    line_width_ = ::mu::engraving::Ambitus::linewidth_default;
    top_pitch_ = ::mu::engraving::invalid_pitch;
    bottom_pitch_ = ::mu::engraving::invalid_pitch;
    top_tpc_ = Tpc::TPC_INVALID;
    bottom_tpc_ = Tpc::TPC_INVALID;

    top_accidental_ = Factory::create_accidental(this, false);
    bottom_accidental_ = Factory::create_accidental(this, false);
}

Ambitus::Ambitus(const Ambitus &a) : EngravingItem(a) {
    note_head_group_ = a.note_head_group_;
    note_head_type_ = a.note_head_type_;
    direction_ = a.direction_;
    has_line_ = a.has_line_;
    line_width_ = a.line_width_;

    top_accidental_ = a.top_accidental_->clone();
    top_accidental_->set_ownership_parent(this);

    bottom_accidental_ = a.bottom_accidental_->clone();
    bottom_accidental_->set_ownership_parent(this);

    top_pitch_ = a.top_pitch_;
    top_tpc_ = a.top_tpc_;
    bottom_pitch_ = a.bottom_pitch_;
    bottom_tpc_ = a.bottom_tpc_;
}

Ambitus::~Ambitus() {
    delete top_accidental_;
    delete bottom_accidental_;
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Ambitus::mag() const { return staff() ? staff()->staff_mag(tick()) : 1.0; }

//---------------------------------------------------------
//   initFrom
//---------------------------------------------------------

void Ambitus::init_from(Ambitus *a) {
    note_head_group_ = a->note_head_group_;
    note_head_type_ = a->note_head_type_;
    direction_ = a->direction_;
    has_line_ = a->has_line_;
    line_width_ = a->line_width_;
    top_pitch_ = a->top_pitch_;
    bottom_pitch_ = a->bottom_pitch_;
    top_tpc_ = a->top_tpc_;
    bottom_tpc_ = a->bottom_tpc_;
}

//---------------------------------------------------------
//   setTrack
//
//    when the Ambitus element is assigned a track,
//    initialize top and bottom 'notes' to top and bottom staff lines
//---------------------------------------------------------

void Ambitus::set_track(track_idx_t t) {
    EngravingItem::set_track(t);

    // if not initialized and there is a segment and a staff,
    // initialize pitches and tpc's to first and last staff line
    // (for use in palettes)
    if (!pitch_is_valid(top_pitch_) || !tpc_is_valid(top_tpc_) || !pitch_is_valid(bottom_pitch_) || !tpc_is_valid(bottom_tpc_)) {
        if (segment() && staff()) {
            Ambitus::Ranges ranges = estimate_ranges();
            top_tpc_ = ranges.top_tpc;
            bottom_tpc_ = ranges.bottom_tpc;
            top_pitch_ = ranges.top_pitch;
            bottom_pitch_ = ranges.bottom_pitch;

            top_accidental_->set_track(t);
            bottom_accidental_->set_track(t);
        }
    }
}

//---------------------------------------------------------
//   setTop/BottomPitch
//
//    setting either pitch requires to adjust the corresponding tpc
//---------------------------------------------------------

void Ambitus::set_top_pitch(int val, bool apply_logic) {
    if (!apply_logic) {
        top_pitch_ = val;
        return;
    }

    // if pitch difference is not an integer number of octaves, adjust tpc
    // (to avoid 'wild' tpc changes with octave changes)
    if ((val - top_pitch()) % ::mu::engraving::pitch_delta_octave != 0) {
        Key key = (staff() && segment()) ? staff()->key(segment()->tick()) : Key::C;
        top_tpc_ = pitch2tpc(val, key, Prefer::NEAREST);
    }
    top_pitch_ = val;
    normalize();
}

void Ambitus::set_bottom_pitch(int val, bool apply_logic) {
    if (!apply_logic) {
        bottom_pitch_ = val;
        return;
    }

    // if pitch difference is not an integer number of octaves, adjust tpc
    // (to avoid 'wild' tpc changes with octave changes)
    if ((val - bottom_pitch()) % ::mu::engraving::pitch_delta_octave != 0) {
        Key key = (staff() && segment()) ? staff()->key(segment()->tick()) : Key::C;
        bottom_tpc_ = pitch2tpc(val, key, Prefer::NEAREST);
    }
    bottom_pitch_ = val;
    normalize();
}

//---------------------------------------------------------
//   setTop/BottomTpc
//
//    setting either tpc requires to adjust the corresponding pitch
//    (but remaining in the same octave)
//---------------------------------------------------------

void Ambitus::set_top_tpc(int val, bool apply_logic) {
    top_tpc_ = val;

    if (!apply_logic) {
        return;
    }

    int octave = top_pitch() / ::mu::engraving::pitch_delta_octave;
    int new_octaved_pitch = (tpc2pitch(val) + ::mu::engraving::pitch_delta_octave) % ::mu::engraving::pitch_delta_octave;
    top_pitch_ = (octave * ::mu::engraving::pitch_delta_octave) + new_octaved_pitch;
    normalize();
}

void Ambitus::set_bottom_tpc(int val, bool apply_logic) {
    bottom_tpc_ = val;

    if (!apply_logic) {
        return;
    }

    int octave = bottom_pitch() / ::mu::engraving::pitch_delta_octave;
    int new_octaved_pitch = (tpc2pitch(val) + ::mu::engraving::pitch_delta_octave) % ::mu::engraving::pitch_delta_octave;
    bottom_pitch_ = (octave * ::mu::engraving::pitch_delta_octave) + new_octaved_pitch;
    normalize();
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Ambitus::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);
    if (top_accidental_->accidental_type() != AccidentalType::NONE) {
        func(top_accidental_);
    }

    if (bottom_accidental_->accidental_type() != AccidentalType::NONE) {
        func(bottom_accidental_);
    }
}

//---------------------------------------------------------
//   noteHead
//---------------------------------------------------------

SymId Ambitus::note_head() const {
    int hg = 1;
    NoteHeadType ht = NoteHeadType::HEAD_QUARTER;

    if (note_head_type_ != NoteHeadType::HEAD_AUTO) {
        ht = note_head_type_;
    }

    SymId t = Note::note_head(hg, note_head_group_, ht);
    if (t == SymId::NO_SYM) {
        LOGD("invalid notehead %d/%d", int(note_head_group_), int(note_head_type_));
        t = Note::note_head(0, NoteHeadGroup::HEAD_NORMAL, ht);
    }
    return t;
}

//---------------------------------------------------------
//   headWidth
//
//    returns the width of the notehead symbol
//---------------------------------------------------------

double Ambitus::head_width() const { return sym_width(note_head()); }

//---------------------------------------------------------
//   normalize
//
//    makes sure topPitch is not < bottomPitch
//---------------------------------------------------------

void Ambitus::normalize() {
    if (top_pitch_ < bottom_pitch_) {
        std::swap(top_pitch_, bottom_pitch_);
        std::swap(top_tpc_, bottom_tpc_);
    }
}

//---------------------------------------------------------
//   updateRange
//
//    scans the staff contents up to next section break to update the range pitches/tpc's
//---------------------------------------------------------

Ambitus::Ranges Ambitus::estimate_ranges() const {
    Ambitus::Ranges result;

    Segment *s = segment() ? segment()->measure()->find_segment(SegmentType::CHORD_REST, segment()->tick()) : nullptr;
    for (; s && !s->measure()->section_break(); s = s->next_cr()) {
        for (track_idx_t t = track(); t < track() + ::mu::engraving::voices; t++) {
            EngravingItem *e = s->element(t);
            if (!e || !e->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(e);
            for (Note *n : chord->notes()) {
                if (!n->play()) {
                    continue;
                }
                int pitch = n->epitch() + n->ottave_capo_fret(); // written pitch, accounting for octave offset
                if (pitch > result.top_pitch) {
                    result.top_pitch = pitch;
                    result.top_tpc = n->tpc();
                }
                if (pitch < result.bottom_pitch) {
                    result.bottom_pitch = pitch;
                    result.bottom_tpc = n->tpc();
                }
            }
        }
    }

    return result;
}

void Ambitus::remove(EngravingItem *e) {
    if (e->is_accidental()) {
        //! NOTE Do nothing (removing _topAccid or _bottomAccid)
        return;
    }

    EngravingItem::remove(e);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Ambitus::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::HEAD_GROUP: return note_head_group();
        case Pid::HEAD_TYPE: return note_head_type();
        case Pid::MIRROR_HEAD: return direction();
        case Pid::GHOST: // recycled property = _hasLine
            return has_line();
        case Pid::LINE_WIDTH: return line_width();
        case Pid::TPC1: return top_tpc();
        case Pid::FBPARENTHESIS1: // recycled property = _bottomTpc
            return bottom_tpc();
        case Pid::PITCH: return top_pitch();
        case Pid::FBPARENTHESIS2: // recycled property = _bottomPitch
            return bottom_pitch();
        case Pid::FBPARENTHESIS3: // recycled property = octave of _topPitch
            return top_octave();
        case Pid::FBPARENTHESIS4: // recycled property = octave of _bottomPitch
            return bottom_octave();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Ambitus::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::HEAD_GROUP: set_note_head_group(v.value<NoteHeadGroup>()); break;
        case Pid::HEAD_TYPE: set_note_head_type(v.value<NoteHeadType>()); break;
        case Pid::MIRROR_HEAD: set_direction(v.value<DirectionH>()); break;
        case Pid::GHOST: // recycled property = _hasLine
            set_has_line(v.to_bool());
            break;
        case Pid::LINE_WIDTH: set_line_width(v.value<Spatium>()); break;
        case Pid::TPC1: set_top_tpc(v.to_int()); break;
        case Pid::FBPARENTHESIS1: // recycled property = _bottomTpc
            set_bottom_tpc(v.to_int());
            break;
        case Pid::PITCH: set_top_pitch(v.to_int()); break;
        case Pid::FBPARENTHESIS2: // recycled property = _bottomPitch
            set_bottom_pitch(v.to_int());
            break;
        case Pid::FBPARENTHESIS3: // recycled property = octave of _topPitch
            set_top_pitch(top_pitch() % ::mu::engraving::pitch_delta_octave + (v.to_int() + 1) * ::mu::engraving::pitch_delta_octave);
            break;
        case Pid::FBPARENTHESIS4: // recycled property = octave of _bottomPitch
            set_bottom_pitch(bottom_pitch() % ::mu::engraving::pitch_delta_octave + (v.to_int() + 1) * ::mu::engraving::pitch_delta_octave);
            break;
        case Pid::COLOR:
        case Pid::VISIBLE:
            top_accidental_->set_property(property_id, v);
            bottom_accidental_->set_property(property_id, v);
            EngravingItem::set_property(property_id, v);
            break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Ambitus::property_default(Pid id) const {
    switch (id) {
        case Pid::HEAD_GROUP: return ::mu::engraving::Ambitus::noteheadgroup_default;
        case Pid::HEAD_TYPE: return ::mu::engraving::Ambitus::noteheadtype_default;
        case Pid::MIRROR_HEAD: return ::mu::engraving::Ambitus::direction_default;
        case Pid::GHOST: return ::mu::engraving::Ambitus::hasline_default;
        case Pid::LINE_WIDTH: return ::mu::engraving::Ambitus::linewidth_default;
        case Pid::TPC1: return estimate_ranges().top_tpc;
        case Pid::FBPARENTHESIS1: return estimate_ranges().bottom_tpc;
        case Pid::PITCH: return estimate_ranges().top_pitch;
        case Pid::FBPARENTHESIS2: return estimate_ranges().bottom_pitch;
        case Pid::FBPARENTHESIS3: return int(estimate_ranges().top_pitch / ::mu::engraving::pitch_delta_octave) - 1;
        case Pid::FBPARENTHESIS4: return int(estimate_ranges().bottom_pitch / ::mu::engraving::pitch_delta_octave) - 1;
        default: return EngravingItem::property_default(id);
    }
    //return QVariant();
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *Ambitus::next_segment_element() { return segment()->first_in_next_segments(staff_idx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *Ambitus::prev_segment_element() { return segment()->last_in_prev_segments(staff_idx()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

AccidentalType Ambitus::accidental_type(int tpc, Key key) {
    AccidentalType accid_type = AccidentalType::NONE;
    // if (13 <= (tpc - key) <= 19) there is no accidental)
    if (tpc - int(key) >= 13 && tpc - int(key) <= 19) {
        accid_type = AccidentalType::NONE;
    } else {
        AccidentalVal accid_val = tpc2alter(tpc);
        accid_type = Accidental::value2subtype(accid_val);
        if (accid_type == AccidentalType::NONE) {
            accid_type = AccidentalType::NATURAL;
        }
    }

    return accid_type;
}

//! TODO Seems to need to be moved to utilities
int Ambitus::staff_line(int tpc, int pitch, ClefType clf) {
    int line = abs_step(tpc, pitch);
    line = rel_step(line, clf);
    return line;
}
