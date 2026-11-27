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

#include "page.h"

#include "factory.h"
#include "global/hashmap.h"
#include "measurebase.h"
#include "mscore.h"
#include "rootitem.h"
#include "score.h"
#include "system.h"
#include "text.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   Page
//---------------------------------------------------------

Page::Page(Score *parent) : EngravingItem(ElementType::PAGE, parent, ElementFlag::NOT_SELECTABLE) {}

void Page::set_ownership_parent(Score *score) { EngravingItem::set_ownership_parent(score); }

EngravingItem *Page::layout_parent() const {
    // A page is the top of the visual hierarchy: it is not laid out inside anything.
    return nullptr;
}

Page::~Page() {
    muse::delete_all(header_texts_);
    muse::delete_all(footer_texts_);
    for (System *s : systems_) {
        if (s->page() == this) {
            s->set_page(nullptr);
        }
    }
}

MeasureBase *Page::first_measure_base() const {
    if (systems_.empty()) {
        return nullptr;
    }
    System *first_sys = systems_.front();
    return first_sys ? first_sys->first() : nullptr;
}

MeasureBase *Page::last_measure_base() const {
    if (systems_.empty()) {
        return nullptr;
    }
    System *last_sys = systems_.back();
    return last_sys ? last_sys->last() : nullptr;
}

//---------------------------------------------------------
//   items
//---------------------------------------------------------

std::vector<EngravingItem *> Page::items(const RectF &rect) {
    std::vector<EngravingItem *> result;
    muse::HashSet<EngravingItem *> seen;
    scan_elements([&](EngravingItem *item) {
        if (item->collect_for_drawing() && item->page_bounding_rect().intersects(rect) && seen.insert(item).second) {
            result.push_back(item);
        }
    });
    return result;
}

//---------------------------------------------------------
//   appendSystem
//---------------------------------------------------------

void Page::append_system(System *s) {
    s->set_page(this);
    systems_.push_back(s);
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Page::scan_elements(muse::FnRef<void(EngravingItem *)> func) {
    for (System *s : systems_) {
        for (MeasureBase *m : s->measures()) {
            m->scan_elements(func);
        }
        s->scan_elements(func);
    }

    func(this);

    for (int i = 0; i < ::mu::engraving::max_headers; i++) {
        if (Text *t = header_text(i)) {
            func(t);
        }
    }
    for (int i = 0; i < ::mu::engraving::max_footers; i++) {
        if (Text *t = footer_text(i)) {
            func(t);
        }
    }
}

//---------------------------------------------------------
//   getDisplayPageNumber
//---------------------------------------------------------

int Page::get_display_page_number() const { return static_cast<int>(page_number_) + 1 + score()->page_number_offset(); }

//---------------------------------------------------------
//   isOdd
//---------------------------------------------------------

bool Page::is_odd() const { return get_display_page_number() & 1; }

//---------------------------------------------------------
//   elements
//---------------------------------------------------------

std::vector<EngravingItem *> Page::elements() const { return get_children(false); }

//---------------------------------------------------------
//   tm
//---------------------------------------------------------

double Page::tm() const {
    return ((!style().style_b(Sid::PAGE_TWOSIDED) || is_odd()) ? style().style_d(Sid::PAGE_ODD_TOP_MARGIN) :
                                                                 style().style_d(Sid::PAGE_EVEN_TOP_MARGIN)) *
           ::mu::engraving::dpi;
}

//---------------------------------------------------------
//   bm
//---------------------------------------------------------

double Page::bm() const {
    return ((!style().style_b(Sid::PAGE_TWOSIDED) || is_odd()) ? style().style_d(Sid::PAGE_ODD_BOTTOM_MARGIN) :
                                                                 style().style_d(Sid::PAGE_EVEN_BOTTOM_MARGIN)) *
           ::mu::engraving::dpi;
}

//---------------------------------------------------------
//   lm
//---------------------------------------------------------

double Page::lm() const {
    return ((!style().style_b(Sid::PAGE_TWOSIDED) || is_odd()) ? style().style_d(Sid::PAGE_ODD_LEFT_MARGIN) :
                                                                 style().style_d(Sid::PAGE_EVEN_LEFT_MARGIN)) *
           ::mu::engraving::dpi;
}

//---------------------------------------------------------
//   rm
//---------------------------------------------------------

double Page::rm() const {
    return (style().style_d(Sid::PAGE_WIDTH) - style().style_d(Sid::PAGE_PRINTABLE_WIDTH)) * ::mu::engraving::dpi - lm();
}

//---------------------------------------------------------
//   tbbox
//    calculates and returns smallest rectangle containing all (visible) page elements
//---------------------------------------------------------

RectF Page::tbbox() const {
    double x1 = width();
    double x2 = 0.0;
    double y1 = height();
    double y2 = 0.0;
    const std::vector<EngravingItem *> el = elements();
    for (EngravingItem *e : el) {
        if (e == this || !e->is_printable()) {
            continue;
        }
        RectF ebbox = e->page_bounding_rect();
        if (ebbox.left() < x1) {
            x1 = ebbox.left();
        }
        if (ebbox.right() > x2) {
            x2 = ebbox.right();
        }
        if (ebbox.top() < y1) {
            y1 = ebbox.top();
        }
        if (ebbox.bottom() > y2) {
            y2 = ebbox.bottom();
        }
    }
    if (x1 < x2 && y1 < y2) {
        return RectF(x1, y1, x2 - x1, y2 - y1);
    } else {
        return page_bounding_rect();
    }
}

//---------------------------------------------------------
//   endTick
//---------------------------------------------------------

Fraction Page::end_tick() const { return systems_.empty() ? Fraction(-1, 1) : systems_.back()->measures().back()->end_tick(); }

Measure *Page::first_measure() const {
    for (System *s : systems_) {
        if (Measure *m = s->first_measure()) {
            return m;
        }
    }

    return nullptr;
}

bool Page::is_locked() const {
    MeasureBase *first_measure = first_measure_base();
    return first_measure ? first_measure->is_start_of_page_lock() : false;
}

const RangeLock *Page::page_lock() const {
    MeasureBase *first_measure = first_measure_base();
    return first_measure ? first_measure->page_lock() : nullptr;
}
