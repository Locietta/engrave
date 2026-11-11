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

#include "rest.h"

#include <cmath>
#include <set>

#include "../editing/addremoveelement.h"
#include "../editing/editchord.h"
#include "../editing/editmeasurerepeat.h"
#include "../editing/noteinput.h"
#include "../editing/transaction/transaction.h"
#include "articulation.h"
#include "chord.h"
#include "containers.h"
#include "deadslapped.h"
#include "factory.h"
#include "iengravingconfiguration.h"
#include "log.h"
#include "measure.h"
#include "measurerepeat.h"
#include "note.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stafftype.h"
#include "translation.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//    Rest
//--------------------------------------------------------

Rest::Rest(DummyParentOr<Segment> parent) : Rest(ElementType::REST, parent) {}

Rest::Rest(const ElementType &type, DummyParentOr<Segment> parent) : ChordRest(type, parent) { beam_mode_ = BeamMode::NONE; }

Rest::Rest(DummyParentOr<Segment> parent, const TDuration &d) : Rest(ElementType::REST, parent, d) {}

Rest::Rest(const ElementType &type, DummyParentOr<Segment> parent, const TDuration &d) : ChordRest(type, parent) {
    beam_mode_ = BeamMode::NONE;
    set_duration_type(d);
    if (d.fraction().is_valid()) {
        set_ticks(d.fraction());
    }
}

Rest::Rest(const Rest &r, bool link) : ChordRest(r, link) {
    if (link) {
        score()->undo(new Link(this, const_cast<Rest *>(&r)));
        set_autoplace(true);
    }
    gap_ = r.gap_;
    dotline_ = r.dotline_;
    for (NoteDot *dot : r.dots_) {
        add(Factory::copy_note_dot(*dot));
    }

    if (r.dead_slapped_) {
        DeadSlapped *ndc = Factory::copy_dead_slapped(*r.dead_slapped_);
        add(ndc);
        if (link) {
            score()->undo(new Link(ndc, r.dead_slapped_));
        }
    }
}

void Rest::hack_to_rest_type() { hack_set_type(ElementType::REST); }

//---------------------------------------------------------
//   setOffset, overridden from EngravingItem
//    (- raster vertical position in spatium units) -> no
//    - half rests and whole rests outside the staff are
//      replaced by special symbols with ledger lines
//---------------------------------------------------------

void Rest::set_offset(const PointF &o) {
    double spatium_value = spatium();
    int line = lrint(o.y() / spatium_value);

    //! NOTE We need to find out why this is being done here.
    //! We rewrite sym in the layout (we get from the Rest::getSymbol method )

    LayoutData *ldata = mutldata();

    if (ldata->sym == SymId::REST_WHOLE && (line <= -2 || line >= 3)) {
        ldata->sym = SymId::REST_WHOLE_LEGER_LINE;
    } else if (ldata->sym == SymId::REST_WHOLE_LEGER_LINE && (line > -2 && line < 4)) {
        ldata->sym = SymId::REST_WHOLE;
    } else if (ldata->sym == SymId::REST_HALF && (line <= -3 || line >= 3)) {
        ldata->sym = SymId::REST_HALF_LEGER_LINE;
    } else if (ldata->sym == SymId::REST_HALF_LEGER_LINE && (line > -3 && line < 3)) {
        ldata->sym = SymId::REST_HALF;
    }

    EngravingItem::set_offset(o);
}

//---------------------------------------------------------
//   getSymbol
//---------------------------------------------------------

SymId Rest::get_symbol(DurationType type, int line, int lines) const {
    switch (type) {
        case DurationType::V_LONG: return SymId::REST_LONGA;
        case DurationType::V_BREVE: return (line < 0 || line >= lines) ? SymId::REST_DOUBLE_WHOLE_LEGER_LINE : SymId::REST_DOUBLE_WHOLE;
        case DurationType::V_MEASURE:
            if (ticks() >= Fraction(2, 1)) {
                return (line < 0 || line >= lines) ? SymId::REST_DOUBLE_WHOLE_LEGER_LINE : SymId::REST_DOUBLE_WHOLE;
            }
        // fall through
        case DurationType::V_WHOLE: return (line < 0 || line >= lines) ? SymId::REST_WHOLE_LEGER_LINE : SymId::REST_WHOLE;
        case DurationType::V_HALF: return (line < 0 || line >= lines) ? SymId::REST_HALF_LEGER_LINE : SymId::REST_HALF;
        case DurationType::V_QUARTER: return SymId::REST_QUARTER;
        case DurationType::V_EIGHTH: return SymId::REST8TH;
        case DurationType::V_16TH: return SymId::REST16TH;
        case DurationType::V_32ND: return SymId::REST32ND;
        case DurationType::V_64TH: return SymId::REST64TH;
        case DurationType::V_128TH: return SymId::REST128TH;
        case DurationType::V_256TH: return SymId::REST256TH;
        case DurationType::V_512TH: return SymId::REST512TH;
        case DurationType::V_1024TH: return SymId::REST1024TH;
        default: LOGD("unknown rest type %d", int(type)); return SymId::REST_QUARTER;
    }
}

