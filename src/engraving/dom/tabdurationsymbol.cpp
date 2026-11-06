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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "tabdurationsymbol.h"

#include "chordrest.h"
#include "stafftype.h"

using namespace mu::engraving;

TabDurationSymbol::TabDurationSymbol(DummyParentOr<ChordRest> parent)
    : EngravingItem(ElementType::TAB_DURATION_SYMBOL, parent, ElementFlag::NOT_SELECTABLE) {
    set_generated(true);
    tab_ = 0;
    text_ = String();
}

TabDurationSymbol::TabDurationSymbol(DummyParentOr<ChordRest> parent, const StaffType *tab, DurationType type, int dots)
    : EngravingItem(ElementType::TAB_DURATION_SYMBOL, parent, ElementFlag::NOT_SELECTABLE) {
    set_generated(true);
    set_duration(type, dots, tab);
}

TabDurationSymbol::TabDurationSymbol(const TabDurationSymbol &e) : EngravingItem(e) {
    tab_ = e.tab_;
    text_ = e.text_;
}

void TabDurationSymbol::set_duration(DurationType type, int dots, const StaffType *tab) {
    tab_ = tab;
    text_ = tab->duration_string(type, dots);
}
