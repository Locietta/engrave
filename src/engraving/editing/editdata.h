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

#include <memory>

#include "../types/types.h"
#include "types/flags.h"
#include "types/string.h"

namespace mu::engraving {
enum KeyboardModifier {
    NO_MODIFIER = 0x00000000,
    SHIFT_MODIFIER = 0x02000000,
    CONTROL_MODIFIER = 0x04000000,
    ALT_MODIFIER = 0x08000000,
    META_MODIFIER = 0x10000000,
    KEYPAD_MODIFIER = 0x20000000,
    GROUP_SWITCH_MODIFIER = 0x40000000,
    // Do not extend the mask to include 0x01000000
    KEYBOARD_MODIFIER_MASK = 0xfe000000
};
DECLARE_FLAGS(KeyboardModifiers, KeyboardModifier)
DECLARE_OPERATORS_FOR_FLAGS(KeyboardModifiers)

#ifdef __APPLE__
constexpr KeyboardModifier text_editing_control_modifier = ALT_MODIFIER;
#else
constexpr KeyboardModifier text_editing_control_modifier = CONTROL_MODIFIER;
#endif

//---------------------------------------------------------
//   Grip
//---------------------------------------------------------

enum class Grip : signed char {
    NO_GRIP = -1,
    START = 0,
    END = 1, // arpeggio etc.
    LEFT = START,
    RIGHT = END, // aliases for dynamic
    MIDDLE = 2,
    APERTURE = 3, // Line
    /*START, END , */
    BEZIER1 = 2,
    SHOULDER = 3,
    BEZIER2 = 4,
    DRAG = 5, // Slur
    GRIPS = 6 // number of grips for slur
};

//---------------------------------------------------------
//   EditData
//    used in dragGrip
//---------------------------------------------------------
struct ElementEditData;
using ElementEditDataPtr = std::shared_ptr<ElementEditData>;

struct EngravingItem;

struct EditData {

    PointF pos;
    KeyboardModifiers modifiers;

    // drop data:
    PointF drag_offset;
    EngravingItem *element = nullptr;
    EngravingItem *drop_element = nullptr;
    track_idx_t track = muse::nidx;

    EditData() = default;

    void clear();

    std::shared_ptr<ElementEditData> get_data(const EngravingItem *) const;
    void add_data(std::shared_ptr<ElementEditData>);
    bool control(bool text_editing = false) const;

    bool shift() const { return modifiers & SHIFT_MODIFIER; }

private:
    std::vector<std::shared_ptr<ElementEditData>> data_;
};
} // namespace mu::engraving
