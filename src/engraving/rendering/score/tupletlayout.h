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
#ifndef MU_ENGRAVING_TUPLETLAYOUT_DEV_H
#define MU_ENGRAVING_TUPLETLAYOUT_DEV_H

#include "layoutcontext.h"

namespace mu::engraving {
class ChordRest;
class DurationElement;
class Tuplet;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
class TupletLayout {
public:
    static void layout(Tuplet *item, LayoutContext &ctx);

    static void layout_tuplet_and_nested_tuplets(Tuplet *t, LayoutContext &ctx);
    static bool is_top_tuplet(ChordRest *cr);
    static bool not_top_tuplet(ChordRest *cr);

private:
    static void create_number(Tuplet *item, LayoutContext &ctx);
    static void compute_direction(Tuplet *item);
    static void compute_start_end_cr(Tuplet *item, const ChordRest **cr1, const ChordRest **cr2);
    static void layout_bracket(Tuplet *item, const ChordRest *cr1, const ChordRest *cr2, LayoutContext &ctx);
    static bool place_number_on_rhythmic_center(Tuplet *item, const ChordRest *cr1, const ChordRest *cr2, LayoutContext &ctx);
    static bool is_symmetric(Tuplet *item, const ChordRest *cr1, const ChordRest *cr2);
    static double compute_rhythmic_center(Tuplet *item, const ChordRest *end_chord);
    static Fraction center_tick(Tuplet *item);
    static void extend_to_end_of_duration(Tuplet *item, const ChordRest *end_cr);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_TUPLETLAYOUT_DEV_H
