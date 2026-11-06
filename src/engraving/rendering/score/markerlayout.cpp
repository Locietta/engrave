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

#include "markerlayout.h"

#include "../dom/barline.h"
#include "../dom/marker.h"
#include "../dom/measure.h"
#include "autoplace.h"
#include "layoutcontext.h"
#include "log.h"
#include "textlayout.h"

using namespace mu::engraving::rendering::score;

void MarkerLayout::layout_marker(Marker *item, TextBase::LayoutData *ldata, LayoutContext &ctx) {
    do_layout_marker(item, ldata, ctx);

    double custom_text_offset = compute_custom_text_offset(item, ldata, ctx);
    ldata->move_x(-custom_text_offset);

    Measure *measure = item->measure();
    if (measure && item->autoplace()) {
        LD_CONDITION(ldata->is_set_pos());
        LD_CONDITION(ldata->is_set_bbox());
        LD_CONDITION(measure->ldata()->is_set_pos());
    }

    Autoplace::autoplace_measure_element(item, ldata);
}

void MarkerLayout::do_layout_marker(Marker *item, TextBase::LayoutData *ldata, LayoutContext &ctx) {
    Measure *measure = item->measure();
    IF_ASSERT_FAILED(measure) { LOGD() << "Marker has no measure"; }
    LD_CONDITION(measure->ldata()->is_set_bbox());

    // If "Center on symbol" is on, override user position. Restore later
    AlignH user_position = item->get_property(Pid::POSITION).value<AlignH>();
    AlignH h_pos =
        item->center_on_symbol() && !item->symbol_string().empty() ? AlignH::HCENTER : item->get_property(Pid::POSITION).value<AlignH>();
    item->set_position(h_pos);

    TextLayout::layout_base_text_base(item, ldata);

    // Adjust for barline
    bool right_marker = item->is_right_marker();
    double x_adj = 0.0;
    double bl_width = 0.0;
    if (right_marker) {
        x_adj = measure ? measure->width() : 0.0;
    }

    bool start_repeat = right_marker ? measure->next_measure() && measure->next_measure()->repeat_start() : measure->repeat_start();
    bool end_repeat = right_marker ? measure->repeat_end() : measure->prev_measure() && measure->prev_measure()->repeat_end();

    bool avoid_barline = item->staff_idx() != 0 && h_pos != AlignH::HCENTER;

    if (!avoid_barline) {
        if (start_repeat) {
            bl_width = item->absolute_from_spatium(-ctx.conf().style_s(Sid::END_BAR_WIDTH));
        } else if (end_repeat) {
            bl_width = item->absolute_from_spatium(ctx.conf().style_s(Sid::END_BAR_WIDTH));
        } else if ((measure->is_first_in_system() || (measure->prev() && !measure->prev()->is_measure())) && !right_marker) {
            // Start of score
            const BarLine *bl = measure->start_bar_line();
            bl_width = bl ? -bl->width() : 0.0;
        } else {
            Measure *bl_measure = right_marker ? measure : measure->prev_measure();
            const BarLine *bl = bl_measure ? bl_measure->end_bar_line() : nullptr;
            bl_width = bl ? bl->width() : 0.0;
        }

        switch (h_pos) {
            case AlignH::LEFT:
            case AlignH::JUSTIFY: x_adj -= start_repeat ? 0.0 : bl_width; break;
            case AlignH::HCENTER: x_adj -= +bl_width / 2; break;
            case AlignH::RIGHT: x_adj -= (start_repeat ? bl_width : 0.0); break;
        }
    } else {
        staff_idx_t bl_idx = item->staff_idx() - 1;

        const BarLine *bl = nullptr;
        if (start_repeat) {
            Measure *bl_measure = right_marker ? measure->next_measure() : measure;
            bl = bl_measure ? bl_measure->start_bar_line(bl_idx) : nullptr;
        } else {
            Measure *bl_measure = right_marker ? measure : measure->prev_measure();
            bl = bl_measure ? bl_measure->end_bar_line(bl_idx) : nullptr;
        }

        double bl_padding = 0.0;
        if (avoid_barline && bl) {
            const double font_size_scale_factor = item->size() / 10.0;
            bl_padding = 0.5 * item->spatium() * font_size_scale_factor;
        }

        if (h_pos == AlignH::LEFT) {
            bl_width = start_repeat ? bl->width() : 0.0;
            x_adj += bl_padding + bl_width;
        } else if (h_pos == AlignH::RIGHT) {
            bl_width = start_repeat ? 0.0 : bl->width();
            x_adj -= bl_width + bl_padding;
        }
    }

    item->set_position(user_position);
    ldata->move_x(x_adj);
}

double MarkerLayout::compute_custom_text_offset(const Marker *item, TextBase::LayoutData *ldata, LayoutContext &ctx) {
    String reference_string = item->symbol_string();

    bool center_on_symbol = item->center_on_symbol() && !reference_string.empty();
    if (!center_on_symbol) {
        return 0.0;
    }

    if (item->xml_text() == reference_string) {
        return 0.0;
    }

    Marker reference_marker(*item);
    reference_marker.set_xml_text(reference_string);
    do_layout_marker(&reference_marker, reference_marker.mutldata(), ctx);

    TextFragment reference_fragment;
    if (!reference_marker.ldata()->blocks.empty()) {
        TextBlock reference_block = reference_marker.ldata()->blocks.front();
        if (!reference_block.fragments().empty()) {
            reference_fragment = reference_marker.ldata()->blocks.front().fragments().front();
        }
    }

    for (const TextBlock &block : ldata->blocks) {
        for (const TextFragment &fragment : block.fragments()) {
            if (fragment.text == reference_fragment.text) {
                return ldata->pos().x() + fragment.pos.x() - reference_marker.ldata()->pos().x() - reference_fragment.pos.x();
            }
        }
    }

    return 0.0;
}
