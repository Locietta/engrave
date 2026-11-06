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
#include "editsystemlocks.h"

#include "transaction/transaction.h"
#include "transaction/undoablecommand.h"

#include "../dom/measure.h"
#include "../dom/measurebase.h"
#include "../dom/page.h"
#include "../dom/pagelockindicator.h"
#include "../dom/rangelock.h"
#include "../dom/score.h"
#include "../dom/system.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   AddPageLock
//---------------------------------------------------------

class AddPageLock : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, AddPageLock)

    const RangeLock *page_lock_;
public:
    AddPageLock(const RangeLock *page_lock) : page_lock_(page_lock) {}

    void undo() override {
        Score *score = page_lock_->start_mb()->score();
        score->remove_page_lock(page_lock_);
    }

    void redo() override {
        Score *score = page_lock_->start_mb()->score();
        score->add_page_lock(page_lock_);
    }

    void cleanup(bool undo) override {
        if (!undo) {
            delete page_lock_;
            page_lock_ = nullptr;
        }
    }

    UNDO_NAME("AddPageLock")

    std::vector<EngravingObject *> object_items() const override { return {page_lock_->start_mb(), page_lock_->end_mb()}; }
};

//---------------------------------------------------------
//   RemovePageLock
//---------------------------------------------------------

class RemovePageLock : public UndoableCommand {
    OBJECT_ALLOCATOR(engraving, RemovePageLock)

    const RangeLock *page_lock_;
public:
    RemovePageLock(const RangeLock *page_lock) : page_lock_(page_lock) {}

    void undo() override {
        Score *score = page_lock_->start_mb()->score();
        score->add_page_lock(page_lock_);
    }

    void redo() override {
        Score *score = page_lock_->start_mb()->score();
        score->remove_page_lock(page_lock_);
    }

    void cleanup(bool undo) override {
        if (undo) {
            delete page_lock_;
            page_lock_ = nullptr;
        }
    }

    UNDO_NAME("RemovePageLock")

    std::vector<EngravingObject *> object_items() const override { return {page_lock_->start_mb(), page_lock_->end_mb()}; }
};

//---------------------------------------------------------
//   EditPageLocks
//---------------------------------------------------------

void EditPageLocks::undo_add_page_lock(Transaction &tx, const RangeLock *lock) {
    remove_layout_breaks_on_add_page_lock(tx, lock);
    tx.push(new AddPageLock(lock));
}

void EditPageLocks::undo_remove_page_lock(Transaction &tx, const RangeLock *lock) { tx.push(new RemovePageLock(lock)); }

void EditPageLocks::undo_remove_all_locks(Transaction &tx, Score *score) {
    std::vector<const RangeLock *> all_locks = score->page_locks()->all_locks(); // copy
    for (const RangeLock *lock : all_locks) {
        undo_remove_page_lock(tx, lock);
    }
}

void EditPageLocks::toggle_page_lock(Transaction &tx, Score *score, const std::vector<Page *> &pages) {
    bool unlock_all = true;
    for (const Page *page : pages) {
        if (!page->is_locked()) {
            unlock_all = false;
            break;
        }
    }

    for (Page *page : pages) {
        MeasureBase *start_meas = page->first_measure_base();
        const RangeLock *current_lock = score->page_locks()->lock_starting_at(start_meas);
        if (current_lock && unlock_all) {
            undo_remove_page_lock(tx, current_lock);
            continue;
        } else if (!current_lock && !unlock_all) {
            RangeLock *new_page_lock = new RangeLock(start_meas, page->last_measure_base());
            undo_add_page_lock(tx, new_page_lock);
        }
    }
}

void EditPageLocks::toggle_score_lock(Transaction &tx, Score *score) {
    bool unlock_all = true;
    for (const Page *page : score->pages()) {
        if (!page->is_locked()) {
            unlock_all = false;
            break;
        }
    }

    for (Page *page : score->pages()) {
        MeasureBase *start_meas = page->first_measure_base();
        const RangeLock *current_lock = score->page_locks()->lock_starting_at(start_meas);
        if (current_lock && unlock_all) {
            undo_remove_page_lock(tx, current_lock);
            continue;
        } else if (!current_lock && !unlock_all) {
            RangeLock *new_page_lock = new RangeLock(start_meas, page->last_measure_base());
            undo_add_page_lock(tx, new_page_lock);
        }
    }
}

