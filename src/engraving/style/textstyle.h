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

#include "../types/types.h"
#include "styledef.h"

namespace mu::engraving {
struct OffsetSids {
    Sid above = Sid::NOSTYLE;
    Sid below = Sid::NOSTYLE;
};

enum class TextStylePropertyType : char {
    UNDEFINED = 0,
    FONT_FACE,
    FONT_SIZE,
    LINE_SPACING,
    SIZE_SPATIUM_DEPENDENT,
    FONT_STYLE,
    COLOR,
    TEXT_ALIGN,
    OFFSET,
    FRAME_TYPE,
    FRAME_PADDING,
    FRAME_WIDTH,
    FRAME_ROUND,
    FRAME_BORDER_COLOR,
    FRAME_FILL_COLOR,
    MUSICAL_SYMBOLS_SCALE,
    MUSICAL_SYMBOLS_SIZE,
    POSITION
};

struct TextStyleProperty {
    TextStylePropertyType type;
    Sid sid;
    Pid pid;
};

constexpr size_t text_style_size = 16;

struct TextStyle {
    std::array<TextStyleProperty, ::mu::engraving::text_style_size> properties;
    OffsetSids offset_sids;

    auto begin() { return properties.begin(); }

    auto end() { return properties.end(); }

    auto begin() const { return properties.begin(); }

    auto end() const { return properties.end(); }

    auto size() const { return properties.size(); }

    auto &operator[](size_t i) { return properties[i]; }

    const auto &operator[](size_t i) const { return properties[i]; }

    auto &at(size_t i) { return properties.at(i); }

    const auto &at(size_t i) const { return properties.at(i); }
};

const TextStyle *text_style(TextStyleType);
const std::vector<TextStyleType> &all_text_styles();
const std::vector<TextStyleType> &editable_text_styles();
const std::vector<TextStyleType> &primary_text_styles();
} // namespace mu::engraving
