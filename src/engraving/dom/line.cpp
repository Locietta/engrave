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

LineSegment::LineSegment(const LineSegment &s) : LineSegment(s.type(), s) {}

LineSegment::LineSegment(ElementType type, const LineSegment &s) : SpannerSegment(type, s) {}

void LineSegment::spatium_changed(double ov, double nv) {
    EngravingItem::spatium_changed(ov, nv);
    double scale = nv / ov;
    offset2_ *= scale;
}

void LineSegment::local_spatium_changed(double ov, double nv) {
    EngravingItem::local_spatium_changed(ov, nv);
    offset2_ *= nv / ov;
}

EngravingObject *LineSegment::property_delegate(Pid pid) const {
    if (pid == Pid::DIAGONAL || pid == Pid::COLOR || pid == Pid::LINE_WIDTH || pid == Pid::LINE_STYLE || pid == Pid::DASH_LINE_LEN ||
        pid == Pid::DASH_GAP_LEN) {
        return spanner();
    }
    return SpannerSegment::property_delegate(pid);
}

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

SLine::SLine(const ElementType &type, EngravingObject *parent, ElementFlags f) : Spanner(type, parent, f) {
    set_track(0);
    line_color_ = configuration()->default_color();
    line_width_ = 0.15_sp;
}

SLine::SLine(const SLine &s) : SLine(s.type(), s) {}

SLine::SLine(ElementType type, const SLine &s) : Spanner(type, s) {
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
                    Segment *prev_seg = mm_rest ? segment->rendered_prev1_mm() : segment->rendered_prev1();
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

//    used to create an element suitable for palette

void SLine::set_len(double l) {
    if (spanner_segments().empty()) {
        add(create_line_segment());
    }
    LineSegment *s = to_line_segment(mutldata()->segments.front());
    s->set_pos(PointF());
    s->set_pos2(PointF(l, 0));
}

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
        const auto graces_before = parent_chord->grace_notes_before();
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
        const auto graces_after = parent_chord->grace_notes_after();
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
        const auto graces = target->grace_notes_before();
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
