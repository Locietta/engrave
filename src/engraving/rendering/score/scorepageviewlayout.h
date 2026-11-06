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
#ifndef MU_ENGRAVING_SCOREPAGEVIEWLAYOUT_DEV_H
#define MU_ENGRAVING_SCOREPAGEVIEWLAYOUT_DEV_H

#include "layoutcontext.h"

namespace mu::engraving::rendering::score {
class ScorePageViewLayout {
public:

    static void layout_page_view(Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick);
    static void layout_headers_footers(LayoutContext &ctx);

private:

    static void init_layout_context(const Score *score, LayoutContext &ctx, const Fraction &stick, const Fraction &etick);
    static void prepare_score(Score *score, const LayoutContext &ctx);

    static void do_layout(LayoutContext &ctx);

    static void do_layout_headers_footers(LayoutContext &ctx);

    static void layout_finished(Score *score, LayoutContext &ctx);
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_SCOREPAGEVIEWLAYOUT_DEV_H
