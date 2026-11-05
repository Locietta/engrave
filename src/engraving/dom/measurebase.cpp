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
#include "measure.h"
#include "page.h"
#include "score.h"
#include "staff.h"
#include "stafftypechange.h"
#include "system.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   MeasureBase
//---------------------------------------------------------

MeasureBase::MeasureBase(const ElementType &type, Score *parent) : EngravingItem(type, parent) {}

void MeasureBase::setOwnershipParent(Score *score) { EngravingItem::setOwnershipParent(score); }

void MeasureBase::setOwnershipParent(Box *box) { EngravingItem::setOwnershipParent(box); }

EngravingItem *MeasureBase::layoutParent() const {
    if (system_) {
        return system_;
    }
    // e.g. a horizontal frame nested inside a vertical frame is placed within its parent frame
    return EngravingItem::layoutParent();
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

void MeasureBase::clearElements() {
    muse::DeleteAll(el_);
    el_.clear();
}

//---------------------------------------------------------
//   takeElements
//---------------------------------------------------------

ElementList MeasureBase::takeElements() {
    ElementList l;
    l.swap(el_);
    return l;
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void MeasureBase::setScore(Score *score) {
    EngravingItem::setScore(score);
    for (EngravingItem *e : el_) {
        e->setScore(score);
    }
}

//---------------------------------------------------------
//   MeasureBase
//---------------------------------------------------------

MeasureBase::~MeasureBase() { muse::DeleteAll(el_); }

System *MeasureBase::prevNonVBoxSystem() const {
    const System *currSystem = system();
    const MeasureBase *firstInCurrSystem = currSystem ? currSystem->first() : nullptr;
    IF_ASSERT_FAILED(firstInCurrSystem) { return nullptr; }

    // TODO: This logic is coupled to moveMeasureToPrevSystem - it should be possible to
    // move a MeasureBase from below a VBox (or n VBoxes) to above. Once that's achieved,
    // we can remove lastIsValid and simply iterate until we find a "non VBox system"...
    const MeasureBase *lastInPrevSystem = firstInCurrSystem->prevMM();
    const bool lastIsValid = lastInPrevSystem && !lastInPrevSystem->isVBoxBase();

    return lastIsValid ? lastInPrevSystem->system() : nullptr;
}

System *MeasureBase::nextNonVBoxSystem() const {
    const System *currSystem = system();
    const MeasureBase *lastInCurrSystem = currSystem ? currSystem->last() : nullptr;
    IF_ASSERT_FAILED(lastInCurrSystem) { return nullptr; }

    // TODO: This logic is coupled to moveMeasureToNextSystem - it should be possible to
    // move a MeasureBase from above a VBox (or n VBoxes) to below. Once that's achieved,
    // we can remove firstIsValid and simply iterate until we find a "non VBox system"...
    const MeasureBase *firstInNextSystem = lastInCurrSystem->nextMM();
    const bool firstIsValid = firstInNextSystem && !firstInNextSystem->isVBoxBase();

    return firstIsValid ? firstInNextSystem->system() : nullptr;
}

Page *MeasureBase::page() const { return system() ? system()->page() : nullptr; }

Page *MeasureBase::prevPage() const {
    Page *curPage = page();
    IF_ASSERT_FAILED(curPage) { return nullptr; }

    Page *prevPage = curPage;
    for (const MeasureBase *mb = this; mb && prevPage == curPage; mb = mb->prevMM()) {
        prevPage = mb->system()->page();
    }

    return prevPage != curPage ? prevPage : nullptr;
}

Page *MeasureBase::nextPage() const {
    Page *curPage = page();
    IF_ASSERT_FAILED(curPage) { return nullptr; }

    Page *nextPage = curPage;
    for (const MeasureBase *mb = this; mb && nextPage == curPage; mb = mb->nextMM()) {
        nextPage = mb->system()->page();
    }

    return nextPage != curPage ? nextPage : nullptr;
}

bool MeasureBase::acceptDrop(EditData &data) const {
    const EngravingItem *e = data.dropElement;
    if (!e || !system() || !page()) {
        return false;
    }

    if (e->isLayoutBreak()) {
        return true;
    }

    if (e->isActionIcon()) {
        switch (toActionIcon(e)->actionType()) {
            case ActionIconType::PAGE_LOCK: {
                LayoutMode layoutMode = score()->layoutMode();
                return layoutMode == LayoutMode::PAGE;
            }
            case ActionIconType::SYSTEM_LOCK: {
                LayoutMode layoutMode = score()->layoutMode();
                return layoutMode == LayoutMode::PAGE || layoutMode == LayoutMode::SYSTEM;
            }
            default: break;
        }
    }
    return false;
}

EngravingItem *MeasureBase::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.dropElement;
    if (!e || !system() || !page()) {
        delete e;
        return nullptr;
    }
    if (e->isLayoutBreak()) {
        LayoutBreak *b = toLayoutBreak(e);
        MeasureBase *mb = this;
        if (isMeasure()) {
            Measure *m = toMeasure(this);
            if (m->isMMRest()) {
                mb = m->mmRestLast();
            }
        }
        switch (b->layoutBreakType()) {
            case LayoutBreakType::PAGE:
                if (mb->pageBreak()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->setLineBreak(false);
                }
                break;
            case LayoutBreakType::LINE:
                if (mb->lineBreak()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->setPageBreak(false);
                }
                break;
            case LayoutBreakType::SECTION:
                if (mb->sectionBreak()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->setLineBreak(false);
                }
                break;
            case LayoutBreakType::NOBREAK:
                if (mb->noBreak() || mb->isEndOfSystemLock() || mb->isEndOfPageLock()) {
                    delete b;
                    b = nullptr;
                } else {
                    mb->setLineBreak(false);
                    mb->setPageBreak(false);
                }
                break;
        }
        if (b) {
            if (b->layoutBreakType() != LayoutBreakType::NOBREAK) {
                EditSystemLocks::removeSystemLocksOnAddLayoutBreak(tx, score(), b->layoutBreakType(), this);
            }
            b->setTrack(0);
            b->setOwnershipParent(mb);
            score()->undoAddElement(b);
        }
        mb->cleanupLayoutBreaks(true);
        return b;
    }
    if (e->isActionIcon()) {
        switch (toActionIcon(e)->actionType()) {
            case ActionIconType::SYSTEM_LOCK: EditSystemLocks::makeIntoSystem(tx, score(), system()->first(), this); break;
            case ActionIconType::PAGE_LOCK: EditPageLocks::makeIntoPage(tx, score(), page()->firstMeasureBase(), this); break;
            default: LOGD("MeasureBase: cannot drop %s here", e->typeName()); break;
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
    if (e->ownershipParent() != this) {
        e->setOwnershipParent(this);
    }

    if (e->isLayoutBreak()) {
        LayoutBreak *b = toLayoutBreak(e);
        switch (b->layoutBreakType()) {
            case LayoutBreakType::PAGE:
                setPageBreak(true);
                setLineBreak(false);
                setNoBreak(false);
                break;
            case LayoutBreakType::LINE:
                setPageBreak(false);
                setLineBreak(true);
                setSectionBreak(false);
                setNoBreak(false);
                break;
            case LayoutBreakType::SECTION:
                setLineBreak(false);
                setSectionBreak(true);
                setNoBreak(false);
                if (b->startWithMeasureOne()) {
                    triggerLayoutToEnd();
                }
                break;
            case LayoutBreakType::NOBREAK:
                setPageBreak(false);
                setLineBreak(false);
                setSectionBreak(false);
                setNoBreak(true);
                break;
        }
        if (next()) {
            next()->triggerLayout();
        }
    }
    triggerLayout();
    el_.push_back(e);
    e->added();
}

//---------------------------------------------------------
//   remove
///   Remove EngravingItem \a el from MeasureBase.
//---------------------------------------------------------

void MeasureBase::remove(EngravingItem *el) {
    if (el->isLayoutBreak()) {
        LayoutBreak *lb = toLayoutBreak(el);
        switch (lb->layoutBreakType()) {
            case LayoutBreakType::PAGE: setPageBreak(false); break;
            case LayoutBreakType::LINE: setLineBreak(false); break;
            case LayoutBreakType::SECTION:
                setSectionBreak(false);
                if (lb->startWithMeasureOne()) {
                    triggerLayoutToEnd();
                }
                break;
            case LayoutBreakType::NOBREAK: setNoBreak(false); break;
        }
    }

    if (!el_.remove(el)) {
        LOGD("MeasureBase(%p)::remove(%s,%p) not found", this, el->typeName(), el);
    } else {
        el->removed();
    }

    triggerLayout();
    if (next()) {
        next()->triggerLayout();
    }
}

//---------------------------------------------------------
//   nextMeasure
//---------------------------------------------------------

Measure *MeasureBase::nextMeasure() const {
    MeasureBase *m = next_;
    for (;;) {
        if (m == 0 || m->isMeasure()) {
            break;
        }
        m = m->next_;
    }
    return toMeasure(m);
}

//---------------------------------------------------------
//   nextMeasureMM
//---------------------------------------------------------

Measure *MeasureBase::nextMeasureMM() const {
    Measure *mm = nextMeasure();
    if (mm && style().styleB(Sid::createMultiMeasureRests) && mm->hasMMRest()) {
        return mm->mmRest();
    }
    return mm;
}

//---------------------------------------------------------
//   prevMeasure
//---------------------------------------------------------

Measure *MeasureBase::prevMeasure() const {
    MeasureBase *m = prev();
    while (m) {
        if (m->isMeasure()) {
            return toMeasure(m);
        }
        m = m->prev();
    }
    return nullptr;
}

//---------------------------------------------------------
//   prevMeasureMM
//---------------------------------------------------------

Measure *MeasureBase::prevMeasureMM() const {
    Measure *m = prevMeasure();
    if (m) {
        return m->coveringMMRestOrThis();
    }

    return nullptr;
}

const MeasureBase *MeasureBase::mbWithPrecedingSectionBreak() const {
    // we're trying to find the MeasureBase that determines
    // if the next one after this starts a new section
    // if this is a measure, it's the one that determines this
    // but if it is a frame, we may need to look backwards
    const MeasureBase *mb = this;
    while (mb && !mb->isMeasure() && !mb->sectionBreak()) {
        mb = mb->prev();
    }
    return mb;
}

//---------------------------------------------------------
//   pause
//---------------------------------------------------------

double MeasureBase::pause() const {
    const LayoutBreak *layoutBreak = sectionBreakElement();
    return layoutBreak ? layoutBreak->pause() : 0.0;
}

//---------------------------------------------------------
//   top
//---------------------------------------------------------

MeasureBase *MeasureBase::top() const {
    const MeasureBase *mb = this;
    while (mb->ownershipParent()) {
        if (mb->ownershipParent()->isMeasureBase()) {
            mb = toMeasureBase(mb->ownershipParent());
        } else {
            break;
        }
    }
    return const_cast<MeasureBase *>(mb);
}

//---------------------------------------------------------
//   getInScore
//---------------------------------------------------------

MeasureBase *MeasureBase::getInScore(Score *score, bool useNextMeasureFallback) const {
    MeasureBase *newMB = nullptr;
    if (!isMeasure() && !excludeFromOtherParts()) {
        for (auto e : linkList()) {
            if (e->score() == score) {
                newMB = toMeasureBase(e);
                break;
            }
        }
    }
    if (isMeasure() || (!newMB && useNextMeasureFallback)) {
        newMB = score->tick2measure(tick());
    }
    if (!newMB) {
        LOGD("measure base not found in score");
    }
    return newMB;
}

//---------------------------------------------------------
//   tick
//---------------------------------------------------------

Fraction MeasureBase::tick() const {
    const MeasureBase *mb = top();
    return mb ? mb->tick_ : Fraction(-1, 1);
}

void MeasureBase::setTick(const Fraction &f) { tick_ = f; }

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void MeasureBase::triggerLayout() const {
    // for measurebases within other measurebases (e.g., hbox within vbox), use top level
    const MeasureBase *mb = top();
    // avoid triggering layout before getting added to a score
    if (mb->prev() || mb->next()) {
        score()->setLayout(mb->tick(), muse::nidx, mb);
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void MeasureBase::scanElements(std::function<void(EngravingItem *)> func) {
    if (isMeasure()) {
        for (EngravingItem *e : el_) {
            staff_idx_t staffIdx = e->staffIdx();
            if (staffIdx != muse::nidx && staffIdx >= score()->staves().size()) {
                LOGD("MeasureBase::scanElements: bad staffIdx %zu in element %s", staffIdx, e->typeName());
            }
            if ((e->track() == muse::nidx) || e->systemFlag() || toMeasure(this)->visible(staffIdx)) {
                e->scanElements(func);
            }
        }
    } else {
        for (EngravingItem *e : el_) {
            e->scanElements(func);
        }
    }
    if (isBox()) {
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

void MeasureBase::undoSetBreak(bool v, LayoutBreakType type) {
    switch (type) {
        case LayoutBreakType::LINE:
            if (lineBreak() == v) {
                return;
            }
            setLineBreak(v);
            break;
        case LayoutBreakType::PAGE:
            if (pageBreak() == v) {
                return;
            }
            if (v && lineBreak()) {
                setLineBreak(false);
            }
            setPageBreak(v);
            break;
        case LayoutBreakType::SECTION:
            if (sectionBreak() == v) {
                return;
            }
            if (v && lineBreak()) {
                setLineBreak(false);
            }
            setSectionBreak(v);
            break;
        case LayoutBreakType::NOBREAK:
            if (noBreak() == v) {
                return;
            }
            if (v) {
                setLineBreak(false);
                setPageBreak(false);
                setSectionBreak(false);
            }
            setNoBreak(v);
            break;
    }

    if (v) {
        MeasureBase *mb = (isMeasure() && toMeasure(this)->isMMRest()) ? toMeasure(this)->mmRestLast() : this;
        LayoutBreak *lb = Factory::createLayoutBreak(mb);
        lb->setLayoutBreakType(type);
        lb->setTrack(0);
        score()->undoAddElement(lb);
    }
    cleanupLayoutBreaks(true);
}

//---------------------------------------------------------
//   cleanupLayoutBreaks
//---------------------------------------------------------

void MeasureBase::cleanupLayoutBreaks(bool undo) {
    // remove unneeded layout breaks
    std::vector<EngravingItem *> toDelete;
    for (EngravingItem *e : el()) {
        if (e->isLayoutBreak()) {
            switch (toLayoutBreak(e)->layoutBreakType()) {
                case LayoutBreakType::LINE:
                    if (!lineBreak()) {
                        toDelete.push_back(e);
                    }
                    break;
                case LayoutBreakType::PAGE:
                    if (!pageBreak()) {
                        toDelete.push_back(e);
                    }
                    break;
                case LayoutBreakType::SECTION:
                    if (!sectionBreak()) {
                        toDelete.push_back(e);
                    }
                    break;
                case LayoutBreakType::NOBREAK:
                    if (!noBreak()) {
                        toDelete.push_back(e);
                    }
                    break;
            }
        }
    }
    for (EngravingItem *e : toDelete) {
        if (undo) {
            score()->undoRemoveElement(e);
        } else {
            el_.remove(e);
        }
    }
}

//---------------------------------------------------------
//   nextMM
//---------------------------------------------------------

MeasureBase *MeasureBase::nextMM() const {
    if (next_ && next_->isMeasure() && style().styleB(Sid::createMultiMeasureRests) && toMeasure(next_)->hasMMRest()) {
        return toMeasure(next_)->mmRest();
    }
    return next_;
}

//---------------------------------------------------------
//   prevMM
//---------------------------------------------------------

MeasureBase *MeasureBase::prevMM() const {
    if (prev_ && prev_->isMeasure() && style().styleB(Sid::createMultiMeasureRests)) {
        return const_cast<Measure *>(toMeasure(prev_)->coveringMMRestOrThis());
    }
    return prev_;
}

//---------------------------------------------------------
//   index
//---------------------------------------------------------

int MeasureBase::index() const {
    int idx = 0;
    MeasureBase *m = score()->first();
    Measure *mmRestFirst = nullptr;
    if (isMeasure() && toMeasure(this)->isMMRest()) {
        mmRestFirst = toMeasure(this)->mmRestFirst();
    }
    while (m) {
        if (m == this || m == mmRestFirst) {
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

int MeasureBase::measureIndex() const {
    int idx = 0;
    MeasureBase *m = score()->firstMeasure();
    Measure *mmRestFirst = nullptr;
    if (isMeasure() && toMeasure(this)->isMMRest()) {
        mmRestFirst = toMeasure(this)->mmRestFirst();
    }
    while (m) {
        if (m == this || m == mmRestFirst) {
            return idx;
        }
        m = m->next();
        if (m && m->isMeasure()) {
            ++idx;
        }
    }
    return -1;
}

bool MeasureBase::isBefore(const EngravingItem *other) const {
    if (other->isMeasureBase()) {
        const MeasureBase *otherMb = toMeasureBase(other);
        return isBefore(otherMb);
    }

    return EngravingItem::isBefore(other);
}

bool MeasureBase::isBefore(const MeasureBase *other) const {
    if (this == other) {
        return false;
    }

    Fraction otherTick = other->tick();
    if (otherTick != tick_) {
        return tick_ < otherTick;
    }

    if (this->isMeasure() && other->isMeasure()) {
        // (this == other) has already been excluded, so this is only
        // possible if one is the overlying mmRest starting on the other.
        // Let's set by convention that the mmRest isBefore the underlying measure.
        return toMeasure(this)->isMMRest();
    }

    bool otherIsMMRest = other->isMeasure() && toMeasure(other)->isMMRest();
    for (const MeasureBase *mb = otherIsMMRest ? nextMM() : next(); mb && mb->tick() == tick_;
         mb = otherIsMMRest ? mb->nextMM() : mb->next()) {
        if (mb == other) {
            return true;
        }
    }

    return false;
}

const RangeLock *MeasureBase::systemLock() const { return score()->systemLocks()->lockContaining(this); }

bool MeasureBase::isStartOfSystemLock() const {
    const RangeLock *lock = score()->systemLocks()->lockStartingAt(this);
    return lock != nullptr;
}

bool MeasureBase::isEndOfSystemLock() const {
    const RangeLock *lock = systemLock();
    return lock && lock->endMB() == this;
}

const RangeLock *MeasureBase::pageLock() const { return score()->pageLocks()->lockContaining(this); }

bool MeasureBase::isStartOfPageLock() const {
    const RangeLock *lock = score()->pageLocks()->lockStartingAt(this);
    return lock != nullptr;
}

bool MeasureBase::isEndOfPageLock() const {
    const RangeLock *lock = pageLock();
    return lock && lock->endMB() == this;
}

//---------------------------------------------------------
//   sectionBreakElement
//---------------------------------------------------------

LayoutBreak *MeasureBase::sectionBreakElement() const {
    if (sectionBreak()) {
        for (EngravingItem *e : el()) {
            if (e->isLayoutBreak() && toLayoutBreak(e)->isSectionBreak()) {
                return toLayoutBreak(e);
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
    tickIndex_.clear();
}

//---------------------------------------------------------
//   push_back
//---------------------------------------------------------

void MeasureBaseList::push_back(MeasureBase *m) {
    ++size_;
    if (last_) {
        last_->setNext(m);
        m->setPrev(last_);
        m->setNext(0);
    } else {
        first_ = m;
        m->setPrev(0);
        m->setNext(0);
    }
    last_ = m;
}

//---------------------------------------------------------
//   push_front
//---------------------------------------------------------

void MeasureBaseList::push_front(MeasureBase *m) {
    ++size_;
    if (first_) {
        first_->setPrev(m);
        m->setNext(first_);
        m->setPrev(0);
    } else {
        last_ = m;
        m->setPrev(0);
        m->setNext(0);
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
    m->setPrev(el->prev());
    el->prev()->setNext(m);
    el->setPrev(m);
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void MeasureBaseList::remove(MeasureBase *m) {
    --size_;
    if (m->prev()) {
        m->prev()->setNext(m->next());
    } else {
        first_ = m->next();
    }
    if (m->next()) {
        m->next()->setPrev(m->prev());
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
        pm->setNext(fm);
    } else {
        first_ = fm;
    }
    MeasureBase *nm = lm->next();
    if (nm) {
        nm->setPrev(lm);
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
        pm->setNext(nm);
    } else {
        first_ = nm;
    }
    if (nm) {
        nm->setPrev(pm);
    } else {
        last_ = pm;
    }
}

//---------------------------------------------------------
//   change
//---------------------------------------------------------

void MeasureBaseList::change(MeasureBase *ob, MeasureBase *nb) {
    nb->setPrev(ob->prev());
    nb->setNext(ob->next());
    if (ob->prev()) {
        ob->prev()->setNext(nb);
    }
    if (ob->next()) {
        ob->next()->setPrev(nb);
    }
    if (ob == last_) {
        last_ = nb;
    }
    if (ob == first_) {
        first_ = nb;
    }
    if (nb->isBox()) {
        nb->setSystem(ob->system());
    }
    for (EngravingItem *e : nb->el()) {
        e->setOwnershipParent(nb);
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
    tickIndex_.emplace(std::make_pair(m->tick().ticks(), m));
}

Measure *MeasureBaseList::measureByTick(int tick) const {
    if (empty() || tick > last_->endTick().ticks()) {
        return nullptr;
    }

    auto it = tickIndex_.upper_bound(tick);

    if (it == tickIndex_.begin()) {
        MeasureBase *mb = it->second;

        if (mb->isMeasure()) {
            return toMeasure(mb);
        }
        return nullptr;
    }

    --it;
    for (;; --it) {
        if (it == tickIndex_.begin()) {
            MeasureBase *mb = it->second;

            if (mb->isMeasure()) {
                return toMeasure(mb);
            }
            return nullptr;
        }

        MeasureBase *mb = it->second;
        if (!mb) {
            break;
        }

        if (mb->isMeasure()) {
            return toMeasure(mb);
        }
    }

    return nullptr;
}

MeasureBase *MeasureBaseList::firstMeasureBaseAtTick(int tick) const {
    if (empty() || tick > last_->endTick().ticks()) {
        return nullptr;
    }

    auto it = tickIndex_.upper_bound(tick);

    if (it != tickIndex_.begin()) {
        --it;
    }
    return it->second;
}

std::vector<MeasureBase *> MeasureBaseList::measureBasesAtTick(int tick) const {
    std::vector<MeasureBase *> result;
    if (empty() || tick > last_->endTick().ticks()) {
        return result;
    }

    result = muse::values(tickIndex_, tick);

    return result;
}

void MeasureBaseList::updateTickIndex() {
    tickIndex_.clear();

    for (MeasureBase *mb = first_; mb; mb = mb->next()) {
        tickIndex_.emplace(std::make_pair(mb->tick().ticks(), mb));
    }
}
