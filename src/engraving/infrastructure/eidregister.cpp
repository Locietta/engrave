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

#include "eidregister.h"

#include <atomic>
#include <limits>
#include <stdexcept>

#include "dom/mscore.h"

using namespace mu::engraving;

namespace {
std::atomic<uint64_t> next_document{1};
} // namespace

EIDRegister::EIDRegister() {
    // Exhaustion is permanent: never wrap an incarnation and revive an old handle.
    auto current = next_document.load(std::memory_order_relaxed);
    do {
        if (current == std::numeric_limits<uint64_t>::max()) throw std::overflow_error("document handle space exhausted");
    } while (!next_document.compare_exchange_weak(current, current + 1, std::memory_order_relaxed));
    document_ = current;
    slots_.emplace_back(); // slot zero is invalid
}

const EIDRegister::Slot *EIDRegister::find_slot(SourceHandle handle) const {
    if (handle.document != document_ || handle.slot == 0 || handle.slot >= slots_.size()) return nullptr;
    const Slot &slot = slots_[handle.slot];
    return slot.item && slot.generation == handle.generation ? &slot : nullptr;
}

SourceHandle EIDRegister::allocate_slot(EngravingObject *item, EID eid) {
    uint32_t index = free_slot_;
    if (index) {
        free_slot_ = slots_[index].next_free;
    } else {
        if (slots_.size() >= std::numeric_limits<uint32_t>::max()) throw std::overflow_error("source handle space exhausted");
        index = static_cast<uint32_t>(slots_.size());
        slots_.emplace_back();
    }
    Slot &slot = slots_[index];
    slot.item = item;
    slot.eid = eid;
    slot.next_free = 0;
    return {document_, index, slot.generation};
}

void EIDRegister::retire_slot(SourceHandle handle) {
    Slot &slot = slots_[handle.slot];
    slot.item = nullptr;
    slot.eid = EID::invalid();
    if (slot.generation == std::numeric_limits<uint64_t>::max()) return;
    ++slot.generation;
    slot.next_free = free_slot_;
    free_slot_ = handle.slot;
}

EID EIDRegister::new_eid_for_item(const EngravingObject *item) {
    if (!item) return EID::invalid();
    EID eid = MScore::test_mode ? EID::new_unique_test_mode(max_val_test_mode_) : EID::new_unique();
    while (eid_to_handle_.contains(eid)) eid = MScore::test_mode ? EID::new_unique_test_mode(max_val_test_mode_) : EID::new_unique();
    register_item_eid(eid, item);
    return eid;
}

bool EIDRegister::register_item_eid(const EID &eid, const EngravingObject *item) {
    if (!eid.is_valid() || !item) return false;
    const auto by_eid = eid_to_handle_.find(eid);
    if (by_eid != eid_to_handle_.end()) {
        if (item_from_handle(by_eid->second) == item) return true;
        ++repairs_.duplicate;
        return false;
    }

    const auto by_item = item_to_handle_.find(item);
    if (by_item != item_to_handle_.end()) {
        const SourceHandle handle = by_item->second;
        // Allocate the new lookup before dropping the old identity.
        eid_to_handle_.emplace(eid, handle);
        eid_to_handle_.erase(slots_[handle.slot].eid);
        slots_[handle.slot].eid = eid;
    } else {
        const SourceHandle handle = allocate_slot(const_cast<EngravingObject *>(item), eid);
        try {
            eid_to_handle_.emplace(eid, handle);
            item_to_handle_.emplace(item, handle);
        } catch (...) {
            eid_to_handle_.erase(eid);
            retire_slot(handle);
            throw;
        }
    }
    if (MScore::test_mode) EID::update_max_val_test_mode(eid, max_val_test_mode_);
    return true;
}

void EIDRegister::remove_item(const EngravingObject *item) { on_item_destroyed(item); }

void EIDRegister::on_item_destroyed(const EngravingObject *item) {
    const auto found = item_to_handle_.find(item);
    if (found == item_to_handle_.end()) return;
    const SourceHandle handle = found->second;
    eid_to_handle_.erase(slots_[handle.slot].eid);
    item_to_handle_.erase(found);
    retire_slot(handle);
}

SourceHandle EIDRegister::handle_of(const EngravingObject *item) const {
    const auto found = item_to_handle_.find(item);
    return found == item_to_handle_.end() ? SourceHandle{} : found->second;
}

EngravingObject *EIDRegister::item_from_handle(SourceHandle handle) const {
    const Slot *slot = find_slot(handle);
    return slot ? slot->item : nullptr;
}

EngravingObject *EIDRegister::item_from_eid(const EID &eid) const {
    const auto found = eid_to_handle_.find(eid);
    return found == eid_to_handle_.end() ? nullptr : item_from_handle(found->second);
}

EID EIDRegister::eid_from_item(const EngravingObject *item) const {
    const Slot *slot = find_slot(handle_of(item));
    return slot ? slot->eid : EID::invalid();
}

EIDRepairCount EIDRegister::repairs() const { return repairs_; }

std::vector<std::pair<EID, const EngravingObject *>> EIDRegister::entries() const {
    std::vector<std::pair<EID, const EngravingObject *>> result;
    result.reserve(eid_to_handle_.size());
    for (const auto &[eid, handle] : eid_to_handle_) result.emplace_back(eid, item_from_handle(handle));
    return result;
}

size_t EIDRegister::size() const { return eid_to_handle_.size(); }
