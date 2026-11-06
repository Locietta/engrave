/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "eid.h"
#include "../types/types.h"

namespace mu::engraving {
class EngravingObject;

// EIDs in the input that could not be used as they were: the text was not a valid
// id, or another element already had it. The reader repairs both, so these count
// load-time repairs, not a live inconsistency.
struct EIDRepairCount {
    size_t malformed = 0;
    size_t duplicate = 0;

    bool any() const { return malformed > 0 || duplicate > 0; }
};

class EIDRegister {
public:
    EIDRegister() = default;

    EID new_eid_for_item(const EngravingObject *item);
    // False, and neither map touched, when the EID already belongs to another
    // element; the caller gives the element a fresh EID instead.
    bool register_item_eid(const EID &eid, const EngravingObject *item);
    void remove_item(const EngravingObject *item);
    void on_item_destroyed(const EngravingObject *item);

    void note_malformed_eid() { ++repairs_.malformed; }

    EIDRepairCount repairs() const;

    EngravingObject *item_from_eid(const EID &eid) const;
    EID eid_from_item(const EngravingObject *item) const;
    // Every pair in the register, for the id bijection checks of the tests.
    std::vector<std::pair<EID, const EngravingObject *>> entries() const;
    size_t size() const;

private:
    EIDRegister(const EIDRegister &) = delete;

    std::unordered_map<EID, EngravingObject *> eid_to_item_;
    std::unordered_map<EngravingObject *, EID> item_to_eid_;

    EIDRepairCount repairs_;

    uint64_t max_val_test_mode_ = 0;

    mutable std::shared_mutex mutex_;
};
} // namespace mu::engraving
