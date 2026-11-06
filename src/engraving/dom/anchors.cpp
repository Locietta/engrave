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

#include "anchors.h"

#include <climits>

#include "factory.h"
#include "figuredbass.h"
#include "fret.h"
#include "harmony.h"
#include "measure.h"
#include "page.h"
#include "rendering/score/measurelayout.h"
#include "score.h"
#include "spanner.h"
#include "staff.h"
#include "system.h"
#include "textlinebase.h"
#include "timesig.h"
#include "utils.h"

using namespace mu::engraving::rendering::score;

namespace mu::engraving {
/***************************************
 * EditTimeTickAnchors
 * ************************************/

void EditTimeTickAnchors::update_anchors(const EngravingItem *item) {
    if (!item->allow_time_anchor()) {
        item->score()->hide_anchors();
        return;
    }

    Fraction start_tick_main_region = item->is_spanner_segment() ? to_spanner_segment(item)->spanner()->tick() : item->tick();
    Fraction end_tick_main_region = item->is_spanner_segment() ? to_spanner_segment(item)->spanner()->tick2() : item->tick();

    Score *score = item->score();
    Measure *start_measure = score->tick2measure(start_tick_main_region);
    Measure *end_measure = score->tick2measure(end_tick_main_region);
    if (!start_measure || !end_measure) {
        return;
    }

    staff_idx_t staff = item->staff_idx();
    Measure *start_one_before = start_measure->prev_measure();
    for (MeasureBase *mb = start_one_before ? start_one_before : start_measure; mb && mb->tick() <= end_measure->tick(); mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        update_anchors(to_measure(mb), staff);
    }

    Fraction start_tick_extended_region = start_measure->tick();
    Fraction end_tick_extended_region = end_measure->end_tick();
    voice_idx_t voice_idx = item->has_voice_assignment_properties() && item->get_property(Pid::VOICE_ASSIGNMENT).value<VoiceAssignment>() !=
                                                                           VoiceAssignment::CURRENT_VOICE_ONLY ?
                                ::mu::engraving::voices :
                                item->voice();

    score->set_show_anchors(
        ShowAnchors(voice_idx, staff, start_tick_main_region, end_tick_main_region, start_tick_extended_region, end_tick_extended_region));

    item->trigger_layout();
}

void EditTimeTickAnchors::update_anchors(Measure *measure, staff_idx_t staff_idx, const std::set<Fraction> &additional_anchor_rel_ticks) {
    Fraction start_tick = Fraction(0, 1);
    Fraction end_tick = measure->ticks();

    TimeSig *time_sig = measure->score()->staff(staff_idx)->time_sig(measure->tick());
    Fraction time_sig_frac = time_sig ? time_sig->sig() : measure->timesig();
    Fraction half_division = Fraction(1, 2 * time_sig_frac.denominator());
    if (time_sig) {
        half_division /= time_sig->stretch();
    }

    std::set<Fraction> anchor_ticks{additional_anchor_rel_ticks};
    for (Fraction tick = start_tick; tick <= end_tick; tick += half_division) {
        anchor_ticks.insert(tick);
    }
    for (Segment *seg = measure->first(SegmentType::DURATION); seg; seg = seg->next(SegmentType::DURATION)) {
        anchor_ticks.insert(seg->rtick());
    }

    for (const Fraction &anchor_tick : anchor_ticks) {
        create_time_tick_anchor(measure, anchor_tick, staff_idx);
    }

    update_layout(measure);
}

TimeTickAnchor *EditTimeTickAnchors::create_time_tick_anchor(Measure *measure, Fraction rel_tick, staff_idx_t staff_idx) {
    TimeTickAnchor *return_anchor = nullptr;

    Staff *staff = measure->score()->staff(staff_idx);
    IF_ASSERT_FAILED(staff) { return nullptr; }

    for (EngravingObject *linked_obj : staff->link_list()) {
        IF_ASSERT_FAILED(linked_obj) { continue; }

        Staff *linked_staff = to_staff(linked_obj);
        Measure *linked_measure = linked_obj == staff ? measure : linked_staff->score()->tick2measure_mm(measure->tick());
        if (!linked_measure) {
            continue;
        }

        Segment *segment = linked_measure->get_segment_r(SegmentType::TIME_TICK, rel_tick);
        track_idx_t track = staff2track(linked_staff->idx());
        EngravingItem *element = segment->element(track);
        TimeTickAnchor *anchor = element ? to_time_tick_anchor(element) : nullptr;
        if (!anchor) {
            anchor = Factory::create_time_tick_anchor(segment);
            anchor->set_track(track);
            segment->add(anchor);
        }
        if (linked_measure == measure) {
            return_anchor = anchor;
        }
    }

    return return_anchor;
}

void EditTimeTickAnchors::update_layout(Measure *measure) {
    measure->compute_ticks();

    Score *score = measure->score();
    LayoutContext ctx(score);
    MeasureLayout::layout_time_tick_anchors(measure, ctx);
}

bool MoveElementAnchors::can_anchor_to_end_of_previous(const EngravingItem *element) {
    switch (element->type()) {
        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM:
        case ElementType::REHEARSAL_MARK: return false;
        default: return true;
    }
}

void MoveElementAnchors::check_measure_boundaries_and_move_if_need(EngravingItem *element) {
    Segment *cur_seg = to_segment(element->parent());
    Fraction cur_tick = cur_seg->tick();
    Measure *cur_measure = cur_seg->measure();
    Measure *prev_measure = cur_measure->prev_measure();
    bool anchor_to_end_of_previous = element->get_property(Pid::ANCHOR_TO_END_OF_PREVIOUS).to_bool();
    bool need_move_to_next = cur_tick == cur_measure->end_tick() && !anchor_to_end_of_previous;
    bool need_move_to_previous = cur_seg->rtick().is_zero() && anchor_to_end_of_previous && prev_measure;

    if (!need_move_to_previous && !need_move_to_next) {
        return;
    }

    Segment *new_seg = nullptr;
    if (need_move_to_previous) {
        new_seg = prev_measure->find_segment(SegmentType::TIME_TICK, cur_seg->tick());
        if (!new_seg) {
            TimeTickAnchor *anchor =
                EditTimeTickAnchors::create_time_tick_anchor(prev_measure, cur_tick - prev_measure->tick(), element->staff_idx());
            EditTimeTickAnchors::update_layout(prev_measure);
            new_seg = anchor->segment();
        }
    } else {
        new_seg = cur_seg->next1(SegmentType::CHORD_REST);
    }

    if (new_seg && new_seg->tick() == cur_tick) {
        move_segment(element, new_seg, Fraction(0, 1));
    }
}

Segment *MoveElementAnchors::find_new_anchorable_segment_from_drag(EngravingItem *element, Segment *cur_seg) {
    const System *system = cur_seg->system();
    if (!system) {
        return nullptr;
    }

    const Measure *new_measure = nullptr;
    double x_ref = element->canvas_x() - system->canvas_x();
    for (MeasureBase *mb : system->measures()) {
        if (!mb->is_measure()) {
            continue;
        }
        if (mb->x() + mb->width() > x_ref) {
            new_measure = to_measure(mb);
            break;
        }
    }
    if (!new_measure) {
        return nullptr;
    }

    Segment *new_seg = nullptr;
    drag_position_to_segment(element->canvas_pos(), new_measure, element->staff_idx(), &new_seg, 0.5, true);

    return new_seg;
}

void MoveElementAnchors::move_segment(EngravingItem *element, bool forward) {
    Segment *cur_seg = to_segment(element->ownership_parent());
    Segment *new_seg = get_new_segment(element, cur_seg, forward);

    if (new_seg) {
        move_segment(element, new_seg, new_seg->tick() - cur_seg->tick());
    }
}

Segment *MoveElementAnchors::get_new_segment(EngravingItem *element, Segment *cur_seg, bool forward) {
    switch (element->type()) {
        case ElementType::REHEARSAL_MARK: {
            Measure *measure = cur_seg->measure();
            Measure *new_measure = forward ? measure->next_measure_mm() : measure->prev_measure_mm();
            return new_measure ? new_measure->first(SegmentType::CHORD_REST) : nullptr;
        }
        default: return find_new_anchorable_segment(cur_seg, forward);
    }
}

Segment *MoveElementAnchors::find_new_anchorable_segment(const Segment *cur_seg, bool forward) {
    Segment *new_seg = forward ? cur_seg->next1(SegmentType::DURATION) : cur_seg->prev1(SegmentType::DURATION);

    // Continue until we get to a different tick than where we are
    while (new_seg && new_seg->tick() == cur_seg->tick()) {
        new_seg = forward ? new_seg->next1(SegmentType::DURATION) : new_seg->prev1(SegmentType::DURATION);
    }
    if (!new_seg) {
        return nullptr;
    }

    // Always prefer ChordRest segment if one exists at the same tick
    if (new_seg->segment_type() != SegmentType::CHORD_REST) {
        Segment *new_chord_rest_seg = forward ? new_seg->next1(SegmentType::CHORD_REST) : new_seg->prev1(SegmentType::CHORD_REST);
        if (new_chord_rest_seg && new_chord_rest_seg->tick() == new_seg->tick()) {
            new_seg = new_chord_rest_seg;
        }
    }

    return new_seg;
}

void MoveElementAnchors::move_segment(EngravingItem *element, Segment *new_seg, Fraction tick_diff) {
    switch (element->type()) {
        case ElementType::FIGURED_BASS: do_move_segment(to_figured_bass(element), new_seg, tick_diff); break;
        case ElementType::HARMONY:
        case ElementType::FRET_DIAGRAM: do_move_harmony_or_fret_diagram_segment(element, new_seg, tick_diff); break;
        default: do_move_segment(to_engraving_item(element), new_seg, tick_diff); break;
    }
}

void MoveElementAnchors::do_move_segment(EngravingItem *element, Segment *new_seg, Fraction tick_diff) {
    IF_ASSERT_FAILED(element->parent() && element->parent()->is_segment()) { return; }

    // NOTE: when creating mmRests, we clone text elements from the underlying measure onto the
    // mmRest measure. This creates lots of additional linked copies which are hard to manage
    // and can result in duplicates. Here we need to remove copies on mmRests because they are invalidated
    // when moving segments (and if needed will be recreated at the next layout). In future we need to
    // change approach and *move* elements onto the mmRests, not clone them. [M.S.]

    Score *score = element->score();
    Measure *parent_measure = to_measure(element->find_ancestor(ElementType::MEASURE));
    bool parent_measure_is_mm_rest = parent_measure && parent_measure->is_mm_rest();

    std::list<EngravingObject *> linked_elements = element->link_list();
    for (EngravingObject *linked_element : linked_elements) {
        if (linked_element == element) {
            continue;
        }
        Measure *linked_parent_measure = to_measure(to_engraving_item(linked_element)->find_ancestor(ElementType::MEASURE));
        bool linked_parent_measure_is_m_mrest = linked_parent_measure && linked_parent_measure->is_mm_rest();
        if (linked_parent_measure_is_m_mrest != parent_measure_is_mm_rest) {
            for (EngravingObject *child : linked_element->children()) {
                // e.g. fret diagram should also remove the harmony
                child->undo_unlink();
                score->undo_remove_element(to_engraving_item(child));
            }
            linked_element->undo_unlink();
            score->undo_remove_element(static_cast<EngravingItem *>(linked_element));
        }
    }

    score->undo_change_parent(element, new_seg, element->staff_idx());
    move_snapped_items(element, new_seg, tick_diff);
}

void MoveElementAnchors::do_move_segment(FiguredBass *element, Segment *new_seg, Fraction tick_diff) {
    IF_ASSERT_FAILED(element->parent() && element->parent()->is_segment()) { return; }

    Segment *old_seg = element->segment();

    do_move_segment(to_engraving_item(element), new_seg, tick_diff);

    track_idx_t start_track = staff2track(element->staff_idx());
    track_idx_t end_track = start_track + ::mu::engraving::voices;

    // Shorten this if needed
    if (new_seg->tick() > old_seg->tick()) {
        FiguredBass *next_fb = nullptr;
        Fraction end_tick = new_seg->tick() + element->ticks();
        for (Segment *seg = new_seg->next1(SegmentType::DURATION); seg && seg->tick() <= end_tick;
             seg = seg->next1(SegmentType::DURATION)) {
            next_fb = to_figured_bass(seg->find_annotation(ElementType::FIGURED_BASS, start_track, end_track));
            if (next_fb) {
                break;
            }
        }
        if (next_fb) {
            element->set_ticks(next_fb->tick() - new_seg->tick());
        }
    }

    // Shorten previous if needed
    if (new_seg->tick() < old_seg->tick()) {
        FiguredBass *prev_fb = nullptr;
        for (Segment *seg = new_seg->prev1(SegmentType::DURATION); seg && seg->measure()->is_after_or_equal(new_seg->measure());
             seg = seg->prev1(SegmentType::DURATION)) {
            prev_fb = (FiguredBass *) (seg->find_annotation(ElementType::FIGURED_BASS, start_track, end_track));
            if (prev_fb) {
                break;
            }
        }
        if (prev_fb) {
            prev_fb->set_ticks(std::min(prev_fb->ticks(), new_seg->tick() - prev_fb->tick()));
        }
    }
}

void MoveElementAnchors::do_move_harmony_or_fret_diagram_segment(EngravingItem *element, Segment *new_seg, Fraction tick_diff) {
    for (EngravingObject *item : element->link_list()) {
        Score *score = item->score();
        Measure *measure = score->tick2measure(new_seg->tick());
        Segment *chord_rest_seg = measure->undo_get_segment(SegmentType::CHORD_REST, new_seg->tick());
        if (item == element) {
            new_seg = chord_rest_seg;
        }
    }

    Fraction old_tick = element->tick();

    do_move_segment(to_engraving_item(element), new_seg, tick_diff);

    for (EngravingObject *item : element->link_list()) {
        Score *score = item->score();
        Segment *old_segment = score->tick2segment(old_tick, true, SegmentType::CHORD_REST, false);
        if (old_segment) {
            old_segment->check_empty();
            if (old_segment->empty()) {
                score->undo_remove_element(old_segment);
            }
        }
    }
}

void MoveElementAnchors::move_snapped_items(EngravingItem *element, Segment *new_seg, Fraction tick_diff) {
    if (EngravingItem *item_after = element->mutldata()->item_snapped_after()) {
        if (item_after->is_text_base() && item_after->parent() != new_seg) {
            element->score()->undo_change_parent(item_after, new_seg, item_after->staff_idx());
            move_snapped_items(item_after, new_seg, tick_diff);
        } else if (item_after->is_text_line_base_segment()) {
            TextLineBase *text_line = ((TextLineBaseSegment *) item_after)->text_line_base();
            if (text_line->tick() != new_seg->tick()) {
                text_line->undo_move_start(tick_diff);
            }
        }
    }

    if (EngravingItem *item_before = element->mutldata()->item_snapped_before()) {
        if (item_before->is_text_base() && item_before->parent() != new_seg) {
            element->score()->undo_change_parent(item_before, new_seg, item_before->staff_idx());
            move_snapped_items(item_before, new_seg, tick_diff);
        } else if (item_before->is_text_line_base_segment()) {
            TextLineBase *text_line = ((TextLineBaseSegment *) item_before)->text_line_base();
            if (text_line->tick2() != new_seg->tick()) {
                text_line->undo_move_end(tick_diff);
            }
        }
    }
}

void MoveElementAnchors::rebase_offset_on_move_segment(EngravingItem *element, const PointF &cur_offset, Segment *new_seg,
                                                       Segment *old_seg) {
    PointF offset_shift = new_seg->page_pos() - old_seg->page_pos();
    element->set_offset(cur_offset - offset_shift);

    if (EngravingItem *item_before = element->mutldata()->item_snapped_before()) {
        if (item_before->is_text_base()) {
            item_before->set_offset(item_before->offset() - offset_shift);
        }
    }

    if (EngravingItem *item_after = element->mutldata()->item_snapped_after()) {
        if (item_after->is_text_base()) {
            item_after->set_offset(item_after->offset() - offset_shift);
        }
    }
}

/********************************************
 * TimeTickAnchor
 * *****************************************/

TimeTickAnchor::TimeTickAnchor(DummyParentOr<Segment> parent)
    : EngravingItem(ElementType::TIME_TICK_ANCHOR, parent, ElementFlag::ON_STAFF | ElementFlag::NOT_SELECTABLE | ElementFlag::GENERATED) {
    set_z(-INT_MAX); // Make sure it is behind everything
}

TimeTickAnchor::DrawRegion TimeTickAnchor::draw_region() const {
    const ShowAnchors &show_anchors = score()->show_anchors();
    const staff_idx_t this_staff_idx = staff_idx();
    const Fraction this_tick = segment()->tick();

    const bool track_out_of_range = this_staff_idx < show_anchors.staff_idx || this_staff_idx >= show_anchors.end_staff_idx;
    const bool tick_out_of_range =
        this_tick < show_anchors.start_tick_extended_region || this_tick >= show_anchors.end_tick_extended_region;
    if (track_out_of_range || tick_out_of_range) {
        return DrawRegion::OUT_OF_RANGE;
    }

    if (this_tick < show_anchors.start_tick_main_region || this_tick >= show_anchors.end_tick_main_region) {
        return DrawRegion::EXTENDED_REGION;
    }

    return DrawRegion::MAIN_REGION;
}

voice_idx_t TimeTickAnchor::voice_idx() const { return score()->show_anchors().voice_idx; }
} // namespace mu::engraving
