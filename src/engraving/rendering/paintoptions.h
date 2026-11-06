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

#include <functional>

#include "types/color.h"

namespace mu::engraving {
struct EngravingItem;
} // namespace mu::engraving

namespace mu::engraving::rendering {
struct PaintOptions {
    bool is_printing = false;
    bool invert_colors = false;

    // Paint items with their literal color, without substituting the user's displayed default color
    bool ignore_displayed_default_color = false;

    std::function<muse::Color(const EngravingItem *item, muse::Color default_color)> override_item_color;
};
} // namespace mu::engraving::rendering
