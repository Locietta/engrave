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

namespace mu::engraving {
class EngravingItem;
class Score;
class Transaction;

class EditVisibility {
public:
    static void set_selected_elements_visible(Transaction &tx, Score *score);
    static void set_selected_elements_invisible(Transaction &tx, Score *score);
    static void toggle_visible(Transaction &tx, Score *score);

    static void undo_change_visible(Transaction &tx, EngravingItem *item, bool visible);
};
} // namespace mu::engraving
