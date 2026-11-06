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

#include "barline.h"

#include "../types/symnames.h"
#include "articulation.h"
#include "log.h"
#include "marker.h"
#include "masterscore.h"
#include "measure.h"
#include "part.h"
#include "playcounttext.h"
#include "score.h"
#include "segment.h"
#include "spanner.h"
#include "staff.h"
#include "stafflines.h"
#include "stafftype.h"
#include "system.h"
#include "translation.h"

using namespace mu;
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;
using namespace mu::engraving::read400;

//---------------------------------------------------------
//   BarLineTable
//---------------------------------------------------------

const std::vector<BarLineTableItem> BarLine::bar_line_table{
    {BarLineType::NORMAL, SymNames::user_name_for_sym_id(SymId::BARLINE_SINGLE)},
    {BarLineType::DOUBLE, SymNames::user_name_for_sym_id(SymId::BARLINE_DOUBLE)},
    {BarLineType::START_REPEAT, SymNames::user_name_for_sym_id(SymId::REPEAT_LEFT)},
    {BarLineType::END_REPEAT, SymNames::user_name_for_sym_id(SymId::REPEAT_RIGHT)},
    {BarLineType::BROKEN, SymNames::user_name_for_sym_id(SymId::BARLINE_DASHED)},
    {BarLineType::END, SymNames::user_name_for_sym_id(SymId::BARLINE_FINAL)},
    {BarLineType::END_START_REPEAT, SymNames::user_name_for_sym_id(SymId::REPEAT_RIGHT_LEFT)},
    {BarLineType::DOTTED, SymNames::user_name_for_sym_id(SymId::BARLINE_DOTTED)},
    {BarLineType::REVERSE_END, SymNames::user_name_for_sym_id(SymId::BARLINE_REVERSE_FINAL)},
    {BarLineType::HEAVY, SymNames::user_name_for_sym_id(SymId::BARLINE_HEAVY)},
    {BarLineType::DOUBLE_HEAVY, SymNames::user_name_for_sym_id(SymId::BARLINE_HEAVY_HEAVY)},
};

//---------------------------------------------------------
//   translatedUserTypeName
//---------------------------------------------------------

String BarLine::translated_user_type_name(BarLineType t) {
    for (const auto &i : ::mu::engraving::BarLine::bar_line_table) {
        if (i.type == t) {
            return i.user_name.translated();
        }
    }
    return String();
}

//---------------------------------------------------------
//   BarLine
//---------------------------------------------------------

BarLine::BarLine(DummyParentOr<Segment> parent) : EngravingItem(ElementType::BAR_LINE, parent) {
    set_height(4 * spatium()); // for use in palettes
}

BarLine::BarLine(const BarLine &bl) : EngravingItem(bl) {
    span_staff_ = bl.span_staff_;
    span_from_ = bl.span_from_;
    span_to_ = bl.span_to_;
    bar_line_type_ = bl.bar_line_type_;
    play_count_ = bl.play_count_;

    for (EngravingItem *e : bl.el_) {
        add(e->clone());
    }
}

BarLine::~BarLine() { muse::delete_all(el_); }

void BarLine::set_ownership_parent(DummyParentOr<Segment> parent) { EngravingItem::set_ownership_parent(parent); }

//---------------------------------------------------------
//   canvasPos
//---------------------------------------------------------

PointF BarLine::canvas_pos() const {
    PointF pos = EngravingItem::canvas_pos();
    if (ownership_parent()) {
        System *system = measure()->system();
        double yoff = system ? system->staff(staff_idx())->y() : 0.0;
        pos.ry() += yoff;
    }
    return pos;
}

//---------------------------------------------------------
//   pagePos
//---------------------------------------------------------

PointF BarLine::page_pos() const {
    if (segment() == 0) {
        return pos();
    }
    System *system = segment()->measure()->system();

    double yp = y();
    if (system) {
        // get first not hidden staff
        staff_idx_t start_idx = staff_idx();
        staff_idx_t end_idx = start_idx + (span_staff() ? 1 : 0);
        staff_idx_t staff_idx1 = start_idx;
        Staff *staff1 = score()->staff(staff_idx1);
        SysStaff *sys_staff1 = system->staff(staff_idx1);

        while (staff1 && sys_staff1 && !(sys_staff1->show() && staff1->show())) {
            if (++staff_idx1 >= end_idx) {
                // no visible staves spanned; just use first
                staff_idx1 = start_idx;
                break;
            }
            staff1 = score()->staff(staff_idx1);
            sys_staff1 = system->staff(staff_idx1);
        }
        yp += system->staff_ypage(staff_idx1);
    }
    return PointF(page_x(), yp);
}

