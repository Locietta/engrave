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

#include "engravingitem.h"

namespace mu::engraving {
struct Score;

//! Retained as the RootItem type in the public property catalogue.
struct RootItem : EngravingItem {
public:
    explicit RootItem(Score *score);

    EngravingItem *clone() const override { return nullptr; }

    PropertyValue get_property(Pid) const override { return PropertyValue(); }

    bool set_property(Pid, const PropertyValue &) override { return false; }
};
} // namespace mu::engraving
