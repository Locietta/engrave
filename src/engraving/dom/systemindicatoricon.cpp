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

#include "systemindicatoricon.h"

#include "system.h"

namespace mu::engraving {
SystemIndicatorIcon::SystemIndicatorIcon(ElementType type, DummyParentOr<System> parent, ElementFlags flags)
    : IndicatorIcon(type, parent, flags) {}

void SystemIndicatorIcon::set_ownership_parent(DummyParentOr<System> parent) { EngravingItem::set_ownership_parent(parent); }

System *SystemIndicatorIcon::system() const {
    auto *owner = ownership_parent();
    return owner && owner->is_system() ? to_system(owner) : nullptr;
}

Fraction SystemIndicatorIcon::tick() const { return system() ? system()->end_tick() : Fraction(0, 1); }
} // namespace mu::engraving