//---------------------------------------------------------
//   prevVisiblespannedStaff
//---------------------------------------------------------

static staff_idx_t prev_visible_spanned_staff(const BarLine *bl) {
    Score *score = bl->score();
    int staff_idx = static_cast<int>(bl->staff_idx());
    Segment *segment = bl->segment();
    for (int i = staff_idx - 1; i >= 0; --i) {
        BarLine *nbl = to_bar_line(segment->element(i * ::mu::engraving::voices));
        if (!nbl || !nbl->span_staff()) {
            break;
        }
        Staff *s = score->staff(i);
        if (s->part()->show() && bl->measure()->visible(i)) {
            return i;
        }
    }
    return staff_idx;
}

//---------------------------------------------------------
//   nextVisiblespannedStaff
//---------------------------------------------------------

static size_t next_visible_spanned_staff(const BarLine *bl) {
    Score *score = bl->score();
    size_t nstaves = score->nstaves();
    size_t staff_idx = bl->staff_idx();
    Segment *segment = bl->segment();
    for (size_t i = staff_idx + 1; i < nstaves; ++i) {
        Staff *s = score->staff(i);
        if (s->part()->show()) {
            // span/show bar line if this measure is visible
            if (bl->measure()->visible(i)) {
                return i;
            }
            // or if this is an endBarLine and:
            if (segment && segment->is_end_bar_line_type()) {
                // ...this measure contains a (cutaway) courtesy clef only
                if (bl->measure()->is_cutaway_clef(i)) {
                    return i;
                }
                // ...or next measure is both visible and in the same system
                Measure *nm = bl->measure()->next_measure();
                if ((nm ? nm->visible(i) : false) && (nm ? nm->system() == bl->measure()->system() : false)) {
                    return i;
                }
            }
        }
        BarLine *nbl = to_bar_line(segment->element(i * ::mu::engraving::voices));
        if (!nbl || !nbl->span_staff()) {
            break;
        }
    }
    return staff_idx;
}

//---------------------------------------------------------
//   calcY
//---------------------------------------------------------

