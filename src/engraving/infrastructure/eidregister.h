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
#include <utility>
#include <vector>

#include <amc/vector.hpp>
#include <ankerl/unordered_dense.h>

#include "../types/types.h"
#include "eid.h"
#include "sourcehandle.h"

namespace mu::engraving {
struct EngravingObject;

// EIDs in the input that could not be used as they were: the text was not a valid
// id, or another element already had it. The reader repairs both, so these count
// load-time repairs, not a live inconsistency.
struct EIDRepairCount {
    size_t malformed = 0;
    size_t duplicate = 0;

    bool any() const { return malformed > 0 || duplicate > 0; }
};

struct EIDRegister {
    EIDRegister();

    EID new_eid_for_item(const EngravingObject *item);
    // False, and neither identity touched, when the EID already belongs to another
    // element; the caller gives the element a fresh EID instead.
    bool register_item_eid(const EID &eid, const EngravingObject *item);
    void remove_item(const EngravingObject *item);
    void on_item_destroyed(const EngravingObject *item);

    void note_malformed_eid() { ++repairs_.malformed; }

    EIDRepairCount repairs() const;

    EngravingObject *item_from_eid(const EID &eid) const;
    SourceHandle handle_from_eid(const EID &eid) const;
    EID eid_from_item(const EngravingObject *item) const;
    SourceHandle handle_of(const EngravingObject *item) const;
    EngravingObject *item_from_handle(SourceHandle handle) const;
    EngravingObject *live_item(SourceHandle handle) const;
    bool published(SourceHandle handle) const;

    bool attached(SourceHandle handle) const {
        const auto *slot = find_slot(handle);
        return slot && slot->owner != 0;
    }

    SourceHandle enroll(EngravingObject &item);
    void attach_root(EngravingObject &item);
    void attach_source(EngravingObject &item, EngravingObject &owner);
    void detach_source(EngravingObject &item);
    void detach_collection(EngravingObject &owner, SourceCollection collection);
    void publish(EngravingObject &item);
    bool owns(const EngravingObject &owner, const EngravingObject &item) const;

    bool membership_ready() const { return membership_ready_; }

    void begin_membership() { membership_ready_ = true; }

    // Every pair in the register, for the id bijection checks of the tests.
    std::vector<std::pair<EID, const EngravingObject *>> entries() const;
    size_t size() const;

private:
    EIDRegister(const EIDRegister &) = delete;
    EIDRegister &operator=(const EIDRegister &) = delete;

    struct Slot {
        EngravingObject *item = nullptr;
        EID eid = EID::invalid();
        uint64_t generation = 1;
        uint32_t next_free = 0;
        uint32_t owner = 0;
        uint32_t first_child = 0;
        uint32_t previous = 0;
        uint32_t next = 0;
        bool live = false;
        bool published = false;
    };

    SourceHandle allocate_slot(EngravingObject *item, EID eid);
    void retire_slot(SourceHandle handle);
    const Slot *find_slot(SourceHandle handle) const;
    void unlink(uint32_t index);
    void set_live(uint32_t index, bool live);

    uint64_t document_ = 0;
    amc::vector<Slot> slots_;
    uint32_t free_slot_ = 0;
    ankerl::unordered_dense::map<EID, SourceHandle> eid_to_handle_;
    bool membership_ready_ = false;

    EIDRepairCount repairs_;

    uint64_t max_val_test_mode_ = 0;
};
} // namespace mu::engraving
