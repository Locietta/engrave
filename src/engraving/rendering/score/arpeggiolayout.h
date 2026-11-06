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
#ifndef MU_ENGRAVING_ARPEGGIOLAYOUT_DEV_H
#define MU_ENGRAVING_ARPEGGIOLAYOUT_DEV_H

#include <climits>

#include "dom/arpeggio.h"
#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
struct ArpeggioLayout {
private:
    static constexpr int arbitrary_arpeggio_length = INT_MAX;
public:

    static void layout_arpeggio2(Arpeggio *item, LayoutContext &ctx);

    static double inset_distance(const Arpeggio *item, const LayoutContext &ctx, double mag, const Chord *chord);
    static double inset_distance(const Arpeggio *item, const LayoutContext &ctx, double mag, const Chord *chord,
                                 const std::vector<Accidental *> &accidentals);
    static double inset_top(const Arpeggio *item, const Chord *c);
    static double inset_bottom(const Arpeggio *item, const Chord *c);
    static double inset_width(const Arpeggio *item);
    static void clear_accidentals(Arpeggio *item, LayoutContext &ctx);

    static double compute_height(const Arpeggio *item);
    static double calc_top(const Arpeggio *item, const LayoutConfiguration &conf);
    static double calc_bottom(const Arpeggio *item, double arpeggio_height, const LayoutConfiguration &conf);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_ARPEGGIOLAYOUT_DEV_H
