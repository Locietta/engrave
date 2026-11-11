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
//    retained for the UndoStack edit-data boundary
//---------------------------------------------------------
struct ElementEditData;
using ElementEditDataPtr = std::shared_ptr<ElementEditData>;

struct EngravingItem;

struct EditData {

    EngravingItem *element = nullptr;

    EditData() = default;

    std::shared_ptr<ElementEditData> get_data(const EngravingItem *) const;
    void add_data(std::shared_ptr<ElementEditData>);


private:
    std::vector<std::shared_ptr<ElementEditData>> data_;
};
} // namespace mu::engraving
