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

#include <optional>

#include "../dom/chordline.h"
#include "../dom/staff.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct ChangeProperty : UndoableCommand {
protected:
    EngravingObject *element = nullptr;
    Pid id;
    PropertyValue property;
    PropertyFlags flags;
    bool reset_chord_line_path = false;
    std::optional<ChordLine::AppearanceState> chord_line_appearance;

    void flip() override;

public:
    ChangeProperty(EngravingObject *e, Pid i, const PropertyValue &v, PropertyFlags ps = PropertyFlags::NOSTYLE, bool reset_path = false)
        : element(e), id(i), property(v), flags(ps), reset_chord_line_path(reset_path) {}

    Pid get_id() const { return id; }

    EngravingObject *get_element() const { return element; }

    PropertyValue data() const { return property; }

    UNDO_TYPE(CommandType::CHANGE_PROPERTY)
    UNDO_NAME("ChangeProperty")

    std::vector<EngravingObject *> object_items() const override;
};

} // namespace mu::engraving
