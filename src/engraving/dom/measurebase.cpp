/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include "measurebase.h"

#include "../editing/editpagelocks.h"
#include "../editing/editsystemlocks.h"
#include "actionicon.h"
#include "box.h"
#include "factory.h"
#include "layoutbreak.h"
#include "log.h"
#include "measure.h"
#include "page.h"
#include "score.h"
#include "staff.h"
#include "stafftypechange.h"
#include "system.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   MeasureBase
//---------------------------------------------------------

MeasureBase::MeasureBase(const ElementType &type, Score *parent) : EngravingItem(type, parent) {}

void MeasureBase::set_ownership_parent(Score *score) { EngravingItem::set_ownership_parent(score); }

void MeasureBase::set_ownership_parent(Box *box) { EngravingItem::set_ownership_parent(box); }

EngravingItem *MeasureBase::layout_parent() const {
    if (system_) {
        return system_;
    }
    // e.g. a horizontal frame nested inside a vertical frame is placed within its parent frame
    return EngravingItem::layout_parent();
}

MeasureBase::MeasureBase(const MeasureBase &m) : EngravingItem(m) {
    next_ = m.next_;
    prev_ = m.prev_;
    tick_ = m.tick_;

    for (EngravingItem *e : m.el_) {
        add(e->clone());
    }
}

//---------------------------------------------------------
//   clearElements
//---------------------------------------------------------

void MeasureBase::clear_elements() {
    muse::delete_all(el_);
    el_.clear();
}

//---------------------------------------------------------
//   takeElements
//---------------------------------------------------------

