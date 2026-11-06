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

#include "../dom/tremolotwochord.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct MoveTremolo : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, MoveTremolo)

    Score *score = nullptr;
    Fraction chord1_tick;
    Fraction chord2_tick;
    TremoloTwoChord *trem = nullptr;
    track_idx_t track = 0;

    Chord *old_c1 = nullptr;
    Chord *old_c2 = nullptr;

    void undo() override;
    void redo() override;

public:
    MoveTremolo(Score *s, Fraction c1, Fraction c2, TremoloTwoChord *tr, track_idx_t t)
        : score(s), chord1_tick(c1), chord2_tick(c2), trem(tr), track(t) {}

    UNDO_TYPE(CommandType::MOVE_TREMOLO)
    UNDO_NAME("MoveTremolo")
    UNDO_CHANGED_OBJECTS({trem})
};
} // namespace mu::engraving
