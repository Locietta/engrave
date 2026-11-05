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

#pragma once

#include "segment.h"

namespace mu::engraving {
class Segment;

//---------------------------------------------------------
//   SegmentList
//---------------------------------------------------------

class SegmentList {
public:
    SegmentList() { clear(); }

    void clear() {
        first_ = last_ = 0;
        size_ = 0;
    }
#ifndef NDEBUG
    void check();
#else
    void check() {}
#endif
    SegmentList clone() const;

    int size() const { return size_; }

    Segment *at(int index) const;

    Segment *first() const { return first_; }

    Segment *firstActive() const;
    Segment *first(SegmentType) const;
    Segment *first(ElementFlag) const;

    Segment *last() const { return last_; }

    Segment *last(ElementFlag) const;
    Segment *last(SegmentType) const;
    Segment *firstCRSegment() const;
    void remove(Segment *);
    void push_back(Segment *);
    void push_front(Segment *);
    void insert(Segment *e, Segment *el); // insert e before el

    class iterator {
        Segment *p;
    public:
        iterator(Segment *s) { p = s; }

        iterator operator++() {
            iterator i(p);
            p = p->next();
            return i;
        }

        bool operator!=(const iterator &i) const { return p != i.p; }

        Segment &operator*() { return *p; }
    };

    class const_iterator {
        const Segment *p;
    public:
        const_iterator(const Segment *s) { p = s; }

        const_iterator operator++() {
            const_iterator i(p);
            p = p->next();
            return i;
        }

        bool operator!=(const const_iterator &i) const { return p != i.p; }

        const Segment &operator*() const { return *p; }
    };

    iterator begin() { return first_; }

    iterator end() { return 0; }

    const_iterator begin() const { return first_; }

    const_iterator end() const { return 0; }

private:

    Segment *first_ = nullptr; // First item of segment list
    Segment *last_ = nullptr;  // Last item of segment list
    int size_ = 0;             // Number of items in segment list
};

// Segment* begin(SegmentList& l) { return l.first(); }
// Segment* end(SegmentList&) { return 0; }
} // namespace mu::engraving
