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

#include "reset.h"

#include <set>
#include <vector>

#include "../dom/engravingitem.h"
#include "../dom/layoutbreak.h"
#include "../dom/masterscore.h"
#include "../dom/measure.h"
#include "../dom/parenthesis.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/spacer.h"
#include "../dom/spanner.h"
#include "../rendering/score/layoutdatastore.h"
#include "editsystemlocks.h"
#include "global/containers.h"
#include "log.h"
#include "transaction/transaction.h"

using namespace mu;
using namespace muse;
using namespace mu::engraving;

static void reset_element_position(EngravingItem *e) {
    if (e->generated() || !source_object(*e)) {
        return;
    }

    e->undo_reset_property(Pid::AUTOPLACE);
    e->undo_reset_property(Pid::OFFSET);
    e->undo_reset_property(Pid::LEADING_SPACE);
    if (e->is_spanner()) {
        e->undo_reset_property(Pid::OFFSET2);
    }
    // Generic parentheses are owned source children outside handle membership.
    for (auto *child : {e->left_paren(), e->right_paren()}) {
        if (!child || child->generated()) continue;
        for (const auto pid : {Pid::AUTOPLACE, Pid::OFFSET}) {
            if (child->get_property(pid) == child->property_default(pid) && child->property_flags(pid) != PropertyFlags::UNSTYLED) continue;
            e->notify_property_change(pid);
            child->undo_reset_property(pid);
        }
    }
}

static void reset_child_positions(Score &score) {
    std::set<EngravingItem *> owners;
    for (auto *spanner : score.spanner_list()) owners.insert(spanner);
    const auto collect = [&](auto &&self, EngravingObject &object) -> void {
        if (!source_object(object)) return;
        if (object.is_engraving_item() && authored_override_table(to_engraving_item(&object))) owners.insert(to_engraving_item(&object));
        for (auto *child : object.children()) self(self, *child);
    };
    collect(collect, score);
    for (auto *owner : owners) {
        auto &table = *authored_override_table(owner);
        std::vector<std::pair<AuthoredChildKey, Pid>> positions;
        for (const auto &entry : table.entries) {
            if (entry.child.role == AuthoredChildRole::BEAM_FRAGMENT) continue;
            const auto pid = entry.pid;
            if (pid == Pid::AUTOPLACE || pid == Pid::OFFSET || pid == Pid::OFFSET2 || pid == Pid::LEADING_SPACE || pid == Pid::SLUR_UOFF1 ||
                pid == Pid::SLUR_UOFF2 || pid == Pid::SLUR_UOFF3 || pid == Pid::SLUR_UOFF4 || pid == Pid::BEND_VERTEX_OFF)
                positions.emplace_back(entry.child, pid);
        }
        for (auto [key, pid] : positions) {
            if (note_change(owner, CommandType::CHANGE_PROPERTY))
                owner->master_score()->cmd_state().changes.changed_property_id_set.insert(pid);
            reset_child_override(*owner, key, pid);
        }
        if (positions.empty()) continue;
        owner->trigger_layout();
        if (auto *store = score.layout_data_store()) store->erase(*owner);
    }
}

void Reset::reset_all_positions(Transaction &, Score *score) {
    score->scan_elements(reset_element_position);
    reset_child_positions(*score);
}
