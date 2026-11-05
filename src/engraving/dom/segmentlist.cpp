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

#include "segmentlist.h"
#include "segment.h"
#include "score.h"

#include "log.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   clone
//---------------------------------------------------------

SegmentList SegmentList::clone() const {
    SegmentList dl;
    Segment *s = first_;
    for (int i = 0; i < size_; ++i) {
        Segment *ns = s->clone();
        dl.push_back(ns);
        s = s->next();
    }
    dl.check();
    return dl;
}

Segment *SegmentList::at(int index) const {
    Segment *s = first_;
    for (int i = 0; i < size_; ++i) {
        if (i == index) {
            return s;
        }
        s = s->next();
    }
    return nullptr;
}

Segment *SegmentList::firstActive() const {
    if (Segment *segment = first_) {
        return segment->isActive() ? segment : segment->nextActive();
    }
    return nullptr;
}

//---------------------------------------------------------
//   check
//---------------------------------------------------------

#ifndef NDEBUG
void SegmentList::check() {
    int n = 0;
    Segment *f = 0;
    Segment *l = 0;
    for (Segment *s = first_; s; s = s->next()) {
        if (f == 0) {
            f = s;
        }
        l = s;
        ++n;
    }
    for (Segment *s = first_; s; s = s->next()) {
        switch (s->segmentType()) {
            case SegmentType::Invalid:
            case SegmentType::BeginBarLine:
            case SegmentType::HeaderClef:
            case SegmentType::Clef:
            case SegmentType::KeySig:
            case SegmentType::Ambitus:
            case SegmentType::TimeSig:
            case SegmentType::StartRepeatBarLine:
            case SegmentType::BarLine:
            case SegmentType::ChordRest:
            case SegmentType::Breath:
            case SegmentType::ClefRepeatAnnounce:
            case SegmentType::TimeSigRepeatAnnounce:
            case SegmentType::KeySigRepeatAnnounce:
            case SegmentType::ClefStartRepeatAnnounce:
            case SegmentType::TimeSigStartRepeatAnnounce:
            case SegmentType::KeySigStartRepeatAnnounce:
            case SegmentType::EndBarLine:
            case SegmentType::TimeSigAnnounce:
            case SegmentType::KeySigAnnounce:
            case SegmentType::TimeTick: break;
            default: ASSERT_X(String(u"SegmentList::check: invalid segment type: %1").arg(int(s->segmentType()))); break;
        }
        Segment *ss = s->next();
        while (ss) {
            if (s == ss) {
                ASSERT_X("SegmentList::check: segment twice in list");
            }
            ss = ss->next();
        }
    }
    if (f != first_) {
        ASSERT_X("SegmentList::check: bad first");
    }
    if (l != last_) {
        ASSERT_X("SegmentList::check: bad last");
    }
    if (f && f->prev()) {
        ASSERT_X("SegmentList::check: first has prev");
    }
    if (l && l->next()) {
        ASSERT_X("SegmentList::check: last has next");
    }
    if (n != size_) {
        ASSERT_X(String(u"SegmentList::check: counted %1 but _size is %d2").arg(n, size_));
        size_ = n;
    }
}

#endif

//---------------------------------------------------------
//   insert
///   Insert Segment \a e before Segment \a el.
//---------------------------------------------------------

void SegmentList::insert(Segment *e, Segment *el) {
    if (el == 0) {
        push_back(e);
    } else if (el == first()) {
        push_front(e);
    } else {
        ++size_;
        e->setNext(el);
        e->setPrev(el->prev());
        el->prev()->setNext(e);
        el->setPrev(e);
    }
    check();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void SegmentList::remove(Segment *e) {
#ifndef NDEBUG
    check();
    bool found = false;
    for (Segment *s = first_; s; s = s->next()) {
        if (e == s) {
            found = true;
            break;
        }
    }
    if (!found) {
        ASSERT_X(String(u"segment %1 not in list").arg(String::fromAscii(e->subTypeName())));
    }
#endif
    --size_;
    if (e == first_) {
        first_ = first_->next();
        if (first_) {
            first_->setPrev(0);
        }
        if (e == last_) {
            last_ = 0;
        }
    } else if (e == last_) {
        last_ = last_->prev();
        if (last_) {
            last_->setNext(0);
        }
    } else {
        e->prev()->setNext(e->next());
        e->next()->setPrev(e->prev());
    }
}

//---------------------------------------------------------
//   push_back
//---------------------------------------------------------

void SegmentList::push_back(Segment *e) {
    ++size_;
    e->setNext(0);
    if (last_) {
        last_->setNext(e);
    } else {
        first_ = e;
    }
    e->setPrev(last_);
    last_ = e;
    check();
}

//---------------------------------------------------------
//   push_front
//---------------------------------------------------------

void SegmentList::push_front(Segment *e) {
    ++size_;
    e->setPrev(0);
    if (first_) {
        first_->setPrev(e);
    } else {
        last_ = e;
    }
    e->setNext(first_);
    first_ = e;
    check();
}

//---------------------------------------------------------
//   firstCRSegment
//---------------------------------------------------------

Segment *SegmentList::firstCRSegment() const { return first(SegmentType::ChordRest); }

//---------------------------------------------------------
//   first
//---------------------------------------------------------

Segment *SegmentList::first(SegmentType types) const {
    for (Segment *s = first_; s; s = s->next()) {
        if (s->segmentType() & types) {
            return s;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   first
//---------------------------------------------------------

Segment *SegmentList::first(ElementFlag flags) const {
    for (Segment *s = first_; s; s = s->next()) {
        if (s->flag(flags)) {
            return s;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   last
//---------------------------------------------------------

Segment *SegmentList::last(ElementFlag flags) const {
    for (Segment *s = last_; s; s = s->prev()) {
        if (s->flag(flags)) {
            return s;
        }
    }
    return nullptr;
}

Segment *SegmentList::last(SegmentType types) const {
    for (Segment *s = last_; s; s = s->prev()) {
        if (s->segmentType() & types) {
            return s;
        }
    }
    return nullptr;
}
} // namespace mu::engraving
