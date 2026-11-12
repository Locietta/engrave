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

#include "editproperty.h"

#include <algorithm>

#include "../dom/score.h"
#include "log.h"
#define LOG_UNDO() \
    if (0) LOGD()

using namespace mu::engraving;

//---------------------------------------------------------
//   ChangeProperty::flip
//---------------------------------------------------------

void ChangeProperty::flip() {
    LOG_UNDO() << element->type_name() << int(id) << "(" << property_name(id) << ")" << element->get_property(id) << "->" << property;

    PropertyValue v = element->get_property(id);
    PropertyFlags ps = element->property_flags(id);

    if (element->is_chord_line() && (std::find(ChordLine::appearance_properties.begin(), ChordLine::appearance_properties.end(), id) !=
                                         ChordLine::appearance_properties.end() ||
                                     id == Pid::APPEARANCE_LINKED_TO_MASTER)) {
        auto *line = to_chord_line(element);
        auto restore = chord_line_appearance;
        chord_line_appearance = line->appearance_state();
        const auto next = property;
        const auto next_flags = flags;
        property = v;
        flags = ps;
        if (restore) {
            line->restore_appearance(*restore);
            line->set_property_flags(id, next_flags);
        } else {
            if (reset_chord_line_path && id == Pid::PATH) {
                line->reset_property(id);
            } else {
                line->set_property(id, next);
            }
            line->set_property_flags(id, next_flags);
        }
        return;
    }

    if (reset_chord_line_path && authored_override_handles(*element, id)) {
        reset_authored_override(*to_engraving_item(element), id);
        // Failure-only inverse replay restores the previous effective value;
        // the paired journal entry restores exact source presence afterwards.
        reset_chord_line_path = false;
    } else {
        element->set_property(id, property);
        element->set_property_flags(id, flags);
    }

    property = v;
    flags = ps;
}

std::vector<EngravingObject *> ChangeProperty::object_items() const { return compound_objects(element); }
