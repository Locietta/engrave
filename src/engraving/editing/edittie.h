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

#include <vector>

#include "global/types/string.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct Note;
struct Score;
struct Selection;
struct Tie;
struct TieJumpPointList;

struct EditTie {
    static void cmd_add_tie(Score *score, bool add_to_chord = false);
    static Tie *cmd_toggle_tie(Score *score);
    static void cmd_toggle_laissez_vib(Score *score);
    static std::vector<Note *> cmd_tie_note_list(const Selection &selection, bool note_entry_mode);
};

struct ChangeTieJumpPointActive : UndoableCommand {
private:

    TieJumpPointList *jump_point_list_ = nullptr;
    muse::String id_;
    bool active_ = false;

    void flip() override;

public:
    ChangeTieJumpPointActive(TieJumpPointList *jump_point_list, muse::String id, bool active)
        : jump_point_list_(jump_point_list), id_(id), active_(active) {}

    UNDO_TYPE(CommandType::CHANGE_TIE_END_POINT_ACTIVE)
    UNDO_NAME("ChangeTieEndPointActive")
};
} // namespace mu::engraving
