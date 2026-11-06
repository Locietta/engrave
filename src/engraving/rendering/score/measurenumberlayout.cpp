/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#include "measurenumberlayout.h"

#include "dom/measure.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "measurelayout.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void MeasureNumberLayout::layout_measure_number(MeasureNumber *item, MeasureNumber::LayoutData *ldata, const LayoutContext &ctx) {
    MeasureNumberPlacement placement_mode = ctx.conf().style_v(Sid::MEASURE_NUMBER_PLACEMENT_MODE).value<MeasureNumberPlacement>();
    if (placement_mode != MeasureNumberPlacement::ON_ALL_STAVES) {
        item->set_placement(PlacementV::ABOVE);
    }

    layout_measure_number_base(item, ldata);

    const RectF &item_b_box = ldata->bbox();

    bool align_to_barline = ctx.conf().style_b(Sid::MEASURE_NUMBER_ALIGN_TO_BARLINE);
    AlignH h_placement = ctx.conf().style_v(Sid::MEASURE_NUMBER_H_PLACEMENT).value<AlignH>();
    const Segment *barline_seg = ref_barline_segment(item, align_to_barline, h_placement);
    const BarLine *ref_barline = barline_seg ? to_bar_line(barline_seg->element(item->track())) : nullptr;

    const Measure *measure = item->measure();

    if (align_to_barline) {
        if (ref_barline) {
            double x_ref = ref_barline->page_x() - measure->page_x();
            x_ref += (h_placement == AlignH::RIGHT ? 1.0 : h_placement == AlignH::HCENTER ? 0.5 : 0.0) * ref_barline->width();
            ldata->set_pos_x(x_ref);
        } else {
            ldata->set_pos_x(0.0);
        }
    } else {
        Segment *first_cr_seg = measure->first(SegmentType::CHORD_REST);
        MeasureLayout::MeasureStartEndPos measure_start_end_pos =
            MeasureLayout::get_measure_start_end_pos(measure, first_cr_seg, item->staff_idx(), false, false, ctx);

        if (h_placement == AlignH::LEFT) {
            if (measure->header()) {
                ldata->set_pos_x(measure->first_note_rest_segment_x(true) - measure->system_pos().x());
            } else {
                double x_ref = ref_barline ? ref_barline->page_x() - measure->page_x() : 0.0;
                ldata->set_pos_x(x_ref);
            }
        } else if (h_placement == AlignH::HCENTER) {
            ldata->set_pos_x(0.5 * (measure_start_end_pos.x1 + measure_start_end_pos.x2));
        } else {
            if (ref_barline) {
                double x_ref = ref_barline->page_x() - measure->page_x() + ref_barline->width();
                ldata->set_pos_x(x_ref);
            } else {
                ldata->set_pos_x(measure_start_end_pos.x2);
            }
        }
    }

    if (h_placement == AlignH::LEFT) {
        ldata->move_x(-item_b_box.left());
    } else if (h_placement == AlignH::HCENTER) {
        ldata->move_x(-0.5 * (item_b_box.right() + item_b_box.left()));
    } else {
        ldata->move_x(-item_b_box.right());
    }

    Staff *staff = item->staff();
    PointF default_pos = item->default_pos() * (staff ? staff->staff_mag(item->tick()) : 1.0);

    ldata->move(default_pos);

    check_barline_collisions(item, barline_seg, h_placement, ldata);
}

void MeasureNumberLayout::layout_mm_rest_range(MMRestRange *item, MMRestRange::LayoutData *ldata, const LayoutContext &ctx) {
    layout_measure_number_base(item, ldata);

    const Measure *measure = item->measure();
    const Segment *cr_seg = measure->first(SegmentType::CHORD_REST);
    const MeasureLayout::MeasureStartEndPos measure_start_end =
        MeasureLayout::get_measure_start_end_pos(measure, cr_seg, item->staff_idx(), true, false, ctx);

    const RectF &item_b_box = ldata->bbox();
    AlignH h_placement = ctx.conf().style_v(Sid::MM_REST_RANGE_H_PLACEMENT).value<AlignH>();
    if (h_placement == AlignH::HCENTER) {
        const double x1 = measure_start_end.x1;
        const double x2 = measure_start_end.x2;
        ldata->set_pos_x(0.5 * (x1 + x2) - 0.5 * (item_b_box.right() + item_b_box.left()));
    } else if (h_placement == AlignH::RIGHT) {
        ldata->set_pos_x(item->measure()->ldata()->bbox().width() - item_b_box.right());
    } else {
        ldata->set_pos_x(measure_start_end.x1 - item_b_box.left());
    }

    Staff *staff = item->staff();
    PointF default_pos = item->default_pos() * (staff ? staff->staff_mag(item->tick()) : 1.0);

    ldata->move(default_pos);
}

