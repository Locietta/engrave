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

#include "line.h"

#include <vector>

#include "../editing/navigation.h"
#include "anchors.h"
#include "barline.h"
#include "chord.h"
#include "containers.h"
#include "dynamic.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "engraving/rendering/score/scorerenderer.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "page.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "utils.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
LineSegment::LineSegment(const ElementType &type, SLine *sp, ElementFlags f) : SpannerSegment(type, sp, f) {}

//---------------------------------------------------------
//   LineSegment
//---------------------------------------------------------

LineSegment::LineSegment(const LineSegment &s) : SpannerSegment(s) {}

//---------------------------------------------------------
//   leftAnchorPosition
//---------------------------------------------------------

PointF LineSegment::left_anchor_position(const double &system_position_y) const {
    if (is_middle_type() || is_end_type()) {
        return PointF(system()->first_measure()->page_bounding_rect().left(), system_position_y);
    }

    PointF result;

    Segment *start_seg = line()->start_segment();
    if (!start_seg) {
        return result;
    }

    System *s = start_seg->measure()->system();
    result = start_seg->pos() + start_seg->measure()->pos();
    result.ry() += system_position_y - system()->pos().y();

    if (s) {
        result += s->pos(); // to page coordinates
    }
    return result;
}

//---------------------------------------------------------
//   rightAnchorPosition
//---------------------------------------------------------

PointF LineSegment::right_anchor_position(const double &system_position_y) const {
    const System *sys = system();
    IF_ASSERT_FAILED(sys) { return PointF(); }

    bool ends_at_system_end = is_single_end_type() && line()->tick2() == sys->end_tick();
    if (is_middle_type() || is_begin_type() || ends_at_system_end) {
        return PointF(sys->ending_x_for_open_ended_lines() + sys->x(), system_position_y);
    }

    PointF result;

    Segment *end_seg = line()->end_segment();
    if (!end_seg) {
        return result;
    }

    System *s = end_seg->measure()->system();
    result = end_seg->pos() + end_seg->measure()->pos();
    result.ry() += system_position_y - system()->pos().y();

    if (s) {
        result += s->pos(); // to page coordinates
    }
    return result;
}

//---------------------------------------------------------
//   findSegmentForGrip
//---------------------------------------------------------

Segment *LineSegment::find_segment_for_grip(Grip grip, PointF pos) const {
    if (grip != Grip::START && grip != Grip::END) {
        return nullptr;
    }

    SLine *l = line();
    const bool left = (grip == Grip::START);

    const staff_idx_t old_staff_index = left ? staff_idx() : track2staff(l->effective_track2());

    const double spacing_factor = 0.5; // defines the point where canvas is divided between segments, systems etc.

    System *sys = system();
    const std::vector<System *> found_systems = score()->search_system(pos, /*includeMeasurelessSystems*/ false, sys, spacing_factor);

    if (!found_systems.empty() && !muse::contains(found_systems, sys) && found_systems[0]->staves().size()) {
        sys = found_systems[0];
    }

    if (!sys) {
        return nullptr;
    }

    // Restrict searching segment to the correct staff
    pos.set_y(sys->staff_canvas_ypage(old_staff_index));

    Segment *seg = nullptr; // don't prefer any segment while searching line position
    staff_idx_t staff_index = old_staff_index;
    score()->drag_position(pos, &staff_index, &seg, spacing_factor, allow_time_anchor());

    // Lines that don't allow time anchors can anchor to the last segment in the score (see also LineSegment::findNewAnchorSegment)
    if (seg && !allow_time_anchor() && grip == Grip::END) {
        Segment *last = score()->last_segment();
        const bool last_seg_is_closest = std::abs(last->canvas_x() - pos.x()) < std::abs(seg->canvas_x() - pos.x());
        if (last_seg_is_closest) {
            return last;
        }
    }

    return seg;
}

//---------------------------------------------------------
//   deltaRebaseLeft
///   Helper function for anchors rebasing when dragging.
//---------------------------------------------------------

PointF LineSegment::delta_rebase_left(const Segment *old_seg, const Segment *new_seg) {
    if (old_seg == new_seg) {
        return PointF();
    }
    return old_seg->canvas_pos() - new_seg->canvas_pos();
}

//---------------------------------------------------------
//   deltaRebaseRight
///   Helper function for anchors rebasing when dragging.
//---------------------------------------------------------

