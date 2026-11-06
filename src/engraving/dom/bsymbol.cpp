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

#include "bsymbol.h"

#include <cmath>

#include "containers.h"

#include "measure.h"
#include "page.h"
#include "score.h"
#include "segment.h"
#include "system.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   BSymbol
//---------------------------------------------------------

BSymbol::BSymbol(const ElementType &type, EngravingObject *parent, ElementFlags f) : EngravingItem(type, parent, f) {
    align_ = {AlignH::LEFT, AlignV::BASELINE};
}

BSymbol::BSymbol(const BSymbol &s) : EngravingItem(s) {
    align_ = s.align_;
    for (EngravingItem *e : s.leafs_) {
        EngravingItem *ee = e->clone();
        ee->set_ownership_parent(this);
        leafs_.push_back(ee);
    }
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void BSymbol::add(EngravingItem *e) {
    if (e->is_symbol() || e->is_image()) {
        e->set_ownership_parent(this);
        e->set_track(track());
        leafs_.push_back(e);
        to_b_symbol(e)->set_z(z() - 1); // draw on top of parent
        e->added();
    } else {
        LOGD("BSymbol::add: unsupported type %s", e->type_name());
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void BSymbol::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);
    for (EngravingItem *e : leafs_) {
        e->scan_elements(func);
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void BSymbol::remove(EngravingItem *e) {
    if (e->is_symbol() || e->is_image()) {
        if (muse::remove(leafs_, e)) {
            e->removed();
        } else {
            LOGD("BSymbol::remove: element <%s> not found", e->type_name());
        }
    } else {
        LOGD("BSymbol::remove: unsupported type %s", e->type_name());
    }
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool BSymbol::accept_drop(EditData &data) const { return data.drop_element->is_symbol() || data.drop_element->is_image(); }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *BSymbol::drop(Transaction &, EditData &data) {
    EngravingItem *el = data.drop_element;
    if (el->is_symbol() || el->is_image()) {
        el->set_ownership_parent(this);
        PointF p = data.pos - page_bounding_rect().top_left() - data.drag_offset;
        if (p == PointF()) {
            // offset position so newly added child is visible
            p = PointF(width(), height());
        }
        el->set_offset(p);
        score()->undo_add_element(el);
        return el;
    } else {
        delete el;
    }
    return 0;
}

//---------------------------------------------------------
//   pagePos
//---------------------------------------------------------

PointF BSymbol::page_pos() const {
    if (ownership_parent() && (ownership_parent()->is_segment())) {
        PointF p(pos());
        System *system = segment()->measure()->system();
        if (system) {
            p.ry() += system->staff(staff_idx())->y() + system->y();
        }
        p.rx() = page_x();
        return p;
    } else {
        return EngravingItem::page_pos();
    }
}

//---------------------------------------------------------
//   canvasPos
//---------------------------------------------------------

PointF BSymbol::canvas_pos() const {
    if (ownership_parent() && (ownership_parent()->is_segment())) {
        PointF p(pos());
        Segment *s = to_segment(ownership_parent());

        System *system = s->measure()->system();
        if (system) {
            staff_idx_t si = staff_idx();
            p.ry() += system->staff(si)->y() + system->y();
            Page *page = system->page();
            if (page) {
                p.ry() += page->y();
            }
        }
        p.rx() = canvas_x();
        return p;
    } else {
        return EngravingItem::canvas_pos();
    }
}
} // namespace mu::engraving
