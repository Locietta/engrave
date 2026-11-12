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

#include "dom/engravingobject.h"
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
    slot.item->source_handle_ = {};
    slot.item = nullptr;
    slot.eid = EID::invalid();
    slot.live = slot.published = false;
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

    const SourceHandle handle = enroll(*const_cast<EngravingObject *>(item));
    // Allocate the new lookup before dropping the old identity.
    eid_to_handle_.emplace(eid, handle);
    if (slots_[handle.slot].eid.is_valid()) eid_to_handle_.erase(slots_[handle.slot].eid);
    slots_[handle.slot].eid = eid;
    if (MScore::test_mode) EID::update_max_val_test_mode(eid, max_val_test_mode_);
    return true;
}

void EIDRegister::remove_item(const EngravingObject *item) {
    const SourceHandle handle = handle_of(item);
    if (!handle.is_valid()) return;
    Slot &slot = slots_[handle.slot];
    eid_to_handle_.erase(slot.eid);
    slot.eid = EID::invalid();
    slot.published = false;
}

void EIDRegister::on_item_destroyed(const EngravingObject *item) {
    const SourceHandle handle = handle_of(item);
    if (!handle.is_valid()) return;
    set_live(handle.slot, false);
    unlink(handle.slot);
    while (slots_[handle.slot].first_child) unlink(slots_[handle.slot].first_child);
    eid_to_handle_.erase(slots_[handle.slot].eid);
    retire_slot(handle);
}

SourceHandle EIDRegister::handle_of(const EngravingObject *item) const {
    if (!item) return {};
    const SourceHandle handle = item->source_handle_;
    const Slot *slot = find_slot(handle);
    return slot && slot->item == item ? handle : SourceHandle{};
}

SourceHandle EIDRegister::enroll(EngravingObject &item) {
    if (const auto handle = handle_of(&item); handle.is_valid()) return handle;
    const auto handle = allocate_slot(&item, EID::invalid());
    item.source_handle_ = handle;
    return handle;
}

void EIDRegister::unlink(uint32_t index) {
    Slot &slot = slots_[index];
    if (slot.previous)
        slots_[slot.previous].next = slot.next;
    else if (slot.owner)
        slots_[slot.owner].first_child = slot.next;
    if (slot.next) slots_[slot.next].previous = slot.previous;
    slot.owner = slot.previous = slot.next = 0;
}

void EIDRegister::set_live(uint32_t root, bool live) {
    // Walk the affected source subtree without allocating during removal/replay.
    uint32_t index = root;
    for (;;) {
        slots_[index].live = live;
        if (slots_[index].first_child) {
            index = slots_[index].first_child;
            continue;
        }
        while (index != root && !slots_[index].next) index = slots_[index].owner;
        if (index == root) break;
        index = slots_[index].next;
    }
}

void EIDRegister::attach_root(EngravingObject &item) {
    const auto handle = enroll(item);
    unlink(handle.slot);
    set_live(handle.slot, true);
}

void EIDRegister::attach_source(EngravingObject &item, EngravingObject &owner) {
    if (&item == &owner) return;
    const auto child = enroll(item);
    const auto parent = enroll(owner);
    // A member can be mentioned by multiple semantic relations. Only owning
    // collection hooks call this function; adding the same member is idempotent.
    if (slots_[child.slot].owner == parent.slot) {
        if (slots_[child.slot].live != slots_[parent.slot].live) set_live(child.slot, slots_[parent.slot].live);
        return;
    }
    for (uint32_t ancestor = parent.slot; ancestor; ancestor = slots_[ancestor].owner) {
        if (ancestor == child.slot) throw std::logic_error("cyclic source membership");
    }
    unlink(child.slot);
    Slot &slot = slots_[child.slot];
    slot.owner = parent.slot;
    slot.next = slots_[parent.slot].first_child;
    if (slot.next) slots_[slot.next].previous = child.slot;
    slots_[parent.slot].first_child = child.slot;
    set_live(child.slot, slots_[parent.slot].live);
}

void EIDRegister::detach_source(EngravingObject &item) {
    const auto handle = handle_of(&item);
    if (!handle.is_valid()) return;
    set_live(handle.slot, false);
    unlink(handle.slot);
}

void EIDRegister::detach_collection(EngravingObject &owner, SourceCollection collection) {
    const auto handle = handle_of(&owner);
    if (!handle.is_valid()) return;
    for (auto index = slots_[handle.slot].first_child; index;) {
        const auto next = slots_[index].next;
        const auto *item = slots_[index].item;
        const bool matches = collection == SourceCollection::SEGMENTS ? item->is_segment() :
                             collection == SourceCollection::MEASURES ? item->is_measure_base() :
                                                                        item->is_staff();
        if (matches) {
            set_live(index, false);
            unlink(index);
        }
        index = next;
    }
}

EngravingObject *EIDRegister::live_item(SourceHandle handle) const {
    const Slot *slot = find_slot(handle);
    return slot && slot->live ? slot->item : nullptr;
}

bool EIDRegister::published(SourceHandle handle) const {
    const Slot *slot = find_slot(handle);
    return slot && slot->published;
}

void EIDRegister::publish(EngravingObject &item) {
    const auto handle = handle_of(&item);
    if (handle.is_valid()) slots_[handle.slot].published = true;
}

bool EIDRegister::owns(const EngravingObject &owner, const EngravingObject &item) const {
    const auto parent = handle_of(&owner);
    const Slot *slot = find_slot(handle_of(&item));
    return parent.is_valid() && slot && slot->owner == parent.slot;
}

EngravingObject *EIDRegister::item_from_handle(SourceHandle handle) const {
    const Slot *slot = find_slot(handle);
    return slot ? slot->item : nullptr;
}

EngravingObject *EIDRegister::item_from_eid(const EID &eid) const { return item_from_handle(handle_from_eid(eid)); }

SourceHandle EIDRegister::handle_from_eid(const EID &eid) const {
    const auto found = eid_to_handle_.find(eid);
    return found == eid_to_handle_.end() ? SourceHandle{} : found->second;
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
