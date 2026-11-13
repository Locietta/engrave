/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "editharppedaldiagram.h"

#include <vector>

#include "../dom/harppedaldiagram.h"
#include "../dom/part.h"
#include "../dom/score.h"
#include "navigation.h"

using namespace mu::engraving;

void mu::engraving::apply_change_harp_pedal_state(HarpPedalDiagram *diagram,
                                                  std::array<PedalPosition, ::mu::engraving::harp_string_no> pedal_state_value) {
    note_change(diagram, CommandType::UNKNOWN);

    std::array<PedalPosition, ::mu::engraving::harp_string_no> f_state = diagram->get_pedal_state();
    if (f_state == pedal_state_value) {
        return;
    }

    diagram->set_pedal_state(pedal_state_value);

    diagram->trigger_layout();
}