void BarLine::calc_y() {
    BarLine::LayoutData *data = mutldata();
    double spatium_value = spatium();
    if (!ownership_parent()) {
        // for use in palette
        data->y1 = span_from_ * spatium_value * .5;
        data->y2 = (8 - span_to_) * spatium_value * .5;
        return;
    }
    staff_idx_t staff_idx1 = staff_idx();
    staff_idx_t staff_idx2 = span_staff_ ? next_visible_spanned_staff(this) : staff_idx1;

    bool span_staff = staff_idx2 != staff_idx1;

    Measure *measure = segment()->measure();
    System *system = measure->system();
    if (!system) {
        return;
    }

    Fraction tick = segment()->measure()->tick();
    const Staff *staff1 = score()->staff(staff_idx1);
    const StaffType *staff_type1 = staff1->staff_type(tick);

    bool one_line = staff_type1->lines() <= 1;

    int from = span_from_;
    int to = span_to_;

    if (one_line && span_from_ == 0 && span_to_ == 0) {
        from = ::mu::engraving::barline_span_1_linestaff_from;
        if (!span_staff) {
            to = ::mu::engraving::barline_span_1_linestaff_to;
        }
    }

    double spatium1 = staff_type1->spatium();
    double line_distance = staff_type1->line_distance().val() * spatium1;
    double offset = staff_type1->yoffset().val() * spatium1;
    double line_width = style().style_s(Sid::STAFF_LINE_WIDTH).val() * spatium1 * .5;

    double y1 = offset + from * line_distance * .5 - line_width;
    double y2 = offset + (staff_type1->lines() * 2 - 2 + to) * line_distance * .5 + line_width;

    if (span_staff) {
        // we need spatium and line distance of bottom staff
        // as it may be scalled diferently
        const Staff *staff2 = score()->staff(staff_idx2);
        const StaffType *staff_type2 = staff2 ? staff2->staff_type(tick) : staff_type1;
        double spatium2 = staff_type2->spatium();
        double line_distance2 = staff_type2->line_distance().val() * spatium2;
        double start_staff_y = system->staff(staff_idx1)->y();

        y2 = measure->staff_lines(staff_idx2)->y1() - start_staff_y - to * line_distance2 * 0.5;

        // if bottom staff is single line, set span-to zeropoint to the top of the standard barline
        if (staff_type2->lines() <= 1) {
            y2 += ::mu::engraving::barline_span_1_linestaff_from * line_distance2 * 0.5;
        }
    }

    // if stafftype change in next measure, check new staff positions
    Fraction tick_next = tick + measure->ticks();
    if (staff1->is_staff_type_start_from(tick_next)) {
        Measure *measure_next = measure->next_measure();
        System *system_next = measure_next ? measure_next->system() : nullptr;
        const StaffType *staff_type1_next = staff1->staff_type(tick_next);
        bool one_line_next = staff_type1_next->lines() <= 1;
        if (system_next && system_next == system && staff_type1_next != staff_type1) {
            if (one_line && !one_line_next) {
                from = span_from_;
                to = span_to_;
            }
            if (one_line_next && span_from_ == 0 && span_to_ == 0) {
                from = ::mu::engraving::barline_span_1_linestaff_from;
                if (!span_staff) {
                    to = ::mu::engraving::barline_span_1_linestaff_to;
                }
            }
            double spatium1_next = staff_type1_next->spatium();
            double line_distance_next = staff_type1_next->line_distance().val() * spatium1_next;
            double offset_next = staff_type1_next->yoffset().val() * spatium1_next;
            double line_width_next = style().style_s(Sid::STAFF_LINE_WIDTH).val() * spatium1_next * .5;

            int y1_next = offset_next + from * line_distance_next * .5 - line_width_next;

            // change barline, only if it is not initial one
            if (rtick().is_not_zero()) {
                if (y1_next < y1) {
                    y1 = y1_next;
                }

                if (!span_staff) {
                    double y2_next = offset_next + (staff_type1_next->lines() * 2 - 2 + to) * line_distance_next * .5 + line_width_next;
                    if (y2_next > y2) {
                        y2 = y2_next;
                    }
                }
            }
        }
    }

    data->y1 = y1;
    data->y2 = y2;
}

//---------------------------------------------------------
//   isTop
//---------------------------------------------------------

bool BarLine::is_top() const {
    staff_idx_t idx = staff_idx();
    if (idx == 0) {
        return true;
    } else {
        return prev_visible_spanned_staff(this) == idx;
    }
}

//---------------------------------------------------------
//   isBottom
//---------------------------------------------------------

