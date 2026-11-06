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
#include "slur.h"

#include "../editing/editspanner.h"
#include "../editing/navigation.h"
#include "../editing/transaction/transaction.h"
#include "../rendering/iscorerenderer.h"
#include "arpeggio.h"
#include "beam.h"
#include "chord.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "note.h"
#include "part.h"
#include "score.h"
#include "staff.h"
#include "stafftype.h"
#include "stem.h"
#include "system.h"
#include "tremolotwochord.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::draw;

namespace mu::engraving {
SlurSegment::SlurSegment(Slur *sp, ElementType type) : SlurTieSegment(type, sp) {}

SlurSegment::SlurSegment(const SlurSegment &ss) : SlurTieSegment(ss) {}

//---------------------------------------------------------
//   searchCR
//---------------------------------------------------------

static ChordRest *search_cr(Segment *segment, track_idx_t start_track, track_idx_t end_track) {
    for (Segment *s = segment; s; s = s->next(SegmentType::CHORD_REST)) { // restrict search to measure
        if (start_track > end_track) {
            for (int t = static_cast<int>(start_track) - 1; t >= static_cast<int>(end_track); --t) {
                if (s->element(t)) {
                    return to_chord_rest(s->element(t));
                }
            }
        } else {
            for (track_idx_t t = start_track; t < end_track; ++t) {
                if (s->element(t)) {
                    return to_chord_rest(s->element(t));
                }
            }
        }
    }
    return 0;
}

//---------------------------------------------------------
//   isEdited
//---------------------------------------------------------

bool SlurSegment::is_edited() const {
    for (int i = 0; i < int(Grip::GRIPS); ++i) {
        if (!ups_[i].off.is_null()) {
            return true;
        }
    }
    return false;
}

bool SlurSegment::is_end_points_edited() const { return !(ups_[int(Grip::START)].off.is_null() && ups_[int(Grip::END)].off.is_null()); }

double SlurSegment::end_width() const { return style().style_absolute(Sid::SLUR_END_WIDTH); }

double SlurSegment::mid_width() const { return style().style_absolute(Sid::SLUR_MID_WIDTH); }

double SlurSegment::dotted_width() const { return style().style_absolute(Sid::SLUR_DOTTED_WIDTH); }

Color SlurSegment::cur_color(const rendering::PaintOptions &opt) const {
    return EngravingItem::cur_color(get_property(Pid::VISIBLE).to_bool(), get_property(Pid::COLOR).value<Color>(), opt);
}

Slur::Slur(const Slur &s) : SlurTie(s) {
    connected_element_value = s.connected_element_value;
    partial_spanner_direction_value = s.partial_spanner_direction_value;
}

//---------------------------------------------------------
//   Slur
//---------------------------------------------------------

Slur::Slur(DummyParentOr<EngravingItem> parent, ElementType type) : SlurTie(type, parent) {}

double Slur::scaling_factor() const {
    Chord *start_c = start_element() && start_element()->is_chord() ? to_chord(start_element()) : nullptr;
    Chord *end_c = end_element() && end_element()->is_chord() ? to_chord(end_element()) : nullptr;

    if (!start_c || !end_c) {
        return 1.0;
    }

    if ((start_c->is_grace_before() && start_c->parent() == end_c) || (end_c->is_grace_after() && end_c->parent() == start_c)) {
        return style().style_d(Sid::GRACE_NOTE_MAG);
    }

    if (start_c->is_grace()) {
        start_c = to_chord(start_c->parent());
    }
    if (end_c->is_grace()) {
        end_c = to_chord(end_c->parent());
    }

    return 0.5 * (start_c->intrinsic_mag() + end_c->intrinsic_mag());
}

void Slur::undo_set_incoming(bool incoming) {
    if (incoming == is_incoming()) {
        return;
    }

    undo_change_property(Pid::PARTIAL_SPANNER_DIRECTION, calc_incoming_direction(incoming), PropertyFlags::UNSTYLED);
}

void Slur::undo_set_outgoing(bool outgoing) {
    if (outgoing == is_outgoing()) {
        return;
    }

    undo_change_property(Pid::PARTIAL_SPANNER_DIRECTION, calc_outgoing_direction(outgoing), PropertyFlags::UNSTYLED);
}

void Slur::set_incoming(bool incoming) {
    if (incoming == is_incoming()) {
        return;
    }

    partial_spanner_direction_value = calc_incoming_direction(incoming);
}

void Slur::set_outgoing(bool outgoing) {
    if (outgoing == is_outgoing()) {
        return;
    }

    partial_spanner_direction_value = calc_outgoing_direction(outgoing);
}

PartialSpannerDirection Slur::calc_incoming_direction(bool incoming) {
    PartialSpannerDirection dir = PartialSpannerDirection::INCOMING;
    if (incoming) {
        SlurSegment *first_seg = nsegments() > 0 ? front_segment() : nullptr;
        if (first_seg) {
            first_seg->set_slur_offset(Grip::START, PointF(0, 0));
        }
        dir = partial_spanner_direction_value == PartialSpannerDirection::OUTGOING ? PartialSpannerDirection::BOTH :
                                                                                     PartialSpannerDirection::INCOMING;
    } else {
        dir = partial_spanner_direction_value == PartialSpannerDirection::BOTH ? PartialSpannerDirection::OUTGOING :
                                                                                 PartialSpannerDirection::NONE;
    }
    return dir;
}

PartialSpannerDirection Slur::calc_outgoing_direction(bool outgoing) {
    PartialSpannerDirection dir = PartialSpannerDirection::OUTGOING;
    if (outgoing) {
        SlurSegment *last_seg = nsegments() > 0 ? back_segment() : nullptr;
        if (last_seg) {
            last_seg->set_slur_offset(Grip::END, PointF(0, 0));
        }
        dir = partial_spanner_direction_value == PartialSpannerDirection::INCOMING ? PartialSpannerDirection::BOTH :
                                                                                     PartialSpannerDirection::OUTGOING;
    } else {
        dir = partial_spanner_direction_value == PartialSpannerDirection::BOTH ? PartialSpannerDirection::INCOMING :
                                                                                 PartialSpannerDirection::NONE;
    }
    return dir;
}

bool Slur::is_incoming() const {
    return partial_spanner_direction_value == PartialSpannerDirection::BOTH ||
           partial_spanner_direction_value == PartialSpannerDirection::INCOMING;
}

bool Slur::is_outgoing() const {
    return partial_spanner_direction_value == PartialSpannerDirection::BOTH ||
           partial_spanner_direction_value == PartialSpannerDirection::OUTGOING;
}

void Slur::undo_change_start_end_elements(ChordRest *scr, ChordRest *ecr) {
    for (EngravingObject *lsp : link_list()) {
        Spanner *sp = to_spanner(lsp);
        if (sp == this) {
            score()->undo(new ChangeSpannerElements(this, scr, ecr));
        } else {
            EngravingItem *se = 0;
            EngravingItem *ee = 0;
            if (scr) {
                std::list<EngravingObject *> sel = scr->link_list();
                for (EngravingObject *lcr : sel) {
                    EngravingItem *le = to_engraving_item(lcr);
                    if (le->score() == sp->score() && le->track() == sp->track()) {
                        se = le;
                        break;
                    }
                }
            }
            if (ecr) {
                std::list<EngravingObject *> sel = ecr->link_list();
                for (EngravingObject *lcr : sel) {
                    EngravingItem *le = to_engraving_item(lcr);
                    if (le->score() == sp->score() && le->track() == sp->track2()) {
                        ee = le;
                        break;
                    }
                }
            }
            if (se && ee) {
                score()->undo(new ChangeStartEndSpanner(sp, se, ee));
                renderer()->layout_item(sp);
            }
        }
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Slur::set_track(track_idx_t n) {
    EngravingItem::set_track(n);
    for (SpannerSegment *ss : spanner_segments()) {
        ss->set_track(n);
    }
}

bool Slur::is_cross_staff() {
    return start_cr() && end_cr() &&
           (start_cr()->staff_move() != 0 || end_cr()->staff_move() != 0 || start_cr()->v_staff_idx() != end_cr()->v_staff_idx());
}

bool Slur::has_cross_beams() {
    return (start_cr() && start_cr()->beam() && start_cr()->beam()->cross()) || (end_cr() && end_cr()->beam() && end_cr()->beam()->cross());
}

PropertyValue Slur::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PARTIAL_SPANNER_DIRECTION: return partial_spanner_direction();
        default: return SlurTie::get_property(property_id);
    }
}

PropertyValue Slur::property_default(Pid id) const {
    switch (id) {
        case Pid::PARTIAL_SPANNER_DIRECTION: return PartialSpannerDirection::NONE;
        default: return SlurTie::property_default(id);
    }
}

bool Slur::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::PARTIAL_SPANNER_DIRECTION: set_partial_spanner_direction(v.value<PartialSpannerDirection>()); break;
        default: return SlurTie::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}
} // namespace mu::engraving
