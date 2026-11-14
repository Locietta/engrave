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

#include "editvisibility.h"

#include "../dom/chord.h"
#include "../dom/engravingitem.h"
#include "../dom/masterscore.h"
#include "../dom/score.h"
#include "cmd.h"

using namespace mu::engraving;

void EditVisibility::change_chord_child_visible(Chord *chord, AuthoredChildKey key, bool visible) {
    const auto *entry = chord->authored_overrides.find(key, Pid::VISIBLE);
    if ((entry ? entry->value.to_bool() : true) == visible) return;
    if (note_change(chord, CommandType::CHANGE_PROPERTY))
        chord->master_score()->cmd_state().changes.changed_property_id_set.insert(Pid::VISIBLE);
    chord->authored_overrides.set({key, Pid::VISIBLE, visible});
    chord->trigger_layout();
}

void EditVisibility::change_dots_visible(EngravingItem *owner, int count, bool visible) {
    for (auto *object : owner->link_list_for_property_propagation()) {
        auto *linked = to_engraving_item(object);
        if (linked != owner && owner->property_propagation(linked, Pid::VISIBLE) != PropertyPropagation::PROPAGATE) continue;
        auto &table = *authored_override_table(linked);
        for (uint32_t i = 0; i < uint32_t(count); ++i) {
            const AuthoredChildKey key{AuthoredChildRole::DOT, i};
            const auto *appearance = table.find(key, Pid::APPEARANCE_LINKED_TO_MASTER);
            if (linked != owner && appearance && !appearance->value.to_bool()) continue;
            const auto *entry = table.find(key, Pid::VISIBLE);
            if ((entry ? entry->value.to_bool() : linked->visible()) == visible) continue;
            if (note_change(linked, CommandType::CHANGE_PROPERTY))
                linked->master_score()->cmd_state().changes.changed_property_id_set.insert(Pid::VISIBLE);
            for (uint32_t j = 0; j <= i; ++j) {
                const AuthoredChildKey ordinal{AuthoredChildRole::DOT, j};
                if (!table.carrier(ordinal)) table.set_carrier({ordinal, ElementType::NOTEDOT});
            }
            table.set({key, Pid::VISIBLE, visible});
            linked->trigger_layout();
        }
    }
}
