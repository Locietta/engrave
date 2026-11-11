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

#include "indicatoricon.h"

#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "mscore.h"
#include "system.h"

using namespace mu::engraving;
using namespace muse::draw;

IndicatorIcon::IndicatorIcon(const ElementType &type, DummyParentOr<System> parent, ElementFlags flags)
    : EngravingItem(type, parent, flags) {}

Font IndicatorIcon::font() const {
    Font font(configuration()->icons_font_family(), Font::Type::ICON);
    font.set_point_size_f(::mu::engraving::ui_icons_default_font_size * mag_s());
    return font;
}

Fraction IndicatorIcon::tick() const { return system() ? system()->end_tick() : Fraction(0, 1); }
