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

#include "beam.h"

#include <algorithm>
#include <cmath>

#include "beamgrouping.h"
#include "chord.h"
#include "containers.h"
#include "groups.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "realfn.h"
#include "rendering/score/layoutdatastore.h"
#include "rootitem.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "tremolotwochord.h"
#include "tuplet.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle beam_style{
    {Sid::BEAM_NO_SLOPE, Pid::BEAM_NO_SLOPE},
};

//---------------------------------------------------------
//   Beam
//---------------------------------------------------------

Beam::Beam(Score *parent) : BeamBase(ElementType::BEAM, parent->dummy()) {
    init_element_style(&::mu::engraving::beam_style);
    reset_property(Pid::BEAM_CROSS_STAFF_MOVE);
}

System *Beam::system() const {
    if (rendered_elements().empty()) {
        return nullptr;
    }
    Measure *measure = rendered_elements().front()->measure();
    return measure ? measure->system() : nullptr;
}

EngravingItem *Beam::layout_parent() const { return system(); }

staff_idx_t Beam::v_staff_idx() const { return ldata()->visual_staff_idx.value_or(staff_idx()); }

Staff *Beam::visual_staff() const { return has_staff() && !score()->staves().empty() ? score()->staff(v_staff_idx()) : nullptr; }

double Beam::visual_spatium() const {
    const Staff *staff = visual_staff();
    return staff ? staff->spatium(this) : style().spatium();
}

//---------------------------------------------------------
//   Beam
//---------------------------------------------------------

Beam::Beam(const Beam &b) : BeamBase(b) {
    clone_authored_overrides(authored_overrides, b.authored_overrides, *master_score()->eid_register());
    elements_ = b.elements_;
    grow_left_ = b.grow_left_;
    grow_right_ = b.grow_right_;
    beam_dist_ = b.beam_dist_;
    min_cr_move_ = b.min_cr_move_;
    max_cr_move_ = b.max_cr_move_;
    is_grace_ = b.is_grace_;
    cross_ = b.cross_;
    full_cross_ = b.full_cross_;
    slope_ = b.slope_;
}

//---------------------------------------------------------
//   Beam
//---------------------------------------------------------

Beam::~Beam() {
    if (auto *store = score()->layout_data_store()) store->retire_beam_borrowers(*this);
    for (ChordRest *cr : elements_) {
        if (cr->authored_beam() == this) cr->set_beam(nullptr);
    }
}

const std::vector<ChordRest *> &Beam::rendered_elements() const {
    const auto &members = ldata()->members;
    return members ? *members : elements_;
}

std::vector<ChordRest *> &Beam::rendered_elements() {
    auto *data = mutldata();
    if (!data->members) data->members = elements_;
    return *data->members;
}

void Beam::add_rendered(ChordRest *cr) {
    auto &members = rendered_elements();
    if (!muse::contains(members, cr)) {
        const auto at = std::upper_bound(members.begin(), members.end(), cr,
                                         [](const auto *left, const auto *right) { return left->tick() < right->tick(); });
        members.insert(at, cr);
    }
    cr->mutldata()->rendered_beam = this;
}