void EditPageLocks::add_remove_page_locks(Transaction &tx, Score *score, int interval, bool lock) {
    // Should only be called on range selection of whole score
    MeasureBase *start_measure = score->selection().start_measure_base();
    MeasureBase *end_measure = score->selection().end_measure_base();
    if (!end_measure) {
        end_measure = score->last_measure_mm();
    }

    if (!start_measure || !end_measure) {
        return;
    }

    if (lock) {
        for (const Page *page : score->pages()) {
            if (page->last_measure_base()->is_before(start_measure)) {
                continue;
            }
            if (page->first_measure_base()->is_after(end_measure)) {
                break;
            }
            if (!page->is_locked()) {
                undo_add_page_lock(tx, new RangeLock(page->first_measure_base(), page->last_measure_base()));
            }
        }
        return;
    }

    std::vector<const RangeLock *> current_locks = score->page_locks()->locks_contained_in_range(start_measure, end_measure);
    for (const RangeLock *l : current_locks) {
        undo_remove_page_lock(tx, l);
    }

    if (interval == 0) {
        return;
    }

    int count = 0;
    MeasureBase *lock_start = nullptr;
    for (MeasureBase *mb = start_measure; mb; mb = mb->next_mm()) {
        if (count == 0) {
            lock_start = mb;
        }
        count++;
        if (count == interval || mb == end_measure) {
            undo_add_page_lock(tx, new RangeLock(lock_start, mb));
            lock_start = nullptr;
            count = 0;
        }
        if (mb == end_measure) {
            break;
        }
    }
}

void EditPageLocks::make_into_page(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last) {
    const RangeLock *lock_containing_first = score->page_locks()->lock_containing(first);
    const RangeLock *lock_containing_last = score->page_locks()->lock_containing(last);

    if (lock_containing_first) {
        undo_remove_page_lock(tx, lock_containing_first);
        if (lock_containing_first->start_mb()->is_before(first)) {
            MeasureBase *one_before_first = first->prev_mm();
            RangeLock *new_lock_before = new RangeLock(lock_containing_first->start_mb(), one_before_first);
            undo_add_page_lock(tx, new_lock_before);
        }
    }

    if (lock_containing_last) {
        if (lock_containing_last != lock_containing_first) {
            undo_remove_page_lock(tx, lock_containing_last);
        }
        if (last->is_before(lock_containing_last->end_mb())) {
            MeasureBase *one_after_last = last->next_mm();
            RangeLock *new_lock_after = new RangeLock(one_after_last, lock_containing_last->end_mb());
            undo_add_page_lock(tx, new_lock_after);
        }
    }

    std::vector<const RangeLock *> locks_contained_in_range = score->page_locks()->locks_contained_in_range(first, last);
    for (const RangeLock *lock : locks_contained_in_range) {
        if (lock != lock_containing_first && lock != lock_containing_last) {
            undo_remove_page_lock(tx, lock);
        }
    }

    RangeLock *new_lock = new RangeLock(first, last);
    undo_add_page_lock(tx, new_lock);
}

void EditPageLocks::move_measures_to_prev_page(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last) {
    const Page *prev_page = last->prev_page();
    if (!prev_page) {
        return;
    }

    // Lock systems we are moving to the previous page
    std::vector<System *> systems_to_lock;
    System *cur_system = nullptr;
    for (MeasureBase *cur_measure = last; cur_measure && cur_measure != first->prev(); cur_measure = cur_measure->prev()) {
        if (cur_measure->system() == cur_system) {
            continue;
        }
        cur_system = cur_measure->system();

        if (!cur_system->is_locked()) {
            systems_to_lock.push_back(cur_system);
        }
    }
    EditSystemLocks::toggle_system_lock(tx, score, systems_to_lock);

    MeasureBase *prev_page_first_meas = prev_page->first_measure_base();

    const RangeLock *prev_page_lock = score->page_locks()->lock_starting_at(prev_page_first_meas);
    if (prev_page_lock) {
        undo_remove_page_lock(tx, prev_page_lock);
    }

    const Page *cur_page = last->page();
    const RangeLock *cur_page_lock = score->page_locks()->lock_starting_at(cur_page->first_measure_base());
    if (cur_page_lock) {
        undo_remove_page_lock(tx, cur_page_lock);
        if (cur_page_lock->end_mb() != last) {
            MeasureBase *next_mb = last->next_mm();
            RangeLock *new_lock_on_cur_page = new RangeLock(next_mb, cur_page_lock->end_mb());
            undo_add_page_lock(tx, new_lock_on_cur_page);
        }
    }

    RangeLock *page_lock = new RangeLock(prev_page_first_meas, last);
    undo_add_page_lock(tx, page_lock);
}

