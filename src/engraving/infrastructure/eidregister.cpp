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

EID EIDRegister::newEIDForItem(const EngravingObject *item) {
    EID eid = MScore::testMode ? EID::newUniqueTestMode(maxValTestMode_) : EID::newUnique();
    registerItemEID(eid, const_cast<EngravingObject *>(item));
    return eid;
}

bool EIDRegister::registerItemEID(const EID &eid, const EngravingObject *item) {
    std::lock_guard lock(mutex_);
    if (!eid.isValid() || !item) {
        return false;
    }

    EngravingObject *object = const_cast<EngravingObject *>(item);

    // A file written by a buggy or hand-edited tool can name one id twice. Keep the
    // first element and leave both maps as they are; the reader gives the second
    // element a fresh EID (read500/tread.cpp, readItemEID).
    const auto byEID = eidToItem_.find(eid);
    if (byEID != eidToItem_.end()) {
        if (byEID->second == object) {
            return true;
        }
        ++repairs_.duplicate;
        return false;
    }

    // An element keeps exactly one id: re-registering replaces the previous one.
    const auto byItem = itemToEid_.find(object);
    if (byItem != itemToEid_.end()) {
        eidToItem_.erase(byItem->second);
        byItem->second = eid;
    } else {
        itemToEid_.emplace(object, eid);
    }
    eidToItem_.emplace(eid, object);

    if (MScore::testMode) {
        EID::updateMaxValTestMode(eid, maxValTestMode_);
    }

    return true;
}

void EIDRegister::removeItem(const EngravingObject *item) {
    std::lock_guard lock(mutex_);
    // NOTE: needed only when elements are removed during read (e.g. broken spanners)

    auto itemIter = itemToEid_.find(const_cast<EngravingObject *>(item));
    IF_ASSERT_FAILED(itemIter != itemToEid_.end()) { return; }

    EID eid = (*itemIter).second;
    DO_ASSERT(eid.isValid());

    itemToEid_.erase(itemIter);

    auto eidIter = eidToItem_.find(eid);
    IF_ASSERT_FAILED(eidIter != eidToItem_.end()) { return; }

    eidToItem_.erase(eidIter);
}

void EIDRegister::onItemDestroyed(const EngravingObject *item) {
    std::lock_guard lock(mutex_);
    // NOTE: most items never get an EID assigned, so a missing entry is the normal case.
    // Items that do have one must be unregistered here: a later allocation can reuse
    // the freed address, and a stale entry would corrupt the register.

    auto itemIter = itemToEid_.find(const_cast<EngravingObject *>(item));
    if (itemIter == itemToEid_.end()) {
        return;
    }

    eidToItem_.erase(itemIter->second);
    itemToEid_.erase(itemIter);
}

EngravingObject *EIDRegister::itemFromEID(const EID &eid) const {
    std::shared_lock lock(mutex_);
    auto iter = eidToItem_.find(eid);
    if (iter == eidToItem_.end()) {
        return nullptr;
    }
    return iter->second;
}

EID EIDRegister::EIDFromItem(const EngravingObject *item) const {
    std::shared_lock lock(mutex_);
    auto iter = itemToEid_.find(const_cast<EngravingObject *>(item));
    return iter == itemToEid_.end() ? EID::invalid() : iter->second;
}

EIDRepairCount EIDRegister::repairs() const {
    std::shared_lock lock(mutex_);
    return repairs_;
}

std::vector<std::pair<EID, const EngravingObject *>> EIDRegister::entries() const {
    std::shared_lock lock(mutex_);
    std::vector<std::pair<EID, const EngravingObject *>> result;
    result.reserve(eidToItem_.size());
    for (const auto &entry : eidToItem_) {
        result.emplace_back(entry.first, entry.second);
    }
    return result;
}

size_t EIDRegister::size() const {
    std::shared_lock lock(mutex_);
    return eidToItem_.size();
}
