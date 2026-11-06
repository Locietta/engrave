/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
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

#include "spannerfilter.h"

#include "dom/spanner.h"
#include "dom/pedal.h"
#include "dom/segment.h"

using namespace mu;
using namespace mu::engraving;

bool SpannerFilter::is_playable(const EngravingItem *item, const RenderingContext &ctx) {
    IF_ASSERT_FAILED(item->is_spanner()) { return false; }

    const Spanner *spanner = to_spanner(item);

    int spanner_from = spanner->tick().ticks();
    int spanner_duration_ticks = spanner_actual_duration_ticks(spanner, spanner->ticks().ticks());
    int spanner_to = spanner_from + spanner_duration_ticks;

    if (spanner_duration_ticks == 0 || spanner_to <= ctx.nominal_position_start_tick || spanner_from >= ctx.nominal_position_end_tick) {
        return false;
    }

    return true;
}

int SpannerFilter::spanner_actual_duration_ticks(const Spanner *spanner, const int nominal_duration_ticks) {
    const ElementType type = spanner->type();

    if (type == ElementType::SLUR || type == ElementType::HAMMER_ON_PULL_OFF || type == ElementType::TRILL) {
        const EngravingItem *start_item = spanner->start_element();
        const EngravingItem *end_item = spanner->end_element();

        if (!start_item || !end_item) {
            return nominal_duration_ticks;
        }

        if (start_item->is_chord_rest() && end_item->is_chord_rest()) {
            const ChordRest *start_chord = to_chord_rest(start_item);
            const ChordRest *end_chord = to_chord_rest(end_item);
            return end_chord->tick().ticks() + end_chord->ticks().ticks() - start_chord->tick().ticks();
        }
    }

    return nominal_duration_ticks;
}

bool SpannerFilter::is_multi_staff_spanner(const Spanner *spanner) {
    static const ElementTypeSet multi_staff_spanners = {ElementType::PEDAL, ElementType::PEDAL_SEGMENT};

    return multi_staff_spanners.find(spanner->type()) != multi_staff_spanners.cend();
}
