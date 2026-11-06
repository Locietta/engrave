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
#ifndef MU_ENGRAVING_TREMOLOLAYOUT_DEV_H
#define MU_ENGRAVING_TREMOLOLAYOUT_DEV_H

#include <utility>

#include "layoutcontext.h"

namespace mu::engraving {
struct TremoloTwoChord;
struct TremoloSingleChord;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
struct TremoloLayout {

    static void layout(TremoloTwoChord *item, const LayoutContext &ctx);
    static void layout(TremoloSingleChord *item, const LayoutContext &ctx);

    static std::pair<double, double> extended_stem_len_with_two_note_tremolo(TremoloTwoChord *tremolo, double stem_len1, double stem_len2);

    static void create_beam_segments(TremoloTwoChord *item, const LayoutContext &ctx);
private:
    static void layout_one_note_tremolo(TremoloSingleChord *item, const LayoutContext &ctx, double x, double y, double h, double spatium);
    static void layout_two_notes_tremolo(TremoloTwoChord *item, const LayoutContext &ctx, double x, double y, double h, double spatium);
    static void calc_is_up(TremoloTwoChord *item);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_TREMOLOLAYOUT_DEV_H
