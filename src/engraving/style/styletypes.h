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

#include <array>
#include <vector>

#include "../dom/property.h"
#include "../types/propertyvalue.h"
#include "global/types/string.h"
#include "sid.h"

namespace mu::engraving {
struct StyledProperty {
    Sid sid;
    Pid pid;
};

typedef std::vector<StyledProperty> ElementStyle;

struct StyleDef {
public:
    struct StyleValue {
        Sid sid{Sid::NOSTYLE};
        muse::AsciiStringView xml_name;
        PropertyValue default_value;

        inline size_t idx() const { return size_t(sid); }

        inline PType value_type() const { return default_value.type(); }
    };

    static const std::array<StyleValue, size_t(Sid::STYLES)> style_values;

    static double default_smufl_point_size();
};
} // namespace mu::engraving
