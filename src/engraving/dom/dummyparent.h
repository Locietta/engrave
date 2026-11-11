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

#include "engravingobject.h"

namespace mu::engraving {
enum class Pid : short;

struct RootItem;
struct Score;

//! The parent an object has while it is not attached to anything. It is not part of
//! the score and nothing is laid out in it - it is not even an item; it only keeps
//! unattached objects reachable, and heads the accessibility tree they form.
struct DummyParent : EngravingObject {
public:
    DummyParent(Score *score);
    ~DummyParent();

    void init();

    RootItem *root_item() const { return root_; }

    PropertyValue get_property(Pid) const override { return PropertyValue(); }

    bool set_property(Pid, const PropertyValue &) override { return false; }

private:
    RootItem *root_ = nullptr;
};
} // namespace mu::engraving