PointF LineSegment::delta_rebase_right(const Segment *old_seg, const Segment *new_seg) {
    if (old_seg == new_seg) {
        return PointF();
    }

    const PointF old_base = old_seg->canvas_pos();
    const PointF new_base = new_seg->canvas_pos();
    return old_base - new_base;
}

//---------------------------------------------------------
//   lastSegmentEndTick
///   Helper function for anchors rebasing when dragging.
//---------------------------------------------------------

Fraction LineSegment::last_segment_end_tick(const Segment *seg, const Spanner *s) {
    return seg->tick() + seg->ticks_in_staff(track2staff(s->effective_track2()));
}

//---------------------------------------------------------
//   rebaseAnchor
///   If system has changed, returns the new line segment
///   (may appear to be reused \c this segment), otherwise
///   returns nullptr.
//---------------------------------------------------------

LineSegment *LineSegment::rebase_anchor(Grip grip, Segment *new_seg) {
    switch (grip) {
        case Grip::START:
            if (!is_single_begin_type()) {
                return nullptr;
            }
            break;
        case Grip::END:
            if (!is_single_end_type()) {
                return nullptr;
            }
            break;
        default: return nullptr;
    }

    SLine *l = line();
    const bool left = (grip == Grip::START);
    Segment *const old_seg = left ? l->start_segment() : l->end_segment();
    System *const old_line_seg_system = system();
    System *const old_spanner_system = old_seg->measure()->system();

    if (!new_seg || old_seg == new_seg) {
        return nullptr;
    }

    Fraction new_seg_tick = new_seg->tick();
    Fraction start_tick = left ? new_seg_tick : l->tick();
    Fraction end_tick = left ? l->tick2() : (new_seg_tick == start_tick ? new_seg_tick + new_seg->ticks() : new_seg_tick);

    if (end_tick <= start_tick) {
        if (left) {
            end_tick = last_segment_end_tick(new_seg, l);
        } else {
            start_tick = new_seg->tick();
        }
    }

    bool anchor_changed = false;

    System *line_seg_sys = system();
    PointF old_pos = line()->line_pos(grip, &line_seg_sys);

    if (l->tick() != start_tick) {
        l->undo_change_property(Pid::SPANNER_TICK, start_tick);
        anchor_changed = true;
    }

    const Fraction ticks_length = end_tick - start_tick;
    if (ticks_length != l->ticks()) {
        l->undo_change_property(Pid::SPANNER_TICKS, ticks_length);
        anchor_changed = true;
    }

    const Segment *new_tick_seg = left ? l->start_segment() : l->end_segment();
    const System *new_spanner_system = new_tick_seg->measure()->system();

    if ((old_line_seg_system && new_seg->system() != old_line_seg_system) || old_spanner_system != new_spanner_system) {
        renderer()->layout_item(l);
        return l->segments_empty() ? nullptr : left ? l->front_segment() : l->back_segment();
    } else if (anchor_changed) {
        rebase_offsets_on_anchor_changed(grip, old_pos, old_line_seg_system);
    }

    return nullptr;
}

void LineSegment::rebase_offsets_on_anchor_changed(Grip grip, const PointF &old_pos, System *sys) {
    PointF new_pos = line()->line_pos(grip, &sys);
    PointF delta = old_pos - new_pos;
    if (grip == Grip::START) {
        set_offset(offset() + delta);
        offset2_ -= delta;
    } else {
        offset2_ += delta;
    }
}

//---------------------------------------------------------
//   spatiumChanged
//---------------------------------------------------------

