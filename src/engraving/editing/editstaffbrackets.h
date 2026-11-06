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

#include "transaction/undoablecommand.h"

#include "../dom/staff.h"

namespace mu::engraving {
struct Score;
struct Transaction;

struct EditStaffBrackets {
    static void undo_add_bracket(Score *score, Staff *staff, size_t level, BracketType type, size_t span);
    static void undo_remove_bracket(Score *score, Bracket *bracket);

    static void fill_brackets(Score *score, staff_idx_t staff_idx, size_t idx);
    static void clean_brackets(Score *score, staff_idx_t staff_idx);

    static void adjust_brackets_del(Score *score, size_t sidx, size_t eidx);
    static void adjust_brackets_ins(Score *score, size_t sidx, size_t eidx);

    static void set_bracket_type(Score *score, staff_idx_t staff_idx, size_t idx, BracketType val);
    static void set_bracket_span(Score *score, staff_idx_t staff_idx, size_t idx, size_t val);
    static void set_bracket_visible(Score *score, staff_idx_t staff_idx, size_t idx, bool v);
    static void change_bracket_column(Score *score, staff_idx_t staff_idx, size_t old_column, size_t new_column);
    static void add_bracket(Score *score, staff_idx_t staff_idx, BracketItem *);
    static void insert_bracket(Score *score, staff_idx_t staff_idx, BracketItem *b);
};

struct RemoveBracket : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, RemoveBracket)

    Staff *staff = nullptr;
    size_t level = 0;
    BracketType bracket_type = BracketType::NORMAL;
    size_t span = 0;

    void undo() override;
    void redo() override;

public:
    RemoveBracket(Staff *s, size_t l, BracketType t, size_t sp) : staff(s), level(l), bracket_type(t), span(sp) {}

    UNDO_TYPE(CommandType::REMOVE_BRACKET)
    UNDO_NAME("RemoveBracket")
};

struct AddBracket : UndoableCommand {
private:
    OBJECT_ALLOCATOR(engraving, AddBracket)

    Staff *staff = nullptr;
    size_t level = 0;
    BracketType bracket_type = BracketType::NORMAL;
    size_t span = 0;

    void undo() override;
    void redo() override;

public:
    AddBracket(Staff *s, size_t l, BracketType t, size_t sp) : staff(s), level(l), bracket_type(t), span(sp) {}

    UNDO_TYPE(CommandType::ADD_BRACKET)
    UNDO_NAME("AddBracket")
};
} // namespace mu::engraving