ElementList MeasureBase::take_elements() {
    ElementList l;
    l.swap(el_);
    return l;
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void MeasureBase::set_score(Score *score) {
    EngravingItem::set_score(score);
    for (EngravingItem *e : el_) {
        e->set_score(score);
    }
}

//---------------------------------------------------------
//   MeasureBase
//---------------------------------------------------------

MeasureBase::~MeasureBase() { muse::delete_all(el_); }

System *MeasureBase::prev_non_v_box_system() const {
    const System *curr_system = system();
    const MeasureBase *first_in_curr_system = curr_system ? curr_system->first() : nullptr;
    IF_ASSERT_FAILED(first_in_curr_system) { return nullptr; }

    // TODO: This logic is coupled to moveMeasureToPrevSystem - it should be possible to
    // move a MeasureBase from below a VBox (or n VBoxes) to above. Once that's achieved,
    // we can remove lastIsValid and simply iterate until we find a "non VBox system"...
    const MeasureBase *last_in_prev_system = first_in_curr_system->prev_mm();
    const bool last_is_valid = last_in_prev_system && !last_in_prev_system->is_v_box_base();

    return last_is_valid ? last_in_prev_system->system() : nullptr;
}

System *MeasureBase::next_non_v_box_system() const {
    const System *curr_system = system();
    const MeasureBase *last_in_curr_system = curr_system ? curr_system->last() : nullptr;
    IF_ASSERT_FAILED(last_in_curr_system) { return nullptr; }

    // TODO: This logic is coupled to moveMeasureToNextSystem - it should be possible to
    // move a MeasureBase from above a VBox (or n VBoxes) to below. Once that's achieved,
    // we can remove firstIsValid and simply iterate until we find a "non VBox system"...
    const MeasureBase *first_in_next_system = last_in_curr_system->next_mm();
    const bool first_is_valid = first_in_next_system && !first_in_next_system->is_v_box_base();

    return first_is_valid ? first_in_next_system->system() : nullptr;
}

Page *MeasureBase::page() const { return system() ? system()->page() : nullptr; }

Page *MeasureBase::prev_page() const {
    Page *cur_page = page();
    IF_ASSERT_FAILED(cur_page) { return nullptr; }

    Page *prev_page = cur_page;
    for (const MeasureBase *mb = this; mb && prev_page == cur_page; mb = mb->prev_mm()) {
        prev_page = mb->system()->page();
    }

    return prev_page != cur_page ? prev_page : nullptr;
}

Page *MeasureBase::next_page() const {
    Page *cur_page = page();
    IF_ASSERT_FAILED(cur_page) { return nullptr; }

    Page *next_page = cur_page;
    for (const MeasureBase *mb = this; mb && next_page == cur_page; mb = mb->next_mm()) {
        next_page = mb->system()->page();
    }

    return next_page != cur_page ? next_page : nullptr;
}

bool MeasureBase::accept_drop(EditData &data) const {
    const EngravingItem *e = data.drop_element;
    if (!e || !system() || !page()) {
        return false;
    }

    if (e->is_layout_break()) {
        return true;
    }

    if (e->is_action_icon()) {
        switch (to_action_icon(e)->action_type()) {
            case ActionIconType::PAGE_LOCK: {
                LayoutMode layout_mode = score()->layout_mode();
                return layout_mode == LayoutMode::PAGE;
            }
            case ActionIconType::SYSTEM_LOCK: {
                LayoutMode layout_mode = score()->layout_mode();
                return layout_mode == LayoutMode::PAGE || layout_mode == LayoutMode::SYSTEM;
            }
            default: break;
        }
    }
    return false;
}

EngravingItem *MeasureBase::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;
    if (!e || !system() || !page()) {
        delete e;
        return nullptr;
    }
    if (e->is_layout_break()) {
        LayoutBreak *b = to_layout_break(e);
        MeasureBase *mb = this;
        if (is_measure()) {
            Measure *m = to_measure(this);
            if (m->is_mm_rest()) {
                mb = m->mm_rest_last();
            }
        }
        switch (b->layout_break_type()) {
            case LayoutBreakType::PAGE:
                if (mb->page_break()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->set_line_break(false);
                }
                break;
            case LayoutBreakType::LINE:
                if (mb->line_break()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->set_page_break(false);
                }
                break;
            case LayoutBreakType::SECTION:
                if (mb->section_break()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->set_line_break(false);
                }
                break;
            case LayoutBreakType::NOBREAK:
                if (mb->no_break() || mb->is_end_of_system_lock() || mb->is_end_of_page_lock()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->set_line_break(false);
                    mb->set_page_break(false);
                }
                break;
        }
        if (b) {
            if (b->layout_break_type() != LayoutBreakType::NOBREAK) {
                EditSystemLocks::remove_system_locks_on_add_layout_break(tx, score(), b->layout_break_type(), this);
            }
            b->set_track(0);
            b->set_ownership_parent(mb);
            score()->undo_add_element(b);
        }
        mb->cleanup_layout_breaks(true);
        return b;
    }
    if (e->is_action_icon()) {
        switch (to_action_icon(e)->action_type()) {
            case ActionIconType::SYSTEM_LOCK: EditSystemLocks::make_into_system(tx, score(), system()->first(), this); break;
            case ActionIconType::PAGE_LOCK: EditPageLocks::make_into_page(tx, score(), page()->first_measure_base(), this); break;
            default: LOGD("MeasureBase: cannot drop %s here", e->type_name()); break;
        }
    }
    delete e;
    return nullptr;
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to MeasureBase
//---------------------------------------------------------

void MeasureBase::add(EngravingItem *e) {
    if (e->ownership_parent() != this) {
        e->set_ownership_parent(this);
    }

    if (e->is_layout_break()) {
        LayoutBreak *b = to_layout_break(e);
        switch (b->layout_break_type()) {
            case LayoutBreakType::PAGE:
                set_page_break(true);
                set_line_break(false);
                set_no_break(false);
                break;
            case LayoutBreakType::LINE:
                set_page_break(false);
                set_line_break(true);
                set_section_break(false);
                set_no_break(false);
                break;
            case LayoutBreakType::SECTION:
                set_line_break(false);
                set_section_break(true);
                set_no_break(false);
                if (b->start_with_measure_one()) {
                    trigger_layout_to_end();
                }
                break;
            case LayoutBreakType::NOBREAK:
                set_page_break(false);
                set_line_break(false);
                set_section_break(false);
                set_no_break(true);
                break;
        }
        if (next()) {
            next()->trigger_layout();
        }
    }
    trigger_layout();
    el_.push_back(e);
    e->added();
}

//---------------------------------------------------------
//   remove
///   Remove EngravingItem \a el from MeasureBase.
//---------------------------------------------------------

void MeasureBase::remove(EngravingItem *el) {
    if (el->is_layout_break()) {
        LayoutBreak *lb = to_layout_break(el);
        switch (lb->layout_break_type()) {
            case LayoutBreakType::PAGE: set_page_break(false); break;
            case LayoutBreakType::LINE: set_line_break(false); break;
            case LayoutBreakType::SECTION:
                set_section_break(false);
                if (lb->start_with_measure_one()) {
                    trigger_layout_to_end();
                }
                break;
            case LayoutBreakType::NOBREAK: set_no_break(false); break;
        }
    }

    if (!el_.remove(el)) {
        LOGD("MeasureBase(%p)::remove(%s,%p) not found", this, el->type_name(), el);
    } else {
        el->removed();
    }

    trigger_layout();
    if (next()) {
        next()->trigger_layout();
    }
}

//---------------------------------------------------------
//   nextMeasure
//---------------------------------------------------------

Measure *MeasureBase::next_measure() const {
    MeasureBase *m = next_;
    for (;;) {
        if (m == 0 || m->is_measure()) {
            break;
        }
        m = m->next_;
    }
    return to_measure(m);
}

//---------------------------------------------------------
//   nextMeasureMM
//---------------------------------------------------------

Measure *MeasureBase::next_measure_mm() const {
    Measure *mm = next_measure();
    if (mm && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && mm->has_mm_rest()) {
        return mm->mm_rest();
    }
    return mm;
}

//---------------------------------------------------------
//   prevMeasure
//---------------------------------------------------------

Measure *MeasureBase::prev_measure() const {
    MeasureBase *m = prev();
    while (m) {
        if (m->is_measure()) {
            return to_measure(m);
        }
        m = m->prev();
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevMeasureMM
//---------------------------------------------------------

Measure *MeasureBase::prev_measure_mm() const {
    Measure *m = prev_measure();
    if (m) {
        return m->covering_mm_rest_or_this();
    }

    return nullptr;
}

const MeasureBase *MeasureBase::mb_with_preceding_section_break() const {
    // we're trying to find the MeasureBase that determines
    // if the next one after this starts a new section
    // if this is a measure, it's the one that determines this
    // but if it is a frame, we may need to look backwards
    const MeasureBase *mb = this;
    while (mb && !mb->is_measure() && !mb->section_break()) {
        mb = mb->prev();
    }
    return mb;
}

//---------------------------------------------------------
//   pause
//---------------------------------------------------------

double MeasureBase::pause() const {
    const LayoutBreak *layout_break = section_break_element();
    return layout_break ? layout_break->pause() : 0.0;
}

//---------------------------------------------------------
//   top
//---------------------------------------------------------

MeasureBase *MeasureBase::top() const {
    const MeasureBase *mb = this;
    while (mb->ownership_parent()) {
        if (mb->ownership_parent()->is_measure_base()) {
            mb = to_measure_base(mb->ownership_parent());
        } else {
            break;
        }
    }
    return const_cast<MeasureBase *>(mb);
}

//---------------------------------------------------------
//   getInScore
//---------------------------------------------------------

MeasureBase *MeasureBase::get_in_score(Score *score, bool use_next_measure_fallback) const {
    MeasureBase *new_mb = nullptr;
    if (!is_measure() && !exclude_from_other_parts()) {
        for (auto e : link_list()) {
            if (e->score() == score) {
                new_mb = to_measure_base(e);
                break;
            }
        }
    }
    if (is_measure() || (!new_mb && use_next_measure_fallback)) {
        new_mb = score->tick2measure(tick());
    }
    if (!new_mb) {
        LOGD("measure base not found in score");
    }
    return new_mb;
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction MeasureBase::tick() const {
    const MeasureBase *mb = top();
    return mb ? mb->tick_ : Fraction(-1, 1);
}

void MeasureBase::set_tick(const Fraction &f) { tick_ = f; }

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void MeasureBase::trigger_layout() const {
    // for measurebases within other measurebases (e.g., hbox within vbox), use top level
    const MeasureBase *mb = top();
    // avoid triggering layout before getting added to a score
    if (mb->prev() || mb->next()) {
        score()->set_layout(mb->tick(), muse::nidx, mb);
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void MeasureBase::scan_elements(std::function<void(EngravingItem *)> func) {
    if (is_measure()) {
        for (EngravingItem *e : el_) {
            staff_idx_t staff_idx = e->staff_idx();
            if (staff_idx != muse::nidx && staff_idx >= score()->staves().size()) {
                LOGD("MeasureBase::scanElements: bad staffIdx %zu in element %s", staff_idx, e->type_name());
            }
            if ((e->track() == muse::nidx) || e->system_flag() || to_measure(this)->visible(staff_idx)) {
                e->scan_elements(func);
            }
        }
    } else {
        for (EngravingItem *e : el_) {
            e->scan_elements(func);
        }
    }
    if (is_box()) {
        func(this);
    }
}

//---------------------------------------------------------
//   first
//---------------------------------------------------------

MeasureBase *Score::first() const { return measures_.first(); }

//---------------------------------------------------------
//   last
//---------------------------------------------------------

MeasureBase *Score::last() const { return measures_.last(); }

//---------------------------------------------------------
//   undoSetBreak
//---------------------------------------------------------

void MeasureBase::undo_set_break(bool v, LayoutBreakType type) {
    switch (type) {
        case LayoutBreakType::LINE:
            if (line_break() == v) {
                return;
            }
            set_line_break(v);
            break;
        case LayoutBreakType::PAGE:
            if (page_break() == v) {
                return;
            }
            if (v && line_break()) {
                set_line_break(false);
            }
            set_page_break(v);
            break;
        case LayoutBreakType::SECTION:
            if (section_break() == v) {
                return;
            }
            if (v && line_break()) {
                set_line_break(false);
            }
            set_section_break(v);
            break;
        case LayoutBreakType::NOBREAK:
            if (no_break() == v) {
                return;
            }
            if (v) {
                set_line_break(false);
                set_page_break(false);
                set_section_break(false);
            }
            set_no_break(v);
            break;
    }

    if (v) {
        MeasureBase *mb = (is_measure() && to_measure(this)->is_mm_rest()) ? to_measure(this)->mm_rest_last() : this;
        LayoutBreak *lb = Factory::create_layout_break(mb);
        lb->set_layout_break_type(type);
        lb->set_track(0);
        score()->undo_add_element(lb);
    }
    cleanup_layout_breaks(true);
}

//---------------------------------------------------------
//   cleanupLayoutBreaks
//---------------------------------------------------------

void MeasureBase::cleanup_layout_breaks(bool undo) {
    // remove unneeded layout breaks
    std::vector<EngravingItem *> to_delete;
    for (EngravingItem *e : el()) {
        if (e->is_layout_break()) {
            switch (to_layout_break(e)->layout_break_type()) {
                case LayoutBreakType::LINE:
                    if (!line_break()) {
                        to_delete.push_back(e);
                    }
                    break;
                case LayoutBreakType::PAGE:
                    if (!page_break()) {
                        to_delete.push_back(e);
                    }
                    break;
                case LayoutBreakType::SECTION:
                    if (!section_break()) {
                        to_delete.push_back(e);
                    }
                    break;
                case LayoutBreakType::NOBREAK:
                    if (!no_break()) {
                        to_delete.push_back(e);
                    }
                    break;
            }
        }
    }
    for (EngravingItem *e : to_delete) {
        if (undo) {
            score()->undo_remove_element(e);
        } else {
            el_.remove(e);
        }
    }
}

//---------------------------------------------------------
//   nextMM
//---------------------------------------------------------

MeasureBase *MeasureBase::next_mm() const {
    if (next_ && next_->is_measure() && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && to_measure(next_)->has_mm_rest()) {
        return to_measure(next_)->mm_rest();
    }
    return next_;
}

//---------------------------------------------------------
//   prevMM
//---------------------------------------------------------

MeasureBase *MeasureBase::prev_mm() const {
    if (prev_ && prev_->is_measure() && style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS)) {
        return const_cast<Measure *>(to_measure(prev_)->covering_mm_rest_or_this());
    }
    return prev_;
}

//---------------------------------------------------------
//   index
//---------------------------------------------------------

int MeasureBase::index() const {
    int idx = 0;
    MeasureBase *m = score()->first();
    Measure *mm_rest_first = nullptr;
    if (is_measure() && to_measure(this)->is_mm_rest()) {
        mm_rest_first = to_measure(this)->mm_rest_first();
    }
    while (m) {
        if (m == this || m == mm_rest_first) {
            return idx;
        }
        m = m->next();
        ++idx;
    }
    return -1;
}

//---------------------------------------------------------
//   measureIndex
//    returns index of measure counting only Measures but
//    skipping other MeasureBase descendants
//---------------------------------------------------------

int MeasureBase::measure_index() const {
    int idx = 0;
    MeasureBase *m = score()->first_measure();
    Measure *mm_rest_first = nullptr;
    if (is_measure() && to_measure(this)->is_mm_rest()) {
        mm_rest_first = to_measure(this)->mm_rest_first();
    }
    while (m) {
        if (m == this || m == mm_rest_first) {
            return idx;
        }
        m = m->next();
        if (m && m->is_measure()) {
            ++idx;
        }
    }
    return -1;
}

bool MeasureBase::is_before(const EngravingItem *other) const {
    if (other->is_measure_base()) {
        const MeasureBase *other_mb = to_measure_base(other);
        return is_before(other_mb);
    }

    return EngravingItem::is_before(other);
}

bool MeasureBase::is_before(const MeasureBase *other) const {
    if (this == other) {
        return false;
    }

    Fraction other_tick = other->tick();
    if (other_tick != tick_) {
        return tick_ < other_tick;
    }

    if (this->is_measure() && other->is_measure()) {
        // (this == other) has already been excluded, so this is only
        // possible if one is the overlying mmRest starting on the other.
        // Let's set by convention that the mmRest isBefore the underlying measure.
        return to_measure(this)->is_mm_rest();
    }

    bool other_is_mm_rest = other->is_measure() && to_measure(other)->is_mm_rest();
    for (const MeasureBase *mb = other_is_mm_rest ? next_mm() : next(); mb && mb->tick() == tick_;
         mb = other_is_mm_rest ? mb->next_mm() : mb->next()) {
        if (mb == other) {
            return true;
        }
    }

    return false;
}

const RangeLock *MeasureBase::system_lock() const { return score()->system_locks()->lock_containing(this); }

bool MeasureBase::is_start_of_system_lock() const {
    const RangeLock *lock = score()->system_locks()->lock_starting_at(this);
    return lock != nullptr;
}

bool MeasureBase::is_end_of_system_lock() const {
    const RangeLock *lock = system_lock();
    return lock && lock->end_mb() == this;
}

const RangeLock *MeasureBase::page_lock() const { return score()->page_locks()->lock_containing(this); }

bool MeasureBase::is_start_of_page_lock() const {
    const RangeLock *lock = score()->page_locks()->lock_starting_at(this);
    return lock != nullptr;
}

bool MeasureBase::is_end_of_page_lock() const {
    const RangeLock *lock = page_lock();
    return lock && lock->end_mb() == this;
}

//---------------------------------------------------------
//   sectionBreakElement
//---------------------------------------------------------

LayoutBreak *MeasureBase::section_break_element() const {
    if (section_break()) {
        for (EngravingItem *e : el()) {
            if (e->is_layout_break() && to_layout_break(e)->is_section_break()) {
                return to_layout_break(e);
            }
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   MeasureBaseList
//---------------------------------------------------------

MeasureBaseList::MeasureBaseList() {
    first_ = nullptr;
    last_ = nullptr;
    size_ = 0;
}

void MeasureBaseList::clear() {
    first_ = nullptr;
    last_ = nullptr;
    size_ = 0;
    tick_index_.clear();
}

//---------------------------------------------------------
//   push_back
//---------------------------------------------------------

void MeasureBaseList::push_back(MeasureBase *m) {
    ++size_;
    if (last_) {
        last_->set_next(m);
        m->set_prev(last_);
        m->set_next(0);
    } else {
        first_ = m;
        m->set_prev(0);
        m->set_next(0);
    }
    last_ = m;
}

//---------------------------------------------------------
//   push_front
//---------------------------------------------------------

void MeasureBaseList::push_front(MeasureBase *m) {
    ++size_;
    if (first_) {
        first_->set_prev(m);
        m->set_next(first_);
        m->set_prev(0);
    } else {
        last_ = m;
        m->set_prev(0);
        m->set_next(0);
    }
    first_ = m;
}

//---------------------------------------------------------
//   add
//    insert m before m->next()
//---------------------------------------------------------

void MeasureBaseList::add(MeasureBase *m) {
    MeasureBase *el = m->next();
    if (el == 0) {
        append(m);
        return;
    }
    if (el == first_) {
        push_front(m);
        return;
    }
    ++size_;
    m->set_prev(el->prev());
    el->prev()->set_next(m);
    el->set_prev(m);
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void MeasureBaseList::remove(MeasureBase *m) {
    --size_;
    if (m->prev()) {
        m->prev()->set_next(m->next());
    } else {
        first_ = m->next();
    }
    if (m->next()) {
        m->next()->set_prev(m->prev());
    } else {
        last_ = m->prev();
    }
}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

void MeasureBaseList::insert(MeasureBase *fm, MeasureBase *lm) {
    for (MeasureBase *m = fm; m != lm; m = m->next()) {
        ++size_;
    }
    ++size_;
    MeasureBase *pm = fm->prev();
    if (pm) {
        pm->set_next(fm);
    } else {
        first_ = fm;
    }
    MeasureBase *nm = lm->next();
    if (nm) {
        nm->set_prev(lm);
    } else {
        last_ = lm;
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void MeasureBaseList::remove(MeasureBase *fm, MeasureBase *lm) {
    for (MeasureBase *m = fm; m != lm; m = m->next()) {
        --size_;
    }
    --size_;
    MeasureBase *pm = fm->prev();
    MeasureBase *nm = lm->next();
    if (pm) {
        pm->set_next(nm);
    } else {
        first_ = nm;
    }
    if (nm) {
        nm->set_prev(pm);
    } else {
        last_ = pm;
    }
}

//---------------------------------------------------------
//   change
//---------------------------------------------------------

void MeasureBaseList::change(MeasureBase *ob, MeasureBase *nb) {
    nb->set_prev(ob->prev());
    nb->set_next(ob->next());
    if (ob->prev()) {
        ob->prev()->set_next(nb);
    }
    if (ob->next()) {
        ob->next()->set_prev(nb);
    }
    if (ob == last_) {
        last_ = nb;
    }
    if (ob == first_) {
        first_ = nb;
    }
    if (nb->is_box()) {
        nb->set_system(ob->system());
    }
    for (EngravingItem *e : nb->el()) {
        e->set_ownership_parent(nb);
    }
}

//---------------------------------------------------------
//   append
//    append measure to the end of the list and update
//    tick index
//---------------------------------------------------------

void MeasureBaseList::append(MeasureBase *m) {
    assert(!m->next());
    assert(!m->prev() || m->prev() == last_);

    push_back(m);
    tick_index_.emplace(std::make_pair(m->tick().ticks(), m));
}

Measure *MeasureBaseList::measure_by_tick(int tick) const {
    if (empty() || tick > last_->end_tick().ticks()) {
        return nullptr;
    }

    auto it = tick_index_.upper_bound(tick);

    if (it == tick_index_.begin()) {
        MeasureBase *mb = it->second;

        if (mb->is_measure()) {
            return to_measure(mb);
        }
        return nullptr;
    }

    --it;
    for (;; --it) {
        if (it == tick_index_.begin()) {
            MeasureBase *mb = it->second;

            if (mb->is_measure()) {
                return to_measure(mb);
            }
            return nullptr;
        }

        MeasureBase *mb = it->second;
        if (!mb) {
            break;
        }

        if (mb->is_measure()) {
            return to_measure(mb);
        }
    }

    return nullptr;
}

MeasureBase *MeasureBaseList::first_measure_base_at_tick(int tick) const {
    if (empty() || tick > last_->end_tick().ticks()) {
        return nullptr;
    }

    auto it = tick_index_.upper_bound(tick);

    if (it != tick_index_.begin()) {
        --it;
    }
    return it->second;
}

std::vector<MeasureBase *> MeasureBaseList::measure_bases_at_tick(int tick) const {
    std::vector<MeasureBase *> result;
    if (empty() || tick > last_->end_tick().ticks()) {
        return result;
    }

    result = muse::values(tick_index_, tick);

    return result;
}

void MeasureBaseList::update_tick_index() {
    tick_index_.clear();

    for (MeasureBase *mb = first_; mb; mb = mb->next()) {
        tick_index_.emplace(std::make_pair(mb->tick().ticks(), mb));
    }
}
