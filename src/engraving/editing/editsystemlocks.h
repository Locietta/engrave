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

namespace mu::engraving {
struct Measure;
struct MeasureBase;
struct Score;
struct System;
struct Transaction;
struct RangeLock;

enum class LayoutBreakType : unsigned char;

struct EditSystemLocks {
    static void undo_add_system_lock(Transaction &tx, const RangeLock *lock);
    static void undo_remove_system_lock(Transaction &tx, const RangeLock *lock);

    static void update_layout_breaks_on_add_system_lock(Transaction &tx, const RangeLock *lock);
    static void remove_system_locks_on_remove_measures(Transaction &tx, Score *score, const MeasureBase *m1, const MeasureBase *m2);
    static void remove_system_locks_containing_mm_rests(Transaction &tx, Score *score);
    static void update_system_locks_on_create_mm_rests(Transaction &tx, Score *score, Measure *first, Measure *last);
};
} // namespace mu::engraving