void LineSegment::spatium_changed(double ov, double nv) {
    EngravingItem::spatium_changed(ov, nv);
    double scale = nv / ov;
    offset2_ *= scale;
}

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void LineSegment::local_spatium_changed(double ov, double nv) {
    EngravingItem::local_spatium_changed(ov, nv);
    offset2_ *= nv / ov;
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *LineSegment::property_delegate(Pid pid) const {
    if (pid == Pid::DIAGONAL || pid == Pid::COLOR || pid == Pid::LINE_WIDTH || pid == Pid::LINE_STYLE || pid == Pid::DASH_LINE_LEN ||
        pid == Pid::DASH_GAP_LEN) {
        return spanner();
    }
    return SpannerSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   dragAnchorLines
//---------------------------------------------------------

Spatium LineSegment::line_width() const {
    if (!line()) {
        return 0.0_sp;
    }

    return line()->line_width();
}

double LineSegment::absolute_from_spatium(const Spatium &sp) const {
    if (!line()) {
        return EngravingItem::absolute_from_spatium(sp);
    }

    return line()->absolute_from_spatium(sp);
}

//---------------------------------------------------------
//   SLine
//---------------------------------------------------------

SLine::SLine(const ElementType &type, EngravingObject *parent, ElementFlags f) : Spanner(type, parent, f) {
    set_track(0);
    line_color_ = configuration()->default_color();
    line_width_ = 0.15_sp;
}

SLine::SLine(const SLine &s) : Spanner(s) {
    diagonal_ = s.diagonal_;
    line_width_ = s.line_width_;
    line_color_ = s.line_color_;
    line_style_ = s.line_style_;
    dash_line_len_ = s.dash_line_len_;
    dash_gap_len_ = s.dash_gap_len_;
}

PointF SLine::line_pos(Grip grip, System **system) const {
    bool start = grip == Grip::START;
    bool mm_rest = style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS);

    switch (anchor()) {
        case Spanner::Anchor::NOTE: {
            EngravingItem *item = start ? start_element() : end_element();
            Note *note = item && item->is_note() ? to_note(item) : nullptr;
            if (!note) {
                return PointF();
            }
            *system = note->chord()->segment()->measure()->system();
            if (!(*system)) {
                return PointF();
            }
            return note->page_pos() - (*system)->page_pos();
        }
        case Spanner::Anchor::CHORDREST:
        case Spanner::Anchor::SEGMENT: {
            Segment *segment = start ? start_segment() : end_segment();
            if (!segment) {
                return PointF();
            }
            if (anchor() == Spanner::Anchor::CHORDREST && !segment->is_chord_rest_type()) {
                return PointF();
            }
            if (!start) {
                Fraction cur_tick = segment->tick();
                while (true) {
                    Segment *prev_seg = mm_rest ? segment->prev1_mm() : segment->prev1();
                    if (prev_seg && prev_seg->tick() == cur_tick) {
                        segment = prev_seg;
                    } else {
                        break;
                    }
                }
            }
            *system = segment->measure()->system();
            double x = segment->x() + segment->measure()->x() - (start ? 0.0 : spatium());
            return PointF(x, 0.0);
        }
        case Spanner::Anchor::MEASURE: {
            Measure *measure = score()->tick2measure_mm(start ? tick() : tick2());
            if (!measure) {
                return PointF();
            }
            *system = measure->system();
            double x = measure->x();
            return PointF(x, 0.0);
        }
        default: return PointF();
    }
}

//---------------------------------------------------------
//   setLen
//    used to create an element suitable for palette
//---------------------------------------------------------

void SLine::set_len(double l) {
    if (spanner_segments().empty()) {
        add(create_line_segment());
    }
    LineSegment *s = front_segment();
    s->set_pos(PointF());
    s->set_pos2(PointF(l, 0));
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue SLine::get_property(Pid id) const {
    switch (id) {
        case Pid::DIAGONAL: return diagonal_;
        case Pid::COLOR: return PropertyValue::from_value(line_color_);
        case Pid::LINE_WIDTH: return line_width_;
        case Pid::LINE_STYLE: return line_style_;
        case Pid::DASH_LINE_LEN: return dash_line_len();
        case Pid::DASH_GAP_LEN: return dash_gap_len();
        default: return Spanner::get_property(id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool SLine::set_property(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::DIAGONAL: diagonal_ = v.to_bool(); break;
        case Pid::COLOR: set_line_color(v.value<Color>()); break;
        case Pid::LINE_WIDTH:
            if (v.type() == PType::ABSOLUTE) {
                line_width_ = Spatium::from_absolute(v.value<double>(), spatium());
            } else if (v.type() == PType::SPATIUM) {
                line_width_ = v.value<Spatium>();
            }
            break;
        case Pid::LINE_STYLE: line_style_ = v.value<LineType>(); break;
        case Pid::DASH_LINE_LEN: set_dash_line_len(v.to_double()); break;
        case Pid::DASH_GAP_LEN: set_dash_gap_len(v.to_double()); break;
        default: return Spanner::set_property(id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue SLine::property_default(Pid pid) const {
    switch (pid) {
        case Pid::DIAGONAL: return false;
        case Pid::COLOR: return PropertyValue::from_value(configuration()->default_color());
        case Pid::LINE_WIDTH:
            if (property_flags(pid) != PropertyFlags::NOSTYLE) {
                return Spanner::property_default(pid);
            }
            return 0.15_sp;
        case Pid::LINE_STYLE:
            if (property_flags(pid) != PropertyFlags::NOSTYLE) {
                return Spanner::property_default(pid);
            }
            return LineType::SOLID;
        case Pid::DASH_LINE_LEN:
        case Pid::DASH_GAP_LEN:
            if (property_flags(pid) != PropertyFlags::NOSTYLE) {
                return Spanner::property_default(pid);
            }
            return 5.0;
        default: return Spanner::property_default(pid);
    }
}

void SLine::undo_move_start(Fraction tick_diff) {
    Fraction new_tick = tick() + tick_diff;
    if (new_tick >= Fraction(0, 1)) {
        undo_change_property(Pid::SPANNER_TICK, new_tick);
    }
    Fraction new_duration = ticks() - tick_diff;
    if (new_duration > Fraction(0, 1)) {
        undo_change_property(Pid::SPANNER_TICKS, new_duration);
    }
}

void SLine::undo_move_end(Fraction tick_diff) {
    Fraction new_duration = ticks() + tick_diff;
    if (new_duration > Fraction(0, 1)) {
        undo_change_property(Pid::SPANNER_TICKS, new_duration);
    } else {
        Fraction new_tick = tick() + tick_diff;
        if (new_tick >= Fraction(0, 1)) {
            undo_change_property(Pid::SPANNER_TICK, tick() + tick_diff);
        }
    }
}

Note *SLine::guess_final_note(Note *start_note) {
    Chord *chord = start_note->chord();
    if (chord->is_grace_before()) {
        Chord *parent_chord = to_chord(chord->parent());
        GraceNotesGroup &graces_before = parent_chord->grace_notes_before();
        auto position_of_this = std::find(graces_before.begin(), graces_before.end(), chord);
        if (position_of_this != graces_before.end()) {
            auto next_position = ++position_of_this;
            if (next_position != graces_before.end()) {
                return (*next_position)->up_note();
            }
        }
        return parent_chord->up_note();
    } else if (chord->is_grace_after()) {
        Chord *parent_chord = to_chord(chord->parent());
        GraceNotesGroup &graces_after = parent_chord->grace_notes_after();
        auto position_of_this = std::find(graces_after.begin(), graces_after.end(), chord);
        if (position_of_this != graces_after.end()) {
            auto next_position = ++position_of_this;
            if (next_position != graces_after.end()) {
                return (*next_position)->up_note();
            }
        }
        chord = to_chord(chord->parent());
    } else {
        std::vector<Chord *> graces = chord->grace_notes_after();
        if (graces.size() > 0) {
            return graces.front()->up_note();
        }
    }

    if (!chord->ownership_parent()->is_segment()) {
        return 0;
    }

    Segment *segm = chord->score()->tick2right_segment(chord->end_tick());
    while (segm && !segm->is_chord_rest_type()) {
        segm = segm->next1();
    }

    if (!segm) {
        return nullptr;
    }

    track_idx_t chord_track = chord->track();
    Part *part = chord->part();

    Chord *target = nullptr;
    if (segm->element(chord_track) && segm->element(chord_track)->is_chord()) {
        target = to_chord(segm->element(chord_track));
    } else {
        for (EngravingItem *curr_chord : segm->elist()) {
            if (curr_chord && curr_chord->is_chord() && to_chord(curr_chord)->part() == part) {
                target = to_chord(curr_chord);
                break;
            }
        }
    }

    if (target && target->notes().size() > 0) {
        const std::vector<Chord *> &graces = target->grace_notes_before();
        if (graces.size() > 0) {
            return graces.front()->up_note();
        }
        // normal case: try to return the note in the next chord that is in the
        // same position as the start note relative to the end chord
        size_t start_note_idx = muse::index_of(chord->notes(), start_note);
        size_t end_note_idx = std::min(start_note_idx, target->notes().size() - 1);
        return target->notes().at(end_note_idx);
    }

    LOGD("no second note for note anchored line found");
    return nullptr;
}
} // namespace mu::engraving