void EditPageLocks::move_measures_to_next_page(Transaction &tx, Score *score, MeasureBase *first, MeasureBase *last) {
    const Page *cur_page = first->page();
    MeasureBase *start_meas = cur_page->first_measure_base();
    bool ref_measure_is_start_of_page = first == start_meas;

    const Page *next_page = first->next_page();
    if (!next_page) {
        return;
    }

    // Lock systems we are moving to the next page
    std::vector<System *> systems_to_lock;
    System *cur_system = nullptr;
    for (MeasureBase *cur_measure = first; cur_measure && cur_measure != last->next(); cur_measure = cur_measure->next()) {
        if (cur_measure->system() == cur_system) {
            continue;
        }
        cur_system = cur_measure->system();

        if (!cur_system->is_locked()) {
            systems_to_lock.push_back(cur_system);
        }
    }
    EditSystemLocks::toggle_system_lock(tx, score, systems_to_lock);

    const RangeLock *cur_lock = score->page_locks()->lock_starting_at(start_meas);
    if (cur_lock) {
        undo_remove_page_lock(tx, cur_lock);
    }

    if (!ref_measure_is_start_of_page) {
        MeasureBase *prev_meas = first->prev_mm();
        if (!cur_lock) {
            prev_meas->undo_set_break(true, LayoutBreakType::PAGE);
        } else {
            RangeLock *page_lock = new RangeLock(start_meas, prev_meas);
            undo_add_page_lock(tx, page_lock);
        }
    }

    const RangeLock *next_page_lock = score->page_locks()->lock_starting_at(next_page->first_measure_base());
    if (next_page_lock) {
        undo_remove_page_lock(tx, next_page_lock);
    }

    if (next_page_lock || ref_measure_is_start_of_page) {
        RangeLock *new_next_page_lock = new RangeLock(first, next_page->last_measure_base());
        undo_add_page_lock(tx, new_next_page_lock);
    }
}

void EditPageLocks::apply_lock_to_selection(Transaction &tx, Score *score) {
    MeasureBase *first = nullptr;
    MeasureBase *last = nullptr;

    if (score->selection().is_range()) {
        first = score->selection().start_measure_base();
        last = score->selection().end_measure_base();
    } else {
        for (EngravingItem *el : score->selection().elements()) {
            if (el->is_page_lock_indicator()) {
                const RangeLock *lock = to_page_lock_indicator(el)->page_lock();
                first = lock->start_mb();
                last = lock->end_mb();
                break;
            }
            MeasureBase *mb = el->find_measure_base();
            if (!mb) {
                continue;
            }
            if (!first || mb->is_before(first)) {
                first = mb;
            }
            if (!last || mb->is_after(last)) {
                last = mb;
            }
        }
    }

    if (!first || !last) {
        return;
    }

    const RangeLock *lock_on_last = score->page_locks()->lock_containing(last);
    if (lock_on_last && lock_on_last->end_mb() == last) {
        undo_remove_page_lock(tx, lock_on_last);
    } else if (first != last) {
        make_into_page(tx, score, first, last);
    } else {
        make_into_page(tx, score, first->page()->first_measure_base(), last);
    }
}

void EditPageLocks::remove_page_locks_on_add_layout_break(Transaction &tx, Score *score, LayoutBreakType break_type,
                                                          const MeasureBase *measure) {
    IF_ASSERT_FAILED(break_type != LayoutBreakType::NOBREAK) {
        return; // NOBREAK not allowed on locked measures
    }

    const RangeLock *lock = score->page_locks()->lock_containing(measure);
    MeasureBase *lock_end_measure = lock ? lock->end_mb() : nullptr;
    if (lock && break_type == LayoutBreakType::PAGE) {
        undo_remove_page_lock(tx, lock);

        if (measure != lock_end_measure && measure->next()) {
            // Make sure the resultant page is locked
            undo_add_page_lock(tx, new RangeLock(measure->next(), lock_end_measure));
        }
    }
}

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
