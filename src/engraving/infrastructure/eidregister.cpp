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

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>

#include "dom/engravingitem.h"
#include "dom/excerpt.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/mscore.h"
#include "dom/staff.h"
#include "dom/textbase.h"

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
    touch(index);
    Slot &slot = slots_[index];
    slot.item = item;
    slot.eid = eid;
    slot.next_free = 0;
    return {document_, index, slot.generation};
}

void EIDRegister::retire_slot(SourceHandle handle) {
    touch(handle.slot);
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

EID EIDRegister::new_eid(bool sequential) {
    EID eid = MScore::test_mode && sequential ? EID::new_unique_test_mode(max_val_test_mode_) : EID::new_unique();
    while (eid_to_handle_.contains(eid))
        eid = MScore::test_mode && sequential ? EID::new_unique_test_mode(max_val_test_mode_) : EID::new_unique();
    return eid;
}

EID EIDRegister::new_eid_for_item(const EngravingObject *item) {
    if (!item) return EID::invalid();
    const EID eid = new_eid();
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
    touch(handle.slot);
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
    touch(handle.slot);
    Slot &slot = slots_[handle.slot];
    eid_to_handle_.erase(slot.eid);
    slot.eid = EID::invalid();
    slot.published = false;
}

void EIDRegister::on_item_destroyed(const EngravingObject *item) {
    const SourceHandle handle = handle_of(item);
    if (!handle.is_valid()) return;
    touch(handle.slot);
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

TouchedSlot EIDRegister::id_state(uint32_t index) const {
    const auto &slot = slots_[index];
    TouchedSlot state{index, slot.eid, {}, slot.item && slot.live && slot.published};
    if (!state.published) return state;
    const auto &object = *slot.item;
    const auto *master = object.master_score();
    if (object.score() == master)
        state.placement.score = 0;
    else {
        int index = 1;
        for (const auto *excerpt : master->excerpts()) {
            const auto *score = excerpt->excerpt_score();
            if (score == object.score()) {
                state.placement.score = index;
                break;
            }
            if (score) ++index;
        }
    }
    if (object.is_engraving_item()) {
        const auto &item = *to_engraving_item(&object);
        state.published = (!item.generated() || item.is_tie() || item.is_partial_tie() || item.is_laissez_vib()) &&
                          (!item.is_text() || !to_text_base(&item)->empty());
        if (static_cast<long long>(item.track()) >= 0) {
            state.placement.tick = item.tick().ticks();
            state.placement.track = static_cast<int>(item.track());
        }
    }
    // Spacer visibility affects enumeration, while its published handle remains usable.
    if (object.is_spacer()) {
        const auto &item = *to_engraving_item(&object);
        const auto *measure = to_measure(item.ownership_parent());
        const auto staff = item.staff_idx();
        state.published &= (measure->visible(staff) && item.score()->staff(staff)->show()) || measure->is_cutaway_clef(staff);
    }
    return state;
}

void EIDRegister::touch(uint32_t index) {
    if (!tracking_) return;
    const auto found = std::ranges::lower_bound(touched_, index, {}, &TouchedSlot::index);
    if (found == touched_.end() || found->index != index) touched_.insert(found, id_state(index));
}

void EIDRegister::touch(const EngravingObject &item) {
    const auto handle = handle_of(&item);
    if (handle.is_valid()) touch(handle.slot);
}

void EIDRegister::set_live(EngravingObject &item, bool live) {
    const auto handle = enroll(item);
    if (slots_[handle.slot].live == live) return;
    touch(handle.slot);
    slots_[handle.slot].live = live;
}

EngravingObject *EIDRegister::live_item(SourceHandle handle) const {
    const Slot *slot = find_slot(handle);
    return slot && slot->live ? slot->item : nullptr;
}

bool EIDRegister::published(SourceHandle handle) const {
    const Slot *slot = find_slot(handle);
    return slot && slot->published;
}

void EIDRegister::publish(EngravingObject &item, bool published) {
    const auto handle = handle_of(&item);
    if (!handle.is_valid() || slots_[handle.slot].published == published) return;
    touch(handle.slot);
    slots_[handle.slot].published = published;
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