double Rest::sym_width_no_ledger_lines(LayoutData *ldata) const {
    if (ldata->sym == SymId::REST_HALF_LEGER_LINE) {
        return sym_width(SymId::REST_HALF);
    }
    if (ldata->sym == SymId::REST_WHOLE_LEGER_LINE) {
        return sym_width(SymId::REST_WHOLE);
    }
    if (ldata->sym == SymId::REST_DOUBLE_WHOLE_LEGER_LINE) {
        return sym_width(SymId::REST_DOUBLE_WHOLE);
    }
    return sym_width(ldata->sym);
}

//---------------------------------------------------------
//   checkDots
//---------------------------------------------------------

void Rest::check_dots() {
    int n = dots() - int(dots_.size());
    for (int i = 0; i < n; ++i) {
        NoteDot *dot = Factory::create_note_dot(this);
        dot->set_visible(visible());
        score()->undo_add_element(dot);
    }
    if (n < 0) {
        for (int i = 0; i < -n; ++i) {
            score()->undo_remove_element(dots_.back());
        }
    }
}

//---------------------------------------------------------
//   dot
//---------------------------------------------------------

NoteDot *Rest::dot(int n) {
    check_dots();
    return dots_[n];
}

const std::vector<NoteDot *> &Rest::dot_list() const { return dots_; }

//---------------------------------------------------------
//   getDotline
//---------------------------------------------------------

int Rest::get_dotline(DurationType duration_type) {
    int dl = -1;
    switch (duration_type) {
        case DurationType::V_64TH:
        case DurationType::V_32ND: dl = -3; break;
        case DurationType::V_1024TH:
        case DurationType::V_512TH:
        case DurationType::V_256TH:
        case DurationType::V_128TH: dl = -5; break;
        case DurationType::V_MEASURE:
        case DurationType::V_WHOLE: dl = 1; break;
        default: dl = -1; break;
    }
    return dl;
}

bool Rest::is_whole_rest() const {
    TDuration dur_type = duration_type();
    return dur_type == DurationType::V_WHOLE || (dur_type == DurationType::V_MEASURE && measure() && measure()->ticks() < Fraction(2, 1));
}

bool Rest::is_breve_rest() const {
    TDuration dur_type = duration_type();
    return dur_type == DurationType::V_BREVE || (dur_type == DurationType::V_MEASURE && measure() && measure()->ticks() >= Fraction(2, 1));
}

//---------------------------------------------------------
//   upPos
//---------------------------------------------------------

double Rest::up_pos() const { return sym_bbox(ldata()->sym()).y(); }

//---------------------------------------------------------
//   downPos
//---------------------------------------------------------

