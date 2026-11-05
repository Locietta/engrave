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

#pragma once

#include "engravingitem.h"

namespace mu::engraving {
//---------------------------------------------------------
//   TabDurationSymbol
//    EngravingItem used to draw duration symbols above tablature staves
//---------------------------------------------------------

enum class TabBeamGrid : char { NONE = 0, INITIAL, MEDIALFINAL, NUM_OF };

class TabDurationSymbol final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, TabDurationSymbol)
    DECLARE_CLASSOF(ElementType::TAB_DURATION_SYMBOL)

public:
    TabDurationSymbol(DummyParentOr<ChordRest> parent);
    TabDurationSymbol(DummyParentOr<ChordRest> parent, const StaffType *tab, DurationType type, int dots);
    TabDurationSymbol(const TabDurationSymbol &);

    TabDurationSymbol *clone() const override { return new TabDurationSymbol(*this); }

    const StaffType *tab() const { return tab_; }

    const String &text() const { return text_; }

    void setDuration(DurationType type, int dots, const StaffType *tab);

    bool isRepeat() const { return repeat_; }

    void setRepeat(bool val) { repeat_ = val; }

    struct LayoutData : public EngravingItem::LayoutData {
        TabBeamGrid beamGrid = TabBeamGrid::NONE; // value for special 'English' grid display
        double beamLength = 0.0;                  // if beamGrid==MEDIALFINAL, length of the beam toward previous grid element
        int beamLevel = 0.0;                      // if beamGrid==MEDIALFINAL, the number of beams
    };
    DECLARE_LAYOUTDATA_METHODS(TabDurationSymbol)

private:
    const StaffType *tab_ = nullptr;
    String text_;
    bool repeat_ = false;
};
} // namespace mu::engraving
