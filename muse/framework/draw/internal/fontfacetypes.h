/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "draw/types/fontstypes.h"
#include "global/types/bytearray.h"

namespace muse::draw {
using f26dot6_t = long; // A signed 26.6 fixed-point type used for vectorial pixel coordinates.

inline long to_f26d6(float v) { return static_cast<long>(v * 64); }

inline double from_f26d6(f26dot6_t v) { return static_cast<double>(v) / 64.0; }

using FBBox = RectX<f26dot6_t>;

struct ShapedGlyph {
    glyph_idx_t idx = 0;
    f26dot6_t x_advance = 0.;
    uint32_t cluster = 0;
};

} // namespace muse::draw
