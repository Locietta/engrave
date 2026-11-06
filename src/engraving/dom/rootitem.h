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

//! Sits above the pages, which are the top of the visual hierarchy. It takes no part in
//! that hierarchy - nothing is laid out inside it - and exists only to head an
//! accessibility tree; see EngravingItem::accessibleChildren().
struct RootItem : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, RootItem)
public:
    //! Which tree this one heads: the score's own, or the one formed by the objects
    //! parked on the dummy, which are not part of the score.
    enum class Kind { SCORE, DUMMY };

    RootItem(Score *score, Kind kind);

    void init();

    //! The top of a tree has no parent within it.
    EngravingItem *accessible_parent_item() const override { return nullptr; }

    EngravingItemList accessible_children() const override;

    EngravingItem *clone() const override { return nullptr; }

    PropertyValue get_property(Pid) const override { return PropertyValue(); }

    bool set_property(Pid, const PropertyValue &) override { return false; }

private:

    Kind kind_ = Kind::SCORE;
};
} // namespace mu::engraving
