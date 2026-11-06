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

#include "logger.h"
#include "eidregister.h"
#include "log.h"

#include "dom/mscore.h"

using namespace mu::engraving;

EID EIDRegister::new_eid_for_item(const EngravingObject *item) {
    EID eid = MScore::test_mode ? EID::new_unique_test_mode(max_val_test_mode_) : EID::new_unique();
    register_item_eid(eid, const_cast<EngravingObject *>(item));
    return eid;
}

bool EIDRegister::register_item_eid(const EID &eid, const EngravingObject *item) {
    std::lock_guard lock(mutex_);
    if (!eid.is_valid() || !item) {
        return false;
    }

    EngravingObject *object = const_cast<EngravingObject *>(item);

    // A file written by a buggy or hand-edited tool can name one id twice. Keep the
    // first element and leave both maps as they are; the reader gives the second
    // element a fresh EID (read500/tread.cpp, readItemEID).
    const auto by_eid = eid_to_item_.find(eid);
    if (by_eid != eid_to_item_.end()) {
        if (by_eid->second == object) {
            return true;
        }
        ++repairs_.duplicate;
        return false;
    }

    // An element keeps exactly one id: re-registering replaces the previous one.
    const auto by_item = item_to_eid_.find(object);
    if (by_item != item_to_eid_.end()) {
        eid_to_item_.erase(by_item->second);
        by_item->second = eid;
    } else {
        item_to_eid_.emplace(object, eid);
    }
    eid_to_item_.emplace(eid, object);

    if (MScore::test_mode) {
        EID::update_max_val_test_mode(eid, max_val_test_mode_);
    }

    return true;
}

void EIDRegister::remove_item(const EngravingObject *item) {
    std::lock_guard lock(mutex_);
    // NOTE: needed only when elements are removed during read (e.g. broken spanners)

    auto item_iter = item_to_eid_.find(const_cast<EngravingObject *>(item));
    IF_ASSERT_FAILED(item_iter != item_to_eid_.end()) { return; }

    EID eid = (*item_iter).second;
    DO_ASSERT(eid.is_valid());

    item_to_eid_.erase(item_iter);

    auto eid_iter = eid_to_item_.find(eid);
    IF_ASSERT_FAILED(eid_iter != eid_to_item_.end()) { return; }

    eid_to_item_.erase(eid_iter);
}

void EIDRegister::on_item_destroyed(const EngravingObject *item) {
    std::lock_guard lock(mutex_);
    // NOTE: most items never get an EID assigned, so a missing entry is the normal case.
    // Items that do have one must be unregistered here: a later allocation can reuse
    // the freed address, and a stale entry would corrupt the register.

    auto item_iter = item_to_eid_.find(const_cast<EngravingObject *>(item));
    if (item_iter == item_to_eid_.end()) {
        return;
    }

    eid_to_item_.erase(item_iter->second);
    item_to_eid_.erase(item_iter);
}

EngravingObject *EIDRegister::item_from_eid(const EID &eid) const {
    std::shared_lock lock(mutex_);
    auto iter = eid_to_item_.find(eid);
    if (iter == eid_to_item_.end()) {
        return nullptr;
    }
    return iter->second;
}

EID EIDRegister::eid_from_item(const EngravingObject *item) const {
    std::shared_lock lock(mutex_);
    auto iter = item_to_eid_.find(const_cast<EngravingObject *>(item));
    return iter == item_to_eid_.end() ? EID::invalid() : iter->second;
}

EIDRepairCount EIDRegister::repairs() const {
    std::shared_lock lock(mutex_);
    return repairs_;
}

std::vector<std::pair<EID, const EngravingObject *>> EIDRegister::entries() const {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<EID, const EngravingObject *>> result;
    result.reserve(eid_to_item_.size());
    for (const auto &entry : eid_to_item_) {
        result.emplace_back(entry.first, entry.second);
    }
    return result;
}

size_t EIDRegister::size() const {
    std::shared_lock lock(mutex_);
    return eid_to_item_.size();
}
