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
#include "autoplace.h"

#include "dom/chordrest.h"
#include "dom/harmony.h"
#include "dom/lyrics.h"
#include "dom/measure.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spanner.h"
#include "dom/staff.h"
#include "dom/system.h"
#include "log.h"
#include "style/style.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

void Autoplace::autoplace_segment_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, bool above, bool add) {
    const Segment *segment = to_segment(item->find_ancestor(ElementType::SEGMENT));
    const Measure *measure = segment ? segment->measure() : nullptr;
    const System *system = measure ? measure->system() : nullptr;
    IF_ASSERT_FAILED(system) { return; }
    LD_CONDITION(segment->ldata()->is_set_pos());
    LD_CONDITION(measure->ldata()->is_set_pos());

    const double spatium = item->style().spatium() * item->staff()->staff_mag(item);
    // TODO: proper item-to-item table for horizontal clearance in skyline
    const double min_skyline_horizontal_clearance =
        item->is_articulation_or_fermata() ? 0.0 : item->style().style_absolute(Sid::SKYLINE_MIN_HORIZONTAL_CLEARANCE) * item->mag();

    do_autoplace_element(item, ldata, system, above, add, min_skyline_horizontal_clearance, spatium);
}

void Autoplace::autoplace_measure_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, bool above, bool add) {
    const Measure *measure = to_measure(item->ownership_parent());
    const System *system = measure ? measure->system() : nullptr;
    IF_ASSERT_FAILED(system) { return; }
    LD_CONDITION(measure->ldata()->is_set_pos());

    const double spatium = item->style().spatium() * item->staff()->staff_mag(item);
    // TODO: check this value for measure-owned items
    // const double minSkylineHorizontalClearance = item->isMeasureNumber() ? 0.0 : item->style().styleAbsolute(
    // Sid::skylineMinHorizontalClearance) * item->mag();
    const double min_skyline_horizontal_clearance = 0.0;
    do_autoplace_element(item, ldata, system, above, add, min_skyline_horizontal_clearance, spatium);
}

void Autoplace::autoplace_spanner_segment(const SpannerSegment *item, EngravingItem::LayoutData *ldata) {
    if (item->spanner()->anchor() == Spanner::Anchor::NOTE) {
        return;
    }
    const System *system = item->system();
    IF_ASSERT_FAILED(system) { return; }
    const double mag = !item->system_flag() && !item->spanner()->system_flag() ? item->staff()->staff_mag(item->spanner()->tick()) : 1.0;
    const double spatium = item->style().spatium() * mag;

    const bool above = item->spanner()->place_above();
    do_autoplace_element(item, ldata, system, above, /* add = */ false, /* minSkylineHorizontalClearance = */ 0.0, spatium);
}

double Autoplace::distance_to_move_item(const EngravingItem *item, const SkylineLine &staff_skyline, const Shape &shape, bool above,
                                        double min_skyline_horizontal_clearance, double min_distance) {
    const SkylineLine filtered_skyline = staff_skyline.get_filtered_copy([item](const ShapeElement &shape_el) {
        const EngravingItem *skyline_item = shape_el.item();
        if (!skyline_item) {
            return false;
        }
        return items_should_ignore_each_other(item, skyline_item);
    });

    if (filtered_skyline.elements().empty()) {
        return 0.0;
    }

    const double d = above ? filtered_skyline.min_distance_to_shape_above(shape, min_skyline_horizontal_clearance) :
                             filtered_skyline.min_distance_to_shape_below(shape, min_skyline_horizontal_clearance);

    if (!(d > -min_distance)) {
        return 0.0;
    }

    const double yd = (d + min_distance) * (above ? -1.0 : 1.0);

    return yd;
}

void Autoplace::do_autoplace_element(const EngravingItem *item, EngravingItem::LayoutData *ldata, const System *system, bool above,
                                     bool add, double min_skyline_horizontal_clearance, double spatium) {
    DO_ASSERT(autoplace_applies_to_type(item->type()));
    IF_ASSERT_FAILED(system) { return; }

    if (!item->autoplace() || !item->ownership_parent()) {
        return;
    }

    const staff_idx_t staff_idx = item->effective_staff_idx();
    if (staff_idx == muse::nidx) {
        return;
    }

    LD_CONDITION(ldata->is_set_pos());
    LD_CONDITION(ldata->is_set_bbox());

    const double min_distance = item->min_distance().to_absolute(spatium);

    SysStaff *sys_staff = system->staff(staff_idx);
    SkylineLine &staff_skyline = above ? sys_staff->skyline().north() : sys_staff->skyline().south();
    // TODO [J.M] staffOffset will be applied twice for spanner segments.
    // We really need to sort out staffOffset and have it applied as early and infrequently as possible. Can't keep up with ~30 callsites
    // Shape shape = item->ldata()->shape().translate(item->systemPos() - item->offset() + item->staffOffset());
    Shape shape = item->ldata()->shape();
    if (item->is_spanner_segment()) {
        shape.translate(item->ldata()->pos());
    } else {
        shape.translate(item->system_pos() - item->offset() + item->staff_offset());
    }

    const double yd = distance_to_move_item(item, staff_skyline, shape, above, min_skyline_horizontal_clearance, min_distance);

    ldata->move_y(yd);
    shape.translate_y(yd);

    if (add && item->add_to_skyline()) {
        staff_skyline.add(shape.translated(item->offset()));
    }
}