bool BarLine::is_bottom() const {
    if (!span_staff_) {
        return true;
    }
    size_t idx = staff_idx();
    if (idx == score()->nstaves() - 1) {
        return true;
    } else {
        return next_visible_spanned_staff(this) == idx;
    }
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool BarLine::accept_drop(EditData &data) const {
    EngravingItem *e = data.drop_element;

    if (e->is_bar_line()) {
        return true;
    } else if (e->is_fermata() || e->is_symbol() || e->is_image()) {
        return segment() && segment()->is_end_bar_line_type();
    } else if (e->is_measure_number() || e->is_jump() || e->is_marker() || e->is_layout_break()) {
        if (Measure *m = measure()) {
            bool left = (e->is_marker() && !to_marker(e)->is_right_marker()) || e->is_measure_number();
            if (left && segment()->is_end_bar_line_type() && m->next_measure_mm()) {
                m = m->next_measure_mm();
            }
            return m->accept_drop(data);
        }
    }
    return false;
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *BarLine::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;

    if (e->is_bar_line()) {
        BarLine *bl = to_bar_line(e);
        BarLineType st = bl->bar_line_type();

        if (segment()->segment_type() == SegmentType::BAR_LINE) {
            // barline exists mid measure
            undo_change_property(Pid::BARLINE_TYPE, st);
            undo_change_property(Pid::BARLINE_SPAN, false);
            undo_change_property(Pid::BARLINE_SPAN_FROM, bl->span_from());
            undo_change_property(Pid::BARLINE_SPAN_TO, bl->span_to());
        } else {
            // check if the new property can apply to this single bar line
            BarLineType bt = BarLineType::START_REPEAT | BarLineType::END_REPEAT | BarLineType::END_START_REPEAT;
            bool old_repeat = bar_line_type() & bt;
            bool new_repeat = bl->bar_line_type() & bt;

            Measure *m = measure();
            if (bl->play_count() != -1) {
                m->undo_change_property(Pid::REPEAT_COUNT, bl->play_count());
            }

            // if ctrl was used and repeats are not involved,
            // or if drop refers to span rather than subtype =>
            // single bar line drop

            if ((data.control() && !old_repeat && !new_repeat) || (bl->span_from() || bl->span_to())) {
                // if drop refers to span, update this bar line span
                if (bl->span_from() || bl->span_to()) {
                    // if dropped spanFrom or spanTo are below the middle of standard staff (5 lines)
                    // adjust to the number of staff lines
                    int span_from = bl->span_from();
                    int span_to = bl->span_to();
                    undo_change_property(Pid::BARLINE_SPAN, false);
                    undo_change_property(Pid::BARLINE_SPAN_FROM, span_from);
                    undo_change_property(Pid::BARLINE_SPAN_TO, span_to);
                }
                // if drop refers to subtype, update this bar line subtype
                else {
                    score()->undo_change_bar_line_type(this, st, false);
                }
            } else {
                score()->undo_change_bar_line_type(this, st, true);
            }
            score()->undo_update_play_count_text(m);
        }
        delete e;
    } else if (e->is_articulation_family()) {
        Articulation *atr = to_articulation(e);
        atr->set_ownership_parent(this);
        atr->set_track(track());
        score()->undo_add_element(atr);
        return atr;
    } else if (e->is_symbol() || e->is_image()) {
        e->set_ownership_parent(this);
        e->set_track(track());
        score()->undo_add_element(e);
        return e;
    } else if (e->is_fermata()) {
        e->set_placement(track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE);
        for (EngravingItem *el : segment()->annotations()) {
            if (el->is_fermata() && (el->track() == track())) {
                if (el->subtype() == e->subtype()) {
                    delete e;
                    return el;
                } else {
                    e->set_placement(el->placement());
                    e->set_track(track());
                    e->set_ownership_parent(segment());
                    score()->undo_change_element(el, e);
                    return e;
                }
            }
        }
        e->set_track(track());
        e->set_ownership_parent(segment());
        score()->undo_add_element(e);
        return e;
    } else if (e->is_measure_number() || e->is_jump() || e->is_marker() || e->is_layout_break()) {
        if (Measure *m = measure()) {
            bool left = (e->is_marker() && !to_marker(e)->is_right_marker()) || e->is_measure_number();
            if (left && segment()->is_end_bar_line_type() && m->next_measure_mm()) {
                m = m->next_measure_mm();
            }
            return m->drop(tx, data);
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   setShowTips
//---------------------------------------------------------

void BarLine::set_show_tips(bool val) {
    if (!score()) {
        return;
    }

    score()->undo_change_style_val(Sid::REPEAT_BAR_TIPS, val);
}

//---------------------------------------------------------
//   showTips
//---------------------------------------------------------

bool BarLine::show_tips() const {
    if (!score()) {
        return false;
    }

    return style().style_b(Sid::REPEAT_BAR_TIPS);
}

void BarLine::set_selected(bool f) {
    if (f == selected()) {
        return;
    }

    if (play_count_text()) {
        play_count_text()->set_selected(f);
    }
    EngravingItem::set_selected(f);
}

//---------------------------------------------------------
//   gripsPositions
//---------------------------------------------------------

void BarLine::style_changed() {
    if (ownership_parent() && bar_line_type_ == BarLineType::END_REPEAT) {
        score()->undo_update_play_count_text(measure());
    }
    EngravingItem::style_changed();
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void BarLine::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);
    for (EngravingItem *e : el_) {
        e->scan_elements(func);
    }
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void BarLine::set_track(track_idx_t t) {
    EngravingItem::set_track(t);
    for (EngravingItem *e : el_) {
        e->set_track(t);
    }
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void BarLine::add(EngravingItem *e) {
    e->set_ownership_parent(this);
    switch (e->type()) {
        case ElementType::ARTICULATION:
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
            el_.push_back(e);
            set_generated(false);
            e->added();
            break;
        default: LOGD("BarLine::add() not impl. %s", e->type_name()); break;
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void BarLine::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::ARTICULATION:
        case ElementType::SYMBOL:
        case ElementType::IMAGE:
            if (!el_.remove(e)) {
                LOGD("BarLine::remove(): cannot find %s", e->type_name());
            } else {
                e->removed();
            }
            break;
        default: LOGD("BarLine::remove() not impl. %s", e->type_name()); return;
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue BarLine::get_property(Pid id) const {
    if (EngravingObject *e = const_cast<BarLine *>(this)->property_delegate(id)) {
        return e->get_property(id);
    }

    switch (id) {
        case Pid::BARLINE_TYPE: return PropertyValue::from_value(bar_line_type_);
        case Pid::BARLINE_SPAN: return span_staff();
        case Pid::BARLINE_SPAN_FROM: return span_from();
        case Pid::BARLINE_SPAN_TO: return span_to();
        case Pid::BARLINE_SHOW_TIPS: return show_tips();
        default: break;
    }
    return EngravingItem::get_property(id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool BarLine::set_property(Pid id, const PropertyValue &v) {
    if (EngravingObject *e = property_delegate(id)) {
        return e->set_property(id, v);
    }

    switch (id) {
        case Pid::BARLINE_TYPE: set_bar_line_type(v.value<BarLineType>()); break;
        case Pid::BARLINE_SPAN: set_span_staff(v.to_bool()); break;
        case Pid::BARLINE_SPAN_FROM: set_span_from(v.to_int()); break;
        case Pid::BARLINE_SPAN_TO: set_span_to(v.to_int()); break;
        case Pid::BARLINE_SHOW_TIPS: set_show_tips(v.to_bool()); break;
        default: return EngravingItem::set_property(id, v);
    }
    set_generated(false);
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void BarLine::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (EngravingObject *e = property_delegate(id)) {
        e->undo_change_property(id, v, ps);
        return;
    }

    if (id == Pid::BARLINE_TYPE && segment() && segment()->segment_type() != SegmentType::BAR_LINE) {
        score()->undo_change_bar_line_type(this, v.value<BarLineType>(), true, true);
    } else {
        EngravingObject::undo_change_property(id, v, ps);
    }
}

EngravingObject *BarLine::property_delegate(Pid pid) const {
    if (pid == Pid::REPEAT_COUNT) {
        return measure();
    } else if (pid == Pid::PLAY_COUNT_TEXT || pid == Pid::PLAY_COUNT_TEXT_SETTING) {
        return play_count_text();
    }

    return EngravingItem::property_delegate(pid);
}

PlayCountText *BarLine::play_count_text() const {
    Segment *end_bar_seg = segment();
    if (!end_bar_seg) {
        return nullptr;
    }
    PlayCountText *play_count_text = to_play_count_text(end_bar_seg->find_annotation(ElementType::PLAY_COUNT_TEXT, track(), track()));

    return play_count_text;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue BarLine::property_default(Pid property_id) const {
    if (EngravingObject *e = const_cast<BarLine *>(this)->property_delegate(property_id)) {
        return e->property_default(property_id);
    }

    switch (property_id) {
        case Pid::BARLINE_TYPE:
            // dynamic default values are a bad idea: writing to xml the value maybe omitted resulting in
            //    wrong values on read (as the default may be different on read)
            //                  if (segment() && segment()->measure() && !segment()->measure()->nextMeasure())
            //                        return QVariant::fromValue(BarLineType::END);
            return PropertyValue::from_value(BarLineType::NORMAL);

        case Pid::BARLINE_SPAN: return staff() ? staff()->bar_line_span() : false;

        case Pid::BARLINE_SPAN_FROM: return staff() ? staff()->bar_line_from() : 0;

        case Pid::BARLINE_SPAN_TO: return staff() ? staff()->bar_line_to() : 0;

        case Pid::BARLINE_SHOW_TIPS: return false;

        default: break;
    }
    return EngravingItem::property_default(property_id);
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *BarLine::next_segment_element() {
    return segment()->first_in_next_segments(staff_idx()); //score()->inputState().prevTrack() / VOICES);
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *BarLine::prev_segment_element() {
    return segment()->last_in_prev_segments(staff_idx()); //score()->inputState().prevTrack() / VOICES);
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString BarLine::subtype_user_name() const {
    for (const auto &i : ::mu::engraving::BarLine::bar_line_table) {
        if (i.type == bar_line_type()) {
            return i.user_name;
        }
    }
    return TranslatableString();
}
