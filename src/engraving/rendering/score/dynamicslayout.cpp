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

#include "dynamicslayout.h"

#include "../dom/hairpin.h"
#include "../dom/measure.h"
#include "../dom/staff.h"
#include "../dom/system.h"
#include "../types/typesconv.h"
#include "layoutcontext.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace mu::engraving::rendering::score;

void DynamicsLayout::layout_dynamic(Dynamic *item, TextBase::LayoutData *ldata, const LayoutConfiguration &conf) {
    do_layout_dynamic(item, ldata, conf);

    // If the dynamic contains custom text, keep it aligned
    double custom_text_offset = compute_custom_text_offset(item, ldata, conf);
    ldata->move_x(-custom_text_offset);

    if (item->autoplace() && item->avoid_bar_lines()) {
        manage_barline_collisions(item, ldata);
    }
}

void DynamicsLayout::do_layout_dynamic(Dynamic *item, Dynamic::LayoutData *ldata, const LayoutConfiguration &conf) {
    ldata->disconnect_snapped_items();

    HairpinSegment *snap_before_hairpin_across_sys_break = item->find_snap_before_hairpin_across_system_break();
    if (snap_before_hairpin_across_sys_break) {
        ldata->connect_item_snapped_before(snap_before_hairpin_across_sys_break);
    }

    const StaffType *st_type = item->staff_type();
    if (st_type && st_type->is_hidden_element_on_tab(Sid::DYNAMICS_SHOW_TAB_COMMON, Sid::DYNAMICS_SHOW_TAB_SIMPLE)) {
        ldata->set_is_skip_draw(true);
        return;
    }
    ldata->set_is_skip_draw(false);

    item->set_placement_based_on_voice_assignment(conf.style_v(Sid::DYNAMICS_HAIRPIN_VOICE_BASED_PLACEMENT).value<DirectionV>());

    // If "Center on notehead" is on, override user position. Restore later
    AlignH user_position = item->get_property(Pid::POSITION).value<AlignH>();
    AlignH h_pos = item->center_on_notehead() ? AlignH::HCENTER : item->position();
    item->set_position(h_pos);
    TextLayout::layout_base_text_base(item, ldata);
    item->set_position(user_position);

    const Segment *s = item->segment();
    if (!s || (!item->center_on_notehead() && item->position() == AlignH::LEFT)) {
        return;
    }

    if (item->anchor_to_end_of_previous()) {
        layout_dynamic_to_end_of_previous(item, ldata);
        return;
    }

    if (!item->center_on_notehead()) {
        return;
    }

    // Use SMuFL optical center for dynamic if available
    SymId sym_id = TConv::sym_id(item->dynamic_type());
    double optical_center = item->sym_smufl_anchor(sym_id, SmuflAnchorId::OPTICAL_CENTER).x();
    if (sym_id != SymId::NO_SYM && optical_center) {
        double sym_width = item->sym_bbox(sym_id).width();
        double offset = sym_width / 2 - optical_center + item->sym_bbox(sym_id).left();
        double spatium_scaling = item->spatium() / conf.spatium();
        offset *= spatium_scaling;
        ldata->move_x(offset);
    }
}

double DynamicsLayout::compute_custom_text_offset(Dynamic *item, Dynamic::LayoutData *ldata, const LayoutConfiguration &conf) {
    if (!item->center_on_notehead() || item->dynamic_type() == DynamicType::OTHER) {
        return 0.0;
    }

    String reference_string = String::from_utf8(Dynamic::dyn_info(item->dynamic_type()).text);
    if (item->xml_text() == reference_string) {
        return 0.0;
    }

    Dynamic reference_dynamic(*item);
    reference_dynamic.set_xml_text(reference_string);
    do_layout_dynamic(&reference_dynamic, reference_dynamic.mutldata(), conf);

    TextFragment reference_fragment;
    if (!reference_dynamic.ldata()->blocks.empty()) {
        TextBlock reference_block = reference_dynamic.ldata()->blocks.front();
        if (!reference_block.fragments().empty()) {
            reference_fragment = reference_dynamic.ldata()->blocks.front().fragments().front();
        }
    }

    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &fragment : block.fragments()) {
            if (fragment.text == reference_fragment.text) {
                return fragment.pos.x() - reference_fragment.pos.x();
            }
        }
    }

    return 0.0;
}

