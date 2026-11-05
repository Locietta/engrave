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

#include "global/allocator.h"
#include "global/types/string.h"

#include "transaction/undoablecommand.h"

namespace mu::engraving {
class Note;
class Score;
class Selection;
class Tie;
class TieJumpPointList;

class EditTie {
public:
    static void cmdAddTie(Score *score, bool addToChord = false);
    static Tie *cmdToggleTie(Score *score);
    static void cmdToggleLaissezVib(Score *score);
    static std::vector<Note *> cmdTieNoteList(const Selection &selection, bool noteEntryMode);
};

class ChangeTieJumpPointActive : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, ChangeTieJumpPointActive)

    TieJumpPointList *jumpPointList_ = nullptr;
    muse::String id_;
    bool active_ = false;

    void flip() override;

public:
    ChangeTieJumpPointActive(TieJumpPointList *jumpPointList, muse::String id, bool active)
        : jumpPointList_(jumpPointList), id_(id), active_(active) {}

    UNDO_TYPE(CommandType::ChangeTieEndPointActive)
    UNDO_NAME("ChangeTieEndPointActive")
};
} // namespace mu::engraving