void MeasureNumberLayout::layout_measure_number_base(MeasureNumberBase *item, MeasureNumberBase::LayoutData *ldata) {
    ldata->set_pos(PointF());

    TextLayout::layout_base_text_base1(item, ldata);

    staff_idx_t effective_staff_idx = item->effective_staff_idx();
    Staff *staff = item->score()->staff(effective_staff_idx);

    if (item->place_below()) {
        double yoff = ldata->bbox().height();

        // If there is only one line, the barline spans outside the staff lines, so the default position is not correct.
        if (staff && staff->lines(item->tick()) == 1) {
            yoff += 2.0 * item->spatium();
        } else {
            yoff += item->staff() ? item->staff()->staff_height() : 0.0;
        }

        ldata->set_pos_y(yoff);
    } else {
        double yoff = 0.0;

        // If there is only one line, the barline spans outside the staff lines, so the default position is not correct.
        if (staff && staff->lines(item->tick()) == 1) {
            yoff -= 2.0 * item->spatium();
        }

        ldata->set_pos_y(yoff);
    }
}

const Segment *MeasureNumberLayout::ref_barline_segment(const MeasureNumber *item, bool align_to_barline, AlignH h_placement) {
    Segment *barline_seg = nullptr;
    Measure *measure = item->measure();
    if (align_to_barline || h_placement == AlignH::LEFT) {
        for (Segment *seg = measure->first(); seg && seg->tick() == measure->tick() && seg->system() == measure->system();
             seg = seg->prev1_m_menabled()) {
            if (seg->is_type(SegmentType::BAR_LINE_TYPES) && seg->is_active()) {
                barline_seg = seg;
                break;
            }
        }
    } else {
        for (Segment *seg = measure->first(SegmentType::CHORD_REST);
             seg && seg->tick() <= measure->end_tick() && seg->system() == measure->system(); seg = seg->next1enabled()) {
            if (seg->is_type(SegmentType::BAR_LINE_TYPES) && seg->is_active()) {
                barline_seg = seg;
                break;
            }
        }
    }

    return barline_seg;
}

void MeasureNumberLayout::check_barline_collisions(const MeasureNumber *item, const Segment *barline_seg, AlignH h_placement,
                                                   MeasureNumber::LayoutData *ldata) {
    if (h_placement == AlignH::HCENTER || !item->autoplace()) {
        return;
    }

    const RectF &item_b_box = ldata->bbox();
    staff_idx_t staff_idx = item->staff_idx();
    staff_idx_t barline_staff = muse::nidx;
    System *system = item->measure()->system();
    if (item->get_property(Pid::PLACEMENT).value<PlacementV>() == PlacementV::ABOVE) {
        barline_staff = system->prev_visible_staff(staff_idx);
    } else {
        barline_staff = staff_idx;
    }
    if (barline_staff == muse::nidx) {
        return;
    }

    const double min_bar_line_distance = 0.25 * item->spatium();
    if (!barline_seg ||
        (barline_seg->segment_type() != SegmentType::BEGIN_BAR_LINE && !item->score()->staff(barline_staff)->bar_line_span())) {
        return;
    }

    BarLine *barline = to_bar_line(barline_seg->element(staff2track(barline_staff)));
    if (!barline) {
        return;
    }

    double x_cur = item->page_x();
    if (h_placement == AlignH::LEFT) {
        double x_min = barline->page_x() + barline->width() - item_b_box.left() + min_bar_line_distance;
        if (x_min > x_cur) {
            ldata->move_x(x_min - x_cur);
        }
    } else {
        double x_max = barline->page_x() - item_b_box.right() - min_bar_line_distance;
        if (x_max < x_cur) {
            ldata->move_x(x_max - x_cur);
        }
    }
}