void DynamicsLayout::layout_dynamic_to_end_of_previous(const Dynamic *item, TextBase::LayoutData *ldata) {
    Segment *cur_segment = item->segment();
    Segment *left_most_segment = cur_segment;
    Segment *prev_seg = cur_segment;
    while (true) {
        prev_seg = prev_seg->prev1enabled();
        if (!prev_seg || prev_seg->tick() != cur_segment->tick()) {
            break;
        }
        if (prev_seg->is_active() && prev_seg->has_elements(item->staff_idx())) {
            left_most_segment = prev_seg;
            break;
        }
    }

    double x_diff = cur_segment->x() + cur_segment->measure()->x() - (left_most_segment->x() + left_most_segment->measure()->x());
    ldata->set_pos_x(-x_diff - ldata->bbox().right() - 0.50 * item->spatium());
}

void DynamicsLayout::manage_barline_collisions(const Dynamic *item, TextBase::LayoutData *ldata) {
    if (item->score()->nstaves() <= 1 || item->anchor_to_end_of_previous()) {
        return;
    }

    Segment *this_segment = item->segment();
    if (!this_segment) {
        return;
    }

    System *system = this_segment->measure()->system();
    if (!system) {
        return;
    }

    staff_idx_t staff_idx = item->staff_idx();
    staff_idx_t bar_line_staff = muse::nidx;
    if (item->place_above()) {
        // need to find the barline from the staff above
        // taking into account there could be invisible staves
        if (staff_idx == 0) {
            return;
        }
        for (int staff_index = static_cast<int>(staff_idx) - 1; staff_index >= 0; --staff_index) {
            if (system->staff(staff_index)->show()) {
                bar_line_staff = staff_index;
                break;
            }
        }
    } else {
        bar_line_staff = staff_idx;
    }

    if (bar_line_staff == muse::nidx) {
        return;
    }

    if (!item->score()->staff(bar_line_staff)->bar_line_span()) {
        return; // Barline doesn't extend through staves
    }

    const double min_bar_line_distance = 0.25 * item->spatium();

    RectF reference_b_box = ldata->bbox();

    // Check barlines to the right
    Segment *right_bar_line_segment = nullptr;
    for (Segment *segment = this_segment; segment && segment->measure()->system() == system; segment = segment->next1enabled()) {
        if (segment->segment_type() & SegmentType::BAR_LINE_TYPES) {
            right_bar_line_segment = segment;
            break;
        }
    }

    if (right_bar_line_segment) {
        EngravingItem *e = right_bar_line_segment->element(bar_line_staff * ::mu::engraving::voices);
        if (e) {
            double right_margin = e->ldata()->bbox().translated(e->page_pos()).left() -
                                  reference_b_box.translated(item->page_pos() - item->offset()).right() - min_bar_line_distance;
            if (right_margin < 0) {
                ldata->move_x(right_margin);
            }
        }
    }

    // Check barlines to the left
    Segment *left_bar_line_segment = nullptr;
    for (Segment *segment = this_segment; segment && segment->measure()->system() == system; segment = segment->prev1enabled()) {
        if (segment->segment_type() & SegmentType::BAR_LINE_TYPES) {
            left_bar_line_segment = segment;
            break;
        }
    }
    if (left_bar_line_segment) {
        EngravingItem *e = left_bar_line_segment->element(bar_line_staff * ::mu::engraving::voices);
        if (e) {
            double left_margin = reference_b_box.translated(item->page_pos() - item->offset()).left() -
                                 e->ldata()->bbox().translated(e->page_pos()).right() - min_bar_line_distance;
            if (left_margin < 0) {
                ldata->move_x(-left_margin);
            }
        }
    }
}