bool Autoplace::items_should_ignore_each_other(const EngravingItem *item_to_autoplace, const EngravingItem *item_in_skyline) {
    if (item_to_autoplace == item_in_skyline) {
        return true;
    }

    const EngravingItem *skyline_item_owner = item_in_skyline->ownership_parent_item();
    if (item_in_skyline->is_text() && skyline_item_owner && skyline_item_owner->is_s_line_segment()) {
        return items_should_ignore_each_other(item_to_autoplace, skyline_item_owner);
    }

    ElementType type1 = item_to_autoplace->type();
    ElementType type2 = item_in_skyline->type();

    if (type1 == ElementType::TIMESIG) {
        return type2 != ElementType::KEYSIG;
    }

    if (type1 == ElementType::FRET_DIAGRAM && (type2 == ElementType::FRET_DIAGRAM || type2 == ElementType::HARMONY)) {
        bool is_fret_diag_against_its_own_harmony = skyline_item_owner == item_to_autoplace;
        bool are_on_different_segments =
            item_to_autoplace->find_ancestor(ElementType::SEGMENT) != item_in_skyline->find_ancestor(ElementType::SEGMENT);
        return is_fret_diag_against_its_own_harmony || are_on_different_segments;
    }

    if ((type1 == ElementType::DYNAMIC || type1 == ElementType::HAIRPIN_SEGMENT) &&
        (type2 == ElementType::DYNAMIC || type2 == ElementType::HAIRPIN_SEGMENT)) {
        return true;
    }

    if (item_to_autoplace->is_lyrics_line_segment() && item_in_skyline->is_lyrics_line_segment()) {
        const LyricsLineSegment *a = to_lyrics_line_segment(item_to_autoplace);
        const LyricsLineSegment *b = to_lyrics_line_segment(item_in_skyline);
        return a->verse() == b->verse() && a->lyrics_place_above() == b->lyrics_place_above();
    }

    if (type1 == type2) {
        // Items of same type should ignore each other in most cases
        static const std::set<ElementType> text_based_types_which_ignore_each_other{ElementType::DYNAMIC, ElementType::EXPRESSION,
                                                                                    ElementType::STICKING};
        return !item_to_autoplace->is_text_base() || muse::contains(text_based_types_which_ignore_each_other, type1);
    }

    if ((type1 == ElementType::DYNAMIC || type1 == ElementType::EXPRESSION) &&
        (type2 == ElementType::DYNAMIC || type2 == ElementType::EXPRESSION)) {
        // Dynamics and expressions should ignore each other if on the same segment
        return item_to_autoplace->parent() == item_in_skyline->parent();
    }

    if ((type1 == ElementType::TUPLET || type1 == ElementType::STAFF_LINES) &&
        (type2 == ElementType::STAFF_LINES || type2 == ElementType::TUPLET)) {
        const Score *score = item_to_autoplace->score();
        const bool out_of_staff = score ? !score->style().style_b(Sid::TUPLET_OUT_OF_STAFF) : false;
        return out_of_staff;
    }

    if ((type1 == ElementType::FIGURED_BASS || type1 == ElementType::FIGURED_BASS_ITEM) &&
        (type2 == ElementType::FIGURED_BASS || type2 == ElementType::FIGURED_BASS_ITEM)) {
        return true;
    }

    if (item_to_autoplace->is_articulation_or_fermata() && item_in_skyline->is_articulation_or_fermata()) {
        // Ignore fermatas and articulations on other segments
        return item_to_autoplace->find_ancestor(ElementType::SEGMENT) != item_in_skyline->find_ancestor(ElementType::SEGMENT);
    }

    return item_to_autoplace->ldata()->item_snapped_before() == item_in_skyline ||
           item_to_autoplace->ldata()->item_snapped_after() == item_in_skyline;
}
