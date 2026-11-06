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

#include "navigation.h"

#include "../dom/harppedaldiagram.h"
#include "../dom/part.h"
#include "../dom/score.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangeHarpPedalState
//---------------------------------------------------------

void ChangeHarpPedalState::flip() {
    std::array<PedalPosition, ::mu::engraving::harp_string_no> f_state = diagram->get_pedal_state();
    if (f_state == pedal_state_value) {
        return;
    }

    diagram->set_pedal_state(pedal_state_value);
    pedal_state_value = f_state;

    diagram->trigger_layout();
}

std::vector<EngravingObject *> ChangeHarpPedalState::object_items() const {
    Part *part = diagram->origin_part();
    std::vector<EngravingObject *> objs{diagram};
    if (!part) {
        return objs;
    }

    HarpPedalDiagram *next_diagram = part->next_harp_diagram(diagram->tick());
    if (next_diagram) {
        objs.push_back(next_diagram);
    } else {
        objs.push_back(Navigation::last_element(diagram->score()));
    }
    return objs;
}

//---------------------------------------------------------
//   ChangeSingleHarpPedal
//---------------------------------------------------------

void ChangeSingleHarpPedal::flip() {
    HarpStringType f_type = type;
    PedalPosition f_pos = diagram->get_pedal_state()[type];
    if (f_pos == pos) {
        return;
    }

    diagram->set_pedal(type, pos);
    type = f_type;
    pos = f_pos;

    diagram->trigger_layout();
}

std::vector<EngravingObject *> ChangeSingleHarpPedal::object_items() const {
    Part *part = diagram->origin_part();
    std::vector<EngravingObject *> objs{diagram};
    if (!part) {
        return objs;
    }

    HarpPedalDiagram *next_diagram = part->next_harp_diagram(diagram->tick());
    if (next_diagram) {
        objs.push_back(next_diagram);
    } else {
        objs.push_back(Navigation::last_element(diagram->score()));
    }
    return objs;
}
