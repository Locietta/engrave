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

#include "alignmentlayout.h"

#include <set>

#include "dom/dynamic.h"
#include "dom/expression.h"
#include "dom/fret.h"
#include "dom/hairpin.h"
#include "dom/hammeronpulloff.h"
#include "dom/harmony.h"
#include "dom/system.h"
#include "dom/tapping.h"
#include "dom/text.h"
#include "systemlayout.h"

namespace mu::engraving::rendering::score {
void AlignmentLayout::align_items_group(const std::vector<EngravingItem *> &elements, const System *system) {
    if (elements.empty()) {
        return;
    }

    double outermost_y = y_optical_center(elements.front());
    for (const EngravingItem *element : elements) {
        double cur_y = y_optical_center(element);
        outermost_y = is_above(element) ? std::min(outermost_y, cur_y) : std::max(outermost_y, cur_y);
    }

    for (EngravingItem *element : elements) {
        move_item_to_y(element, outermost_y, system);
    }
}

void AlignmentLayout::align_hopo_letters(const HammerOnPullOff *hopo, const System *system) {
    IF_ASSERT_FAILED(!hopo->segments_empty()) { return; }

    std::vector<EngravingItem *> items_to_align;
    for (SpannerSegment *seg : hopo->spanner_segments()) {
        if (seg->system() == system) {
            HammerOnPullOffSegment *hopo_segment = to_hammer_on_pull_off_segment(seg);
            for (HammerOnPullOffText *hopo_text : hopo_segment->hopo_text()) {
                items_to_align.push_back(hopo_text);
            }
            break;
        }
    }

    if (items_to_align.empty()) {
        return;
    }

    bool above = is_above(items_to_align.front());
    for (EngravingItem *item : items_to_align) {
        if (is_above(item) != above) {
            return; // Don't try to align items split above/below
        }
    }

    ChordRest *scr = to_chord_rest(hopo->start_element());
    if (scr && scr->is_chord()) {
        for (Articulation *art : to_chord(scr)->articulations()) {
            if (art->is_tapping() && to_tapping(art)->text() && is_above(art) == above) {
                items_to_align.push_back(art);
            }
        }
    }

    align_items_group(items_to_align, system);
}

void AlignmentLayout::align_items_with_their_snapping_chain(const std::vector<EngravingItem *> &elements, const System *system) {
    std::set<EngravingItem *> aligned_items;

    double outermost_y = 0.0;
    bool first_item = true;
    auto compute_outermost_y = [&outermost_y, &first_item](EngravingItem *item) {
        double cur_y = y_optical_center(item);
        if (first_item) {
            outermost_y = cur_y;
            first_item = false;
        } else {
            outermost_y = item->place_above() ? std::min(outermost_y, cur_y) : std::max(outermost_y, cur_y);
        }
    };

    auto move_elements_to_outermost_y = [&outermost_y, &aligned_items, system](EngravingItem *item) {
        aligned_items.insert(item);
        move_item_to_y(item, outermost_y, system);
    };

    for (EngravingItem *item : elements) {
        if (muse::contains(aligned_items, item)) {
            continue;
        }
        first_item = true;
        item->scan_connected_items_in_snapping_chain(system, compute_outermost_y);
        item->scan_connected_items_in_snapping_chain(system, move_elements_to_outermost_y);
    }
}

void AlignmentLayout::align_items_for_system(const std::vector<EngravingItem *> &elements, const System *system) {
    struct StaffItemGroups {
        std::vector<EngravingItem *> items_above;
        std::vector<EngravingItem *> items_below;
    };

    std::map<staff_idx_t, StaffItemGroups> staff_items;

    for (EngravingItem *item : elements) {
        if (item->add_to_skyline() && !item->exclude_vertical_align()) {
            item->place_above() ? staff_items[item->staff_idx()].items_above.push_back(item) :
                                  staff_items[item->staff_idx()].items_below.push_back(item);
        }
    }

    for (const auto &staff_item : staff_items) {
        AlignmentLayout::align_items_group(staff_item.second.items_above, system);
        AlignmentLayout::align_items_group(staff_item.second.items_below, system);
    }
}

void AlignmentLayout::move_item_to_y(EngravingItem *item, double y, const System *system) {
    double cur_y = y_optical_center(item);
    double y_diff = y - cur_y;
    item->mutldata()->move_y(y_diff);
    SystemLayout::update_skyline_for_element(item, system, y_diff);
}

double AlignmentLayout::y_optical_center(const EngravingItem *item) {
    double cur_y = item->ldata()->pos().y();
    switch (item->type()) {
        case ElementType::DYNAMIC:
        case ElementType::EXPRESSION: {
            // Please keep `Dynamic::gripsPositions` in sync with this calculation
            cur_y += item->staff_offset_y();
            AlignV vert_align = to_text_base(item)->align().vertical;
            double bbox_height = item->ldata()->bbox().height();
            switch (vert_align) {
                case AlignV::TOP: cur_y += 0.5 * bbox_height; break;
                case AlignV::VCENTER: break;
                case AlignV::BOTTOM: cur_y -= 0.5 * bbox_height; break;
                case AlignV::BASELINE:
                    if (item->is_dynamic()) {
                        cur_y -= 0.46 * item->spatium() * to_dynamic(item)->symbol_scale(); // approximated half x-height of dynamic
                    } else {
                        cur_y -= 0.5 * to_expression(item)->font_metrics().x_height();
                    }
                    break;
            }
            break;
        }
        case ElementType::HAIRPIN_SEGMENT: {
            const HairpinSegment *hairpin_seg = to_hairpin_segment(item);
            if (hairpin_seg->hairpin()->is_line_type()) {
                Text *text = hairpin_seg->text();
                if (!text) {
                    text = hairpin_seg->end_text();
                }
                if (text) {
                    cur_y -= 0.5 * text->font_metrics().x_height();
                } else {
                    cur_y -= 0.5 * item->spatium();
                }
            }
            break;
        }
        case ElementType::FRET_DIAGRAM: cur_y += to_fret_diagram(item)->ldata()->grid_height; break;
        case ElementType::HARMONY: {
            EngravingItem *parent = to_harmony(item)->layout_parent();
            if (parent && parent->is_fret_diagram()) {
                cur_y += parent->pos().y();
            }
            break;
        }
        default: break;
    }
    return cur_y;
}

bool AlignmentLayout::is_above(const EngravingItem *item) {
    return item->is_articulation_family() ? to_articulation(item)->ldata()->up() : item->place_above();
}
} // namespace mu::engraving::rendering::score
