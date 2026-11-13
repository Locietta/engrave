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

#include "../dom/harppedaldiagram.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct ChangeHarpPedalState : UndoableCommand {
private:
    HarpPedalDiagram *diagram;
    std::array<PedalPosition, ::mu::engraving::harp_string_no> pedal_state_value;

    void flip() override;

public:
    ChangeHarpPedalState(HarpPedalDiagram *input_diagram, std::array<PedalPosition, ::mu::engraving::harp_string_no> input_pedal_state)
        : diagram(input_diagram), pedal_state_value(input_pedal_state) {}

    UNDO_NAME("ChangeHarpPedalState")
    //    UNDO_CHANGED_OBJECTS({ diagram })
    std::vector<EngravingObject *> object_items() const override;
};

} // namespace mu::engraving
