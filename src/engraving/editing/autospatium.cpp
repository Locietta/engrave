/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "autospatium.h"

#include "../dom/measurebase.h"
#include "../dom/score.h"

using namespace mu::engraving;

//--------------------------------------------------------
// update()
// Reduces the spatium size as necessary to accommodate all
// staves in the page. Caution: the spatium is expressed in
// DPI, page dimensions in inches, staff sizes in mm.
//--------------------------------------------------------
void AutoSpatium::update(Score *score) {
    static constexpr double breathing_space = 2.5;  // allow breathing space between staves
    static constexpr double min_staff_height = 2.0; // never make staff smaller than 2.0 mm
    static constexpr double max_staff_height = 7.0; // never make staff bigger than 7.0 mm (default value)

    const MStyle &style = score->style();
    double available_height =
        (style.style_d(Sid::PAGE_HEIGHT) - style.style_d(Sid::PAGE_ODD_TOP_MARGIN) - style.style_d(Sid::PAGE_ODD_BOTTOM_MARGIN)) *
        ::mu::engraving::dpi; // convert from inches to DPI
    double title_height =
        (!score->measures()->empty() && score->measures()->first()->is_v_box()) ? score->measures()->first()->height() : 0.0;
    available_height -= title_height;
    double total_needed_spaces = 4 * score->nstaves() * breathing_space;
    double target_spatium = available_height / total_needed_spaces;

    double resulting_staff_height = 4 * target_spatium / ::mu::engraving::dpi * ::mu::engraving::inch; // conversion from DPI to mm
    resulting_staff_height = round(resulting_staff_height * 10) / 10;                                  // round to nearest 0.1 mm
    if (resulting_staff_height > max_staff_height) {
        return;
    }
    if (resulting_staff_height < min_staff_height) {
        resulting_staff_height = min_staff_height;
    }

    target_spatium = (resulting_staff_height / 4) * ::mu::engraving::dpi / ::mu::engraving::inch;

    score->style().set_spatium(target_spatium);
    score->update_padding_tables();
}
