/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "../dom/engravingitem.h"
#include "../dom/property.h"

namespace mu::engraving {
//-----------------------------------------------------------------------------
//   ElementEditData
//    holds element specific data during element editing:
//
enum class EditDataType : signed char {
    ELEMENT_EDIT_DATA,
    TEXT_EDIT_DATA,
};

class ElementEditData {
public:
    virtual ~ElementEditData() = default;

    struct PropertyData {
        Pid id;
        PropertyValue data;
        PropertyFlags f;
    };

    EngravingItem *e = nullptr;
    std::vector<PropertyData> property_data;

    void push_property(Pid pid) { property_data.emplace_back(PropertyData{pid, e->get_property(pid), e->property_flags(pid)}); }

    virtual EditDataType type() { return EditDataType::ELEMENT_EDIT_DATA; }
};

using ElementEditDataPtr = std::shared_ptr<ElementEditData>;
} // namespace mu::engraving
