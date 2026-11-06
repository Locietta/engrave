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

#include "trill.h"

#include <cmath>

#include "../editing/addremoveelement.h"
#include "accidental.h"
#include "chord.h"
#include "clef.h"
#include "factory.h"
#include "iengravingfont.h"
#include "measure.h"
#include "ornament.h"
#include "score.h"
#include "system.h"
#include "types/typesconv.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   trillStyle
//---------------------------------------------------------

static const ElementStyle trill_style{
    {Sid::TRILL_PLACEMENT, Pid::PLACEMENT},
};

TrillSegment::TrillSegment(Trill *sp) : LineSegment(ElementType::TRILL_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void TrillSegment::remove(EngravingItem *e) {
    if (trill()->accidental() == e) {
        // accidental is part of trill
        trill()->set_accidental(nullptr);
        e->removed();
    }
}

//---------------------------------------------------------
//   symbolLine
//---------------------------------------------------------

void TrillSegment::symbol_line(SymId start, SymId fill) {
    double x1 = 0.0;
    double x2 = pos2().x();
    double w = x2 - x1;
    double mag = mag_s();
    IEngravingFontPtr f = score()->engraving_font();

    symbols_.clear();
    symbols_.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    int n = lrint((w - w1) / w2);
    for (int i = 0; i < n; ++i) {
        symbols_.push_back(fill);
    }
    RectF r(f->bbox(symbols_, mag));
    setbbox(r);
}

void TrillSegment::symbol_line(SymId start, SymId fill, SymId end) {
    double x1 = 0.0;
    double x2 = pos2().x();
    double w = x2 - x1;
    double mag = mag_s();
    IEngravingFontPtr f = score()->engraving_font();

    symbols_.clear();
    symbols_.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    double w3 = f->advance(end, mag);
    int n = lrint((w - w1 - w3) / w2);
    for (int i = 0; i < n; ++i) {
        symbols_.push_back(fill);
    }
    symbols_.push_back(end);
    RectF r(f->bbox(symbols_, mag));
    setbbox(r);
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void TrillSegment::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);
    if (is_single_type() || is_begin_type()) {
        Accidental *a = trill()->accidental();
        if (a) {
            func(a);
        }
        Chord *cue_note_chord = trill()->cue_note_chord();
        if (cue_note_chord) {
            cue_note_chord->scan_elements(func);
        }
    }
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *TrillSegment::property_delegate(Pid pid) const {
    if (pid == Pid::TRILL_TYPE || pid == Pid::ORNAMENT_STYLE || pid == Pid::PLACEMENT) {
        return spanner();
    }
    return LineSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   Trill
//---------------------------------------------------------

Trill::Trill(DummyParentOr<EngravingItem> parent) : SLine(ElementType::TRILL, parent) {
    trill_type_ = TrillType::TRILL_LINE;
    ornament_ = nullptr;
    accidental_ = nullptr;
    cue_note_chord_ = nullptr;
    ornament_style_ = OrnamentStyle::DEFAULT;
    init_element_style(&::trill_style);
}

Trill::Trill(const Trill &t) : SLine(t) {
    trill_type_ = t.trill_type_;
    ornament_ = t.ornament_ ? t.ornament_->clone() : nullptr;
    ornament_style_ = t.ornament_style_;
    init_element_style(&::trill_style);
}

EngravingItem *Trill::linked_clone() {
    Trill *linked_trill = clone();
    Ornament *linked_ornament = to_ornament(ornament_->linked_clone());
    linked_trill->set_ornament(linked_ornament);
    linked_trill->set_autoplace(true);
    score()->undo(new Link(linked_trill, this));
    return linked_trill;
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Trill::remove(EngravingItem *e) {
    if (e == accidental_) {
        accidental_ = nullptr;
        e->removed();
    }
}

void Trill::set_track(track_idx_t n) {
    EngravingItem::set_track(n);

    for (SpannerSegment *ss : spanner_segments()) {
        ss->set_track(n);
    }

    if (ornament_) {
        ornament_->set_track(n);
    }
}

void Trill::set_score(Score *s) {
    Spanner::set_score(s);
    if (ornament_) {
        ornament_->set_score(s);
    }
}

void Trill::compute_start_element() {
    Spanner::compute_start_element();
    if (start_element() && start_element()->is_chord() && ornament_) {
        ornament_->set_ownership_parent(start_element());

        Chord *cue_chord = ornament_->cue_note_chord();
        if (cue_chord) {
            cue_chord->set_ownership_parent(to_chord(start_element())->segment());
        }
    }
}

PointF Trill::trill_line_pos(const SLine *line, Grip grip, System **system) {
    if (!line) {
        return PointF();
    }

    bool start = grip == Grip::START;
    bool mm_rest = line->style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS);
    double grace_offset = 0.0;
    double clef_offset = 0.0;

    Segment *segment = start ? line->start_segment() : line->end_segment();
    if (!segment) {
        return PointF();
    }

    if (start) {
        *system = segment->measure()->system();
        double x = segment->x() + segment->measure()->x();
        return PointF(x, 0.0);
    }

    Segment *clef_seg = segment->is_clef_type() ? segment : nullptr;
    Fraction cur_tick = segment->tick();
    while (true) {
        Segment *prev_seg = mm_rest ? segment->prev1_mm() : segment->prev1();
        if (prev_seg && prev_seg->tick() == cur_tick) {
            clef_seg = prev_seg->is_clef_type() ? prev_seg : clef_seg;
            segment = prev_seg;
        } else {
            break;
        }
    }

    // Stop line before clefs
    if (clef_seg) {
        EngravingItem *clef_item = clef_seg->element(line->track2());
        if (clef_item && clef_item->is_clef()) {
            Clef *clef = to_clef(clef_item);
            SymId clef_sym = ClefInfo::sym_id(clef->clef_type());
            Shape clef_shape = line->sym_shape_with_cutouts(clef_sym).translated(clef->pos());
            clef_offset = segment->page_x() - clef->page_x() + clef_shape.left_most_edge_at_top();
        }
    }

    // Stop line before grace notes
    if (const EngravingItem *pre_appended_item = segment->pre_appended_item(line->track2());
        pre_appended_item && pre_appended_item->is_grace_notes_group()) {
        // get x position of leftmost grace note
        const Chord *left_most_grace_chord = nullptr;
        const GraceNotesGroup *grace_group = to_grace_notes_group(pre_appended_item);
        for (const Chord *grace_chord : *grace_group) {
            left_most_grace_chord =
                left_most_grace_chord && left_most_grace_chord->x() < grace_chord->x() ? left_most_grace_chord : grace_chord;
        }
        if (left_most_grace_chord) {
            grace_offset = segment->page_x() - left_most_grace_chord->page_x();
        }
    }

    double offset = std::max(grace_offset, clef_offset);

    *system = segment->measure()->system();
    double x = segment->x() + segment->measure()->x() - line->spatium() - offset;
    return PointF(x, 0.0);
}

PointF Trill::line_pos(Grip grip, System **system) const { return trill_line_pos(this, grip, system); }

void Trill::set_trill_type(TrillType tt) {
    trill_type_ = tt;
    if (!ornament_) {
        // ornament parent will be explicitely set at layout stage
        ornament_ = Factory::create_ornament(score()->dummy());
    }
    ornament_->set_track(track());
    ornament_->set_sym_id(Ornament::from_trill_type(tt));
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle trill_segment_style{
    {Sid::TRILL_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

LineSegment *Trill::create_line_segment() {
    TrillSegment *seg = new TrillSegment(this);
    seg->set_track(track());
    seg->set_color(line_color());
    seg->init_element_style(&::trill_segment_style);
    return seg;
}

//---------------------------------------------------------
//   trillTypeName
//---------------------------------------------------------

String Trill::trill_type_user_name() const { return TConv::translated_user_name(trill_type()); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Trill::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::TRILL_TYPE: return int(trill_type());
        case Pid::ORNAMENT_STYLE: return ornament_style();
        default: break;
    }
    return SLine::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Trill::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::TRILL_TYPE: set_trill_type(TrillType(val.to_int())); break;
        case Pid::ORNAMENT_STYLE: set_ornament_style(val.value<OrnamentStyle>()); break;
        case Pid::COLOR:
            set_color(val.value<Color>());
            set_line_color(val.value<Color>());
            break;
        default: return SLine::set_property(property_id, val);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Trill::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::TRILL_TYPE: return static_cast<int>(TrillType::TRILL_LINE);
        case Pid::ORNAMENT_STYLE: return OrnamentStyle::DEFAULT;
        case Pid::PLACEMENT: return style().style_v(Sid::TRILL_PLACEMENT);

        default: return SLine::property_default(property_id);
    }
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Trill::subtype_user_name() const { return TConv::user_name(trill_type()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

Sid Trill::default_pos_sid() const { return place_above() ? Sid::TRILL_POS_ABOVE : Sid::TRILL_POS_BELOW; }

void Trill::do_compute_end_element() {
    set_end_element(score()->find_chord_rest_ending_before_tick_in_staff_and_voice(tick2(), track2staff(track2()), voice()));
}
