/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>

namespace mu::engraving {
class EngravingItem;
class Note;
class Score;
class Transaction;

class EditParentheses {
public:
    static void toggle_parentheses(Transaction &tx, Score *score);

    static void add_parentheses_to_notes(Transaction &tx, const std::vector<Note *> &notes);
    static void remove_parentheses_from_notes(Transaction &tx, const std::vector<Note *> &notes);
    static void add_parentheses_to_notes(Transaction &tx, Score *score);
    static void remove_parentheses_from_notes(Transaction &tx, Score *score);

    static void toggle_parentheses_on_notes(Transaction &tx, Score *score);

private:
    static void toggle_parentheses_on_item(Transaction &tx, EngravingItem *el);
};
} // namespace mu::engraving
