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

#include "editpagelocks.h"

#include <stdexcept>

#include "../dom/measure.h"
#include "../dom/measurebase.h"
#include "../dom/page.h"
#include "../dom/pagelockindicator.h"
#include "../dom/rangelock.h"
#include "../dom/score.h"
#include "../dom/system.h"
#include "editsystemlocks.h"
#include "engraving/editing/cmd.h"
#include "transaction/transaction.h"

using namespace mu::engraving;

static void apply_add_page_lock(const RangeLock *lock) {
    auto *score = lock->start_mb()->score();
    note_change(lock->start_mb(), CommandType::UNKNOWN);
    note_change(lock->end_mb(), CommandType::UNKNOWN);
    score->add_page_lock(lock);
}

static void apply_remove_page_lock(const RangeLock *lock) {
    auto *score = lock->start_mb()->score();
    note_change(lock->start_mb(), CommandType::UNKNOWN);
    note_change(lock->end_mb(), CommandType::UNKNOWN);
    defer_root(*score, lock);
    score->remove_page_lock(lock);
}

void EditPageLocks::undo_add_page_lock(Transaction &tx, const RangeLock *lock) {
    remove_layout_breaks_on_add_page_lock(tx, lock);
    apply_add_page_lock(lock);
}

void EditPageLocks::undo_remove_page_lock(Transaction &tx, const RangeLock *lock) { apply_remove_page_lock(lock); }

void EditPageLocks::remove_layout_breaks_on_add_page_lock(Transaction &, const RangeLock *lock) {
    for (MeasureBase *mb = lock->start_mb(); mb && mb->is_before_or_equal(lock->end_mb()); mb = mb->next_mm()) {
        mb->undo_set_break(false, LayoutBreakType::PAGE);
    }
}

void EditPageLocks::remove_page_locks_on_remove_measures(Transaction &tx, Score *score, const MeasureBase *m1, const MeasureBase *m2) {
    std::vector<const RangeLock *> all_page_locks = score->page_locks()->all_locks();
    for (const RangeLock *lock : all_page_locks) {
        MeasureBase *lock_start = lock->start_mb();
        MeasureBase *lock_end = lock->end_mb();
        bool lock_start_is_in_range = lock_start->is_after_or_equal(m1) && lock_start->is_before_or_equal(m2);
        bool lock_end_is_in_range = lock_end->is_after_or_equal(m1) && lock_end->is_before_or_equal(m2);
        if (lock_start_is_in_range || lock_end_is_in_range) {
            undo_remove_page_lock(tx, lock);
        }
        if (lock_start_is_in_range && !lock_end_is_in_range) {
            MeasureBase *new_lock_start = m2->next_measure();
            if (new_lock_start) {
                undo_add_page_lock(tx, new RangeLock(new_lock_start, lock_end));
            }
        } else if (!lock_start_is_in_range && lock_end_is_in_range) {
            MeasureBase *new_lock_end = m1->prev_measure();
            if (new_lock_end) {
                undo_add_page_lock(tx, new RangeLock(lock_start, new_lock_end));
            }
        }
    }
}

void EditPageLocks::remove_page_locks_containing_mm_rests(Transaction &tx, Score *score) {
    std::vector<const RangeLock *> all_locks = score->page_locks()->all_locks(); // copy
    for (const RangeLock *lock : all_locks) {
        if ((lock->start_mb()->is_measure() && to_measure(lock->start_mb())->is_mm_rest()) ||
            (lock->end_mb()->is_measure() && to_measure(lock->end_mb())->is_mm_rest())) {
            undo_remove_page_lock(tx, lock);
            continue;
        }
        for (MeasureBase *mb = lock->start_mb(); mb; mb = mb->next()) {
            if (mb->is_measure() && to_measure(mb)->mm_rest()) {
                undo_remove_page_lock(tx, lock);
                break;
            }
            if (mb->is_after(lock->end_mb())) {
                break;
            }
        }
    }
}

void EditPageLocks::update_page_locks_on_create_mm_rests(Transaction &tx, Score *score, Measure *first, Measure *last) {
    // NOTE: this must be done during layout as the mmRests get created.

    for (const RangeLock *lock : score->page_locks()->locks_contained_in_range(first, last)) {
        // These locks are inside the range of the mmRest so remove them
        undo_remove_page_lock(tx, lock);
    }

    const RangeLock *lock_on_first = score->page_locks()->lock_containing(first);
    const RangeLock *lock_on_last = score->page_locks()->lock_containing(last);

    if (lock_on_first) {
        MeasureBase *start_mb = lock_on_first->start_mb();
        MeasureBase *end_mb = lock_on_first->end_mb();

        if (start_mb->is_before(first)) {
            if (end_mb->is_before_or_equal(last)) {
                end_mb = first->mm_rest();
            } else {
                return;
            }
        } else {
            start_mb = first->mm_rest();
        }

        if (start_mb != lock_on_first->start_mb() || end_mb != lock_on_first->end_mb()) {
            undo_remove_page_lock(tx, lock_on_first);
            undo_add_page_lock(tx, new RangeLock(start_mb, end_mb));
        }
    }

    if (!lock_on_last || lock_on_last == lock_on_first) {
        return;
    }

    MeasureBase *start_mb = lock_on_last->start_mb();
    MeasureBase *end_mb = lock_on_last->end_mb();
    assert(start_mb->is_after(first) && end_mb->is_after(last));

    undo_remove_page_lock(tx, lock_on_last);
    start_mb = last->next_mm();
    undo_add_page_lock(tx, new RangeLock(start_mb, end_mb));
}