void Beam::remove_rendered(ChordRest *cr) {
    muse::remove(rendered_elements(), cr);
    cr->mutldata()->rendered_beam = nullptr;
    cr->set_beamlet(nullptr);
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Beam::add(EngravingItem *e) {
    if (e->is_chord_rest()) {
        add_chord_rest(to_chord_rest(e));
        e->added();
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Beam::remove(EngravingItem *e) {
    if (e->is_chord_rest()) {
        remove_chord_rest(to_chord_rest(e));
        e->removed();
    }
}

//---------------------------------------------------------
//   addChordRest
//---------------------------------------------------------

void Beam::add_chord_rest(ChordRest *a) {
    source_child_added(*score(), this);
    a->set_beam(this);
    if (!muse::contains(elements_, a)) {
        //
        // insert element in same order as it appears
        // in the score
        //
        if (a->segment() && !elements_.empty()) {
            for (size_t i = 0; i < elements_.size(); ++i) {
                Segment *s = elements_[i]->segment();
                if ((s->tick() > a->segment()->tick()) ||
                    ((s->tick() == a->segment()->tick()) && (a->segment()->next(SegmentType::CHORD_REST) == s))) {
                    elements_.insert(elements_.begin() + i, a);
                    return;
                }
            }
        }
        elements_.push_back(a);
    }
}

//---------------------------------------------------------
//   removeChordRest
//---------------------------------------------------------

void Beam::remove_chord_rest(ChordRest *a) {
    if (has_layout_data()) remove_rendered(a);
    if (!muse::remove(elements_, a)) {
        LOGD("Beam::remove(): cannot find ChordRest");
    }
    a->set_beam(0);
    if (elements_.empty()) source_child_removed(*score(), this);
}

const Chord *Beam::find_chord_with_custom_stem_direction() const {
    for (const ChordRest *rest : rendered_elements()) {
        if (!rest->is_chord()) {
            continue;
        }

        const Chord *chord = to_chord(rest);
        if (chord->stem_direction() != DirectionV::AUTO) {
            return chord;
        }
    }

    return nullptr;
}

const BeamSegment *Beam::top_level_segment_for_element(const ChordRest *element) const {
    const std::vector<BeamSegment *> &segments = beam_segments();
    size_t segments_size = segments.size();

    IF_ASSERT_FAILED(segments_size > 0) { return nullptr; }

    const BeamSegment *cur_segment = segments[0];
    if (segments_size == 1) {
        return cur_segment;
    }

    for (const BeamSegment *segment : segments) {
        if (segment->level <= cur_segment->level) {
            continue;
        }
        Fraction element_tick = element->tick();
        if (segment->start_tick <= element_tick && segment->end_tick >= element_tick) {
            cur_segment = segment;
        }
    }

    return cur_segment;
}

//---------------------------------------------------------
//   move
//---------------------------------------------------------

void Beam::move(const PointF &offset) {
    EngravingItem::move(offset);
    for (BeamSegment *bs : beam_segments()) {
        bs->line.translate(offset);
    }
}

void Beam::calc_beam_breaks(const ChordRest *cr, const ChordRest *prev_cr, int level, bool &is_broken16, bool &is_broken32) const {
    BeamMode beam_mode = cr->beam_mode();
    if (cr->is_rest() && (beam_mode == BeamMode::MID || beam_mode == BeamMode::BEGIN16 || beam_mode == BeamMode::BEGIN32)) {
        // when a rest has beamMode MID we can just ignore it entirely and allow any beams to continue through
        switch (beam_mode) {
            case BeamMode::MID: is_broken16 = is_broken32 = false; break;
            case BeamMode::BEGIN16:
                is_broken16 = level > 0;
                is_broken32 = false;
                break;
            case BeamMode::BEGIN32:
                is_broken16 = false;
                is_broken32 = level > 1;
                break;
            default:
                // should be unreachable
                assert(false);
        }
        return;
    }
    // get default beam mode -- based on time signature preferences
    const Groups &group = cr->staff()->group(cr->measure()->tick());
    BeamMode default_beam_mode = group.base_beam_mode(cr, prev_cr);

    bool is_manually_broken16 = level >= 1 && beam_mode == BeamMode::BEGIN16;
    bool is_manually_broken32 = level >= 2 && beam_mode == BeamMode::BEGIN32;
    bool is_default_broken16 = beam_mode == BeamMode::AUTO && level >= 1 && default_beam_mode == BeamMode::BEGIN16;
    bool is_default_broken32 = beam_mode == BeamMode::AUTO && level >= 2 && default_beam_mode == BeamMode::BEGIN32;

    is_broken16 = is_manually_broken16 || is_default_broken16;
    is_broken32 = is_manually_broken32 || is_default_broken32;

    // deal with beam-embedded triplets by breaking beams as if they are their underlying durations
    // note that we use max(hooks, 1) here because otherwise we'd end up breaking the main (level 0) beam for
    // tuplets that take up non-beamed amounts of space (eg. 16th note quintuplets)
    // a tuplet's total duration needn't be a single note value, so truncate it
    if (level > 0 && prev_cr && cr->beam_mode() == BeamMode::AUTO) {
        if (cr->tuplet() && cr->tuplet() != prev_cr->tuplet()) {
            // this cr starts a tuplet
            int beams = std::max(TDuration(cr->tuplet()->ticks(), /*truncate*/ true).hooks(), 1);
            if (beams <= level) {
                is_broken16 = level == 1;
                is_broken32 = level >= 2;
            }
        } else if (prev_cr->tuplet() && prev_cr->tuplet() != cr->tuplet()) {
            // this is a non-tuplet cr that is first after a tuplet
            int beams = std::max(TDuration(prev_cr->tuplet()->ticks(), /*truncate*/ true).hooks(), 1);
            if (beams <= level) {
                is_broken16 = level == 1;
                is_broken32 = level >= 2;
            }
        }
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void Beam::spatium_changed(double, double) {}

bool Beam::can_continue_across_measure(const ChordRest *prev, const ChordRest *next) {
    const auto mode = next->beam_mode();
    const auto prev_mode = prev->beam_mode();
    const auto *measure = prev->measure();
    return (mode == BeamMode::MID || mode == BeamMode::END || mode == BeamMode::BEGIN16 || mode == BeamMode::BEGIN32) &&
           prev_mode != BeamMode::END && prev_mode != BeamMode::NONE && prev_mode != BeamMode::INVALID && !measure->line_break() &&
           !measure->page_break() && !measure->section_break() && measure->next() == next->measure() &&
           measure->end_tick() == next->measure()->tick() && !(prev->is_chord() && prev->duration_type().type() <= DurationType::V_QUARTER);
}

std::vector<ChordRest *> Beam::source_elements() const {
    std::vector<ChordRest *> members;
    const auto append = [&](ChordRest *cr) {
        if (!muse::contains(members, cr)) members.push_back(cr);
    };
    for (auto *anchor : elements_) {
        ChordRest *pending = nullptr;
        std::vector<ChordRest *> group;
        bool found = false;
        const auto finish = [&] {
            const auto at = std::find(group.begin(), group.end(), anchor);
            if (at != group.end()) {
                const auto first = std::find_if(group.begin(), group.end(), [](const auto *cr) { return cr->authored_beam(); });
                auto *owner = first == group.end() ? nullptr : (*first)->authored_beam();
                // Leading members belong to the first anchor; subsequent members
                // stay with the latest anchor, so authored ownership never overlaps.
                for (auto *cr : group) {
                    if (cr->authored_beam()) owner = cr->authored_beam();
                    if (owner == this) append(cr);
                }
                found = true;
            }
            group.clear();
        };
        const auto consume = [&](ChordRest *cr, BeamMode mode, bool grace) {
            const auto step = beam_group_step(pending, !group.empty(), cr, mode, grace);
            if (step.finish_before) finish();
            if (step.first) group.push_back(step.first);
            if (step.append) group.push_back(cr);
            if (step.finish_after) finish();
            pending = step.pending;
        };
        if (anchor->is_grace()) {
            auto *main = to_chord(anchor->ownership_parent());
            for (const auto &notes : {main->grace_notes_before(), main->grace_notes_after()}) {
                if (std::find(notes.begin(), notes.end(), anchor) == notes.end()) continue;
                for (auto *cr : notes) consume(cr, Groups::grace_beam_mode(cr), true);
                finish();
            }
        } else if (anchor->segment()) {
            ChordRest *previous_measure_last = nullptr;
            for (auto *measure = score()->first_measure(); measure && !found; measure = measure->next_measure()) {
                const ChordRest *prev = nullptr;
                bool first = true;
                for (auto *segment = measure->first(SegmentType::CHORD_REST); segment; segment = segment->next(SegmentType::CHORD_REST)) {
                    auto *cr = to_chord_rest(segment->element(anchor->track()));
                    if (!cr) continue;
                    if (first && (!previous_measure_last || !can_continue_across_measure(previous_measure_last, cr))) {
                        finish();
                        pending = nullptr;
                    }
                    first = false;
                    consume(cr, Groups::actual_beam_mode(cr, prev), false);
                    prev = cr;
                    previous_measure_last = cr;
                }
                if (first) {
                    finish();
                    pending = nullptr;
                    previous_measure_last = nullptr;
                }
            }
            finish();
        }
        // An authored beam can remain editable while its only chord is unbeamed.
        if (!found) append(anchor);
    }
    return members;
}

std::pair<int, int> Beam::source_staff_move_bounds() const {
    std::optional<std::pair<int, int>> bounds;
    for (const auto *member : source_elements()) {
        if (!member->is_chord()) continue;
        const int move = member->staff_move();
        if (!bounds)
            bounds = {move, move};
        else {
            bounds->first = std::min(bounds->first, move);
            bounds->second = std::max(bounds->second, move);
        }
    }
    return bounds.value_or(std::pair{0, 0});
}

void Beam::set_direction(DirectionV d) {
    const auto [min_move, max_move] = source_staff_move_bounds();
    if (direction() == d || min_move != max_move) {
        return;
    }

    do_set_direction(d);

    const auto members = source_elements();
    for (ChordRest *e : members) {
        if (e->is_chord()) {
            to_chord(e)->undo_change_property(Pid::STEM_DIRECTION, d);
        }
    }
}

void Beam::set_as_feathered(const bool slower) {
    if (slower) {
        undo_change_property(Pid::GROW_LEFT, 1.0);
        undo_change_property(Pid::GROW_RIGHT, 0.0);
    } else {
        undo_change_property(Pid::GROW_LEFT, 0.0);
        undo_change_property(Pid::GROW_RIGHT, 1.0);
    }
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Beam::reset() {
    if (!real_is_equal(grow_left(), 1.0)) {
        undo_change_property(Pid::GROW_LEFT, 1.0);
    }
    if (!real_is_equal(grow_right(), 1.0)) {
        undo_change_property(Pid::GROW_RIGHT, 1.0);
    }
    if (user_modified()) {
        undo_change_property(Pid::BEAM_POS, PropertyValue::from_value(beam_pos()));
        undo_change_property(Pid::USER_MODIFIED, false);
    }
    undo_change_property(Pid::STEM_DIRECTION, DirectionV::AUTO);
    undo_reset_property(Pid::BEAM_NO_SLOPE);
    undo_reset_property(Pid::BEAM_CROSS_STAFF_MOVE);
    undo_change_property(Pid::GENERATED, true);
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void Beam::trigger_layout() const {
    if (!elements_.empty()) {
        elements_.front()->trigger_layout();
        elements_.back()->trigger_layout();
    }
}

//---------------------------------------------------------
//   beamPos
//---------------------------------------------------------

PairF Beam::beam_pos() const {
    const AuthoredOverrideEntry *last = nullptr;
    for (const auto &entry : authored_overrides.entries) {
        if (entry.child.role == AuthoredChildRole::BEAM_FRAGMENT && entry.child.direction == direction_idx() && entry.pid == Pid::BEAM_POS)
            if (!last || last->child.ordinal < entry.child.ordinal) last = &entry;
    }
    return last ? last->value.value<PairF>() : PairF(0.0, 0.0);
}

//---------------------------------------------------------
//   setBeamPos
//---------------------------------------------------------

void Beam::set_beam_pos(const PairF &bp) {
    const auto &fragments = ldata()->fragments;
    uint32_t ordinal = fragments.empty() ? 0 : static_cast<uint32_t>(fragments.size() - 1);
    bool found = false;
    for (const auto &entry : authored_overrides.entries) {
        if (entry.child.role == AuthoredChildRole::BEAM_FRAGMENT && entry.child.direction == direction_idx() &&
            entry.pid == Pid::BEAM_POS) {
            ordinal = found ? std::max(ordinal, entry.child.ordinal) : entry.child.ordinal;
            found = true;
        }
    }
    const PairF value = no_slope() ? PairF((bp.first + bp.second) * 0.5, (bp.first + bp.second) * 0.5) : bp;
    authored_overrides.set({{AuthoredChildRole::BEAM_FRAGMENT, ordinal, static_cast<uint8_t>(direction_idx())},
                            Pid::BEAM_POS,
                            PropertyValue::from_value(value),
                            PropertyFlags::NOSTYLE});
}

void Beam::set_no_slope(bool b) {
    no_slope_ = b;
    if (b && user_modified()) set_beam_pos(beam_pos());
}

void Beam::compute_and_set_slope() {
    double x_diff = end_anchor_.x() - start_anchor_.x();
    double y_diff = end_anchor_.y() - start_anchor_.y();
    if (std::abs(x_diff) < 0.5 * visual_spatium()) {
        // Temporary safeguard: a beam this short is invalid, and exists only as a temporary state,
        // so don't try to compute the slope as it will be wrong. Needs a better solution in future.
        set_slope(0.0);
    } else {
        set_slope(y_diff / x_diff);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Beam::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::GROW_LEFT: return grow_left();
        case Pid::GROW_RIGHT: return grow_right();
        case Pid::BEAM_POS: return PropertyValue::from_value(beam_pos());
        case Pid::BEAM_NO_SLOPE: return no_slope();
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            for (ChordRest *chord_rest : elements()) {
                bool linked = chord_rest->get_property(property_id).to_bool();
                if (!linked) {
                    return false;
                }
            }
            return true;
        default: return BeamBase::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Beam::set_property(Pid property_id, const PropertyValue &v) {
    if (property_id == Pid::GENERATED) return EngravingItem::set_property(property_id, v);
    switch (property_id) {
        case Pid::GROW_LEFT: set_grow_left(v.to_double()); break;
        case Pid::GROW_RIGHT: set_grow_right(v.to_double()); break;
        case Pid::BEAM_POS: set_beam_pos(v.value<PairF>()); break;
        case Pid::USER_MODIFIED: set_user_modified(v.to_bool()); break;
        case Pid::BEAM_NO_SLOPE: set_no_slope(v.to_bool()); break;
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (v.to_bool() == true) {
                for (ChordRest *chord_rest : elements()) {
                    // when re-linking, re-link all the chords
                    chord_rest->set_property(property_id, v);
                }
                reset_property(Pid::STEM_DIRECTION);
                break;
            }
        // fall through
        default:
            if (!BeamBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    if (property_id != Pid::BEAM_POS && property_id != Pid::BEAM_NO_SLOPE && property_id != Pid::USER_MODIFIED) set_generated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Beam::property_default(Pid id) const {
    switch (id) {
        case Pid::GROW_LEFT: return 1.0;
        case Pid::GROW_RIGHT: return 1.0;
        case Pid::BEAM_POS: return PropertyValue::from_value(beam_pos());
        default: return BeamBase::property_default(id);
    }
}

//---------------------------------------------------------
//   addSkyline
//    add beam shape to skyline
//---------------------------------------------------------

void Beam::add_skyline(Skyline &sk) {
    if (beam_segments().empty() || !add_to_skyline()) {
        return;
    }

    // Only add the outer segment, no need to add the inner one
    sk.add(beam_segments().front()->shape());
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction Beam::tick() const {
    const auto &members = is_layout_copy() ? rendered_elements() : elements_;
    return members.empty() ? Fraction(0, 1) : members.front()->segment()->tick();
}

//---------------------------------------------------------
//   rtick
//---------------------------------------------------------

Fraction Beam::rtick() const {
    const auto &members = is_layout_copy() ? rendered_elements() : elements_;
    return members.empty() ? Fraction(0, 1) : members.front()->segment()->rtick();
}

//---------------------------------------------------------
//   ticks
//    calculate the ticks of all chords and rests connected by the beam
//---------------------------------------------------------

Fraction Beam::ticks() const {
    Fraction ticks = Fraction(0, 1);
    for (ChordRest *cr : rendered_elements()) {
        ticks += cr->actual_ticks();
    }
    return ticks;
}

//---------------------------------------------------------
//   actionIconTypeForBeamMode
//---------------------------------------------------------

ActionIconType Beam::action_icon_type_for_beam_mode(BeamMode mode) {
    switch (mode) {
        case BeamMode::AUTO: return ActionIconType::BEAM_AUTO;
        case BeamMode::NONE: return ActionIconType::BEAM_NONE;
        case BeamMode::BEGIN: return ActionIconType::BEAM_BREAK_LEFT;
        case BeamMode::BEGIN16: return ActionIconType::BEAM_BREAK_INNER_8TH;
        case BeamMode::BEGIN32: return ActionIconType::BEAM_BREAK_INNER_16TH;
        case BeamMode::MID: return ActionIconType::BEAM_JOIN;
        default: break;
    }
    return ActionIconType::UNDEFINED;
}

//---------------------------------------------------------
//   isMovable
//---------------------------------------------------------
bool Beam::is_movable() const { return true; }

//---------------------------------------------------------
//   startDrag
//---------------------------------------------------------
} // namespace mu::engraving

//---------------------------------------------------------
//   containsChord
//---------------------------------------------------------
bool Beam::has_all_rests() {
    for (ChordRest *cr : rendered_elements()) {
        if (cr && cr->is_chord()) {
            return false;
        }
    }
    return true;
}

void Beam::clear_beam_segments() {
    for (ChordRest *chord_rest : rendered_elements()) {
        chord_rest->set_beamlet(nullptr);
    }

    BeamBase::clear_beam_segments();
}

//-------------------------------------------------------
// BEAM SEGMENT CLASS
//-------------------------------------------------------

BeamSegment::BeamSegment(EngravingItem *b) : parent_element(b) {
    DO_ASSERT(parent_element->is_type(ElementType::BEAM) || parent_element->is_type(ElementType::TREMOLO_TWOCHORD));
}

Shape BeamSegment::shape() const {
    Shape shape;
    PointF start_point = line.p1();
    PointF end_point = line.p2();
    double beam_width = parent_element->is_beam() ? item_cast<const Beam *>(parent_element)->beam_width_ :
                                                    item_cast<const TremoloTwoChord *>(parent_element)->ldata()->beam_width;
    // This is the case of right-beamlets
    if (start_point.x() > end_point.x()) {
        std::swap(start_point, end_point);
    }
    double beam_horizontal_length = end_point.x() - start_point.x();
    // If beam is horizontal, one rectangle is enough
    if (muse::real_is_equal(start_point.y(), end_point.y())) {
        RectF rect(start_point.x(), start_point.y(), beam_horizontal_length, beam_width / 2);
        rect.adjust(0.0, -beam_width / 2, 0.0, 0.0);
        shape.add(rect, parent_element);
        return shape;
    }
    // If not, break the beam shape into multiple rectangles
    double beam_height_diff = end_point.y() - start_point.y();
    const double spatium = parent_element->is_beam() ? to_beam(parent_element)->visual_spatium() : parent_element->spatium();
    int sub_boxes_count = floor(beam_horizontal_length / spatium);
    sub_boxes_count = std::max(sub_boxes_count, 1); // at least one rectangle, of course (avoid division by zero)
    double horizontal_step = beam_horizontal_length / sub_boxes_count;
    double vertical_step = beam_height_diff / sub_boxes_count;
    std::vector<PointF> points_on_beam_line;
    points_on_beam_line.reserve(sub_boxes_count);
    points_on_beam_line.push_back(start_point);
    for (int i = 0; i < sub_boxes_count - 1; ++i) {
        PointF next_point = points_on_beam_line.back() + PointF(horizontal_step, vertical_step);
        points_on_beam_line.push_back(next_point);
    }
    for (PointF point : points_on_beam_line) {
        RectF rect(point.x(), point.y(), horizontal_step, beam_width / 2);
        rect.adjust(0.0, -beam_width / 2, 0.0, 0.0);
        shape.add(rect, parent_element);
    }
    return shape;
}

Beam::LayoutData::~LayoutData() { muse::delete_all(fragments); }
