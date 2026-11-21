/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "stavesharingchange.h"

#include "engraving/style/styletypes.h"
#include "segment.h"
#include "sharedpart.h"

namespace mu::engraving {
static const ElementStyle stave_sharing_change_style{
    {Sid::STAVE_SHARING_CHANGE_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

StaveSharingChange::StaveSharingChange(DummyParentOr<Segment> parent)
    : IndicatorIcon(ElementType::STAVE_SHARING_CHANGE, parent, ElementFlag::GENERATED | ElementFlag::PLACE_ABOVE | ElementFlag::ON_STAFF) {
    init_element_style(&stave_sharing_change_style);
}

void StaveSharingChange::set_ownership_parent(DummyParentOr<Segment> parent) { EngravingItem::set_ownership_parent(parent); }

const SharedPart *StaveSharingChange::shared_part() const {
    const auto *owner = ownership_parent();
    if (!owner || !owner->is_segment()) return nullptr;
    const Part *p = part();
    return p && p->is_shared_part() ? to_shared_part(p) : nullptr;
}

char16_t StaveSharingChange::icon_code() const {
    if (const auto *p = shared_part()) {
        if (const auto *state = p->exact_authored_state(tick()); state && state->is_reset) return 0xEF19;
        const auto &authored = p->authored_states();
        const auto next = std::upper_bound(authored.begin(), authored.end(), tick(),
                                           [](const Fraction &value, const auto &entry) { return value < entry.first; });
        const auto &rendered = p->rendered_states();
        const auto end = next == authored.end() ? rendered.end() : rendered.lower_bound(next->first);
        for (auto it = rendered.lower_bound(tick()); it != end; ++it) {
            if (!it->second.is_user_map_valid) return 0xF3CE;
        }
    }
    return 0xF499;
}

} // namespace mu::engraving