double Rest::down_pos() const { return sym_bbox(ldata()->sym()).y() + sym_height(ldata()->sym()); }

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Rest::scan_elements(std::function<void(EngravingItem *)> func) {
    ChordRest::scan_elements(func);
    for (EngravingItem *e : el()) {
        e->scan_elements(func);
    }
    for (NoteDot *dot : dots_) {
        dot->scan_elements(func);
    }
    if (!is_gap() || debug_draw_gap()) {
        func(this);
    }
    if (left_paren()) {
        func(left_paren());
    }
    if (right_paren()) {
        func(right_paren());
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Rest::set_track(track_idx_t val) {
    ChordRest::set_track(val);
    for (NoteDot *dot : dots_) {
        dot->set_track(val);
    }
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Rest::mag() const {
    double m = staff() ? staff()->staff_mag(this) : 1.0;
    return m * intrinsic_mag();
}

//---------------------------------------------------------
//   intrinsicMag
//   returns the INTRINSIC mag of the rest (i.e. NOT scaled
//   by staff size)
//---------------------------------------------------------

double Rest::intrinsic_mag() const {
    double m = 1.0;
    if (is_small()) {
        m *= style().style_d(Sid::SMALL_NOTE_MAG);
    }
    return m;
}

//---------------------------------------------------------
//   rightEdge
//---------------------------------------------------------

double Rest::right_edge() const { return x() + width(); }

double Rest::center_x() const {
    SymId sym = ldata()->sym();
    RectF bbox = sym_bbox(sym);
    return bbox.left() + bbox.width() / 2;
}

//---------------------------------------------------------
//   accent
//---------------------------------------------------------

bool Rest::accent() { return voice() >= 2 && is_small(); }

//---------------------------------------------------------
//   setAccent
//---------------------------------------------------------

void Rest::set_accent(bool flag) {
    undo_change_property(Pid::SMALL, flag);
    if (voice() % 2 == 0) {
        if (flag) {
            double y_offset = -(ldata()->bbox().bottom());
            if (duration_type() >= DurationType::V_HALF) {
                y_offset -= staff()->spatium(tick()) * 0.5;
            }
            mutldata()->move_y(y_offset);
        }
    }
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Rest::add(EngravingItem *e) {
    if (e->ownership_parent() != this) {
        e->set_ownership_parent(this);
    }
    e->set_track(track());

    switch (e->type()) {
        case ElementType::NOTEDOT:
            dots_.push_back(to_note_dot(e));
            e->added();
            break;
        case ElementType::DEAD_SLAPPED:
            dead_slapped_ = to_dead_slapped(e);
            e->added();
            break;
        default: ChordRest::add(e); break;
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Rest::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::NOTEDOT:
            dots_.pop_back();
            e->removed();
            break;
        case ElementType::DEAD_SLAPPED: dead_slapped_ = nullptr;
        // fallthrough
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
            if (!remove_el(e)) {
                LOGD("Rest::remove(): cannot find %s", e->type_name());
            } else {
                e->removed();
            }
            break;
        default: ChordRest::remove(e); break;
    }
}

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Rest::local_spatium_changed(double old_value, double new_value) {
    ChordRest::local_spatium_changed(old_value, new_value);
    for (EngravingItem *e : dots_) {
        e->local_spatium_changed(old_value, new_value);
    }
    for (EngravingItem *e : el()) {
        e->local_spatium_changed(old_value, new_value);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Rest::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::GAP: return false;
        case Pid::ALIGN_WITH_OTHER_RESTS: return true;
        default: return ChordRest::property_default(property_id);
    }
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void Rest::reset_property(Pid id) {
    set_property(id, property_default(id));
    return;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Rest::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::GAP: return gap_;
        case Pid::ALIGN_WITH_OTHER_RESTS: return align_with_other_rests();
        default: return ChordRest::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Rest::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::GAP: gap_ = v.to_bool(); break;
        case Pid::VISIBLE: set_visible(v.to_bool()); break;
        case Pid::OFFSET:
            set_offset(v.value<PointF>());
            if (measure() && duration_type().type() == DurationType::V_MEASURE) {
                measure()->trigger_layout();
            }
            break;
        case Pid::ALIGN_WITH_OTHER_RESTS: set_align_with_other_rests(v.to_bool()); break;
        default: return ChordRest::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   undoChangeDotsVisible
//---------------------------------------------------------

void Rest::undo_change_dots_visible(bool v) {
    for (NoteDot *dot : dots_) {
        dot->undo_change_property(Pid::VISIBLE, v);
    }
}

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *Rest::next_element() { return ChordRest::next_element(); }

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *Rest::prev_element() { return ChordRest::prev_element(); }

//---------------------------------------------------------
//   Rest::shouldNotBeDrawn
//    in tab staff, do not draw rests (except mmrests)
//    if rests are off OR if dur. symbols are on
//    also measures covered by MeasureRepeat show no rests
//---------------------------------------------------------

bool Rest::should_not_be_drawn() const {
    const StaffType *st = staff() ? staff()->staff_type_for_element(this) : nullptr;
    if (generated()) {
        return true;
    }

    if (st && st->is_tab_staff() && (!st->show_rests() || st->gen_durations()) && (!measure() || !measure()->is_mm_rest())) {
        return true;
    }

    if (measure() && measure()->measure_repeat_count(staff_idx())) {
        return true;
    }

    return false;
}

bool Rest::debug_draw_gap() const { return configuration()->debugging_options().show_gap_rests; }

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------
Sid Rest::get_property_style(Pid pid) const { return ChordRest::get_property_style(pid); }
} // namespace mu::engraving
