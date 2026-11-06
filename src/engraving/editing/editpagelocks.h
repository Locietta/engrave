/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

namespace mu::engraving {
struct Measure;
struct MeasureBase;
struct Page;
struct Score;
struct System;
struct Transaction;
struct RangeLock;

enum class LayoutBreakType : unsigned char;

struct EditPageLocks {
    static void undo_add_page_lock(Transaction &tx, const RangeLock *lock);
    static void undo_remove_page_lock(Transaction &tx, const RangeLock *lock);
    static void undo_remove_all_locks(Transaction &tx, Score *score);

    static void toggle_page_lock(Transaction &tx, Score *score, const std::vector<Page *> &pages);
    static void toggle_score_lock(Transaction &tx, Score *score);

    static void add_remove_page_locks(Transaction &tx, Score *score, int interval, bool lock);

    static void make_into_page(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last);
    static void move_measures_to_prev_page(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last);
    static void move_measures_to_next_page(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last);

    static void apply_lock_to_selection(Transaction &tx, Score *score);

    static void remove_layout_breaks_on_add_page_lock(Transaction &tx, const RangeLock *lock);
    static void remove_page_locks_on_add_layout_break(Transaction &tx, Score *score, LayoutBreakType break_type,
                                                      const MeasureBase *measure);
    static void remove_page_locks_on_remove_measures(Transaction &tx, Score *score, const MeasureBase *m1, const MeasureBase *m2);
    static void remove_page_locks_containing_mm_rests(Transaction &tx, Score *score);
    static void update_page_locks_on_create_mm_rests(Transaction &tx, Score *score, Measure *first, Measure *last);
};
} // namespace mu::engraving
