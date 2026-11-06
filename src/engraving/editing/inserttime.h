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

#include "../dom/score.h"
#include "transaction/undoablecommand.h"

namespace mu::engraving {
struct InsertTime : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, InsertTime)

    Score *score = nullptr;
    Fraction tick;
    Fraction len;

    void redo() override;
    void undo() override;

public:
    InsertTime(Score *input_score, const Fraction &input_tick, const Fraction &input_len)
        : score(input_score), tick(input_tick), len(input_len) {}

    UNDO_TYPE(CommandType::INSERT_TIME)
    UNDO_NAME("InsertTime")
    UNDO_CHANGED_OBJECTS({score})
};

struct InsertTimeUnmanagedSpanner : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, InsertTimeUnmanagedSpanner)

    Score *score = nullptr;
    Fraction tick;
    Fraction len;

    void flip() override;

public:
    InsertTimeUnmanagedSpanner(Score *s, const Fraction &input_tick, const Fraction &input_len)
        : score(s), tick(input_tick), len(input_len) {}

    UNDO_TYPE(CommandType::INSERT_TIME_UNMANAGED_SPANNER)
    UNDO_NAME("InsertTimeUnmanagedSpanner")
    UNDO_CHANGED_OBJECTS({score})
};
} // namespace mu::engraving
