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

#include "durationelement.h"

#include "property.h"
#include "score.h"
#include "staff.h"
#include "tuplet.h"

#include "editing/transaction/undostack.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   DurationElement
//---------------------------------------------------------

DurationElement::DurationElement(const ElementType &type, EngravingObject *parent, ElementFlags f) : EngravingItem(type, parent, f) {
    tuplet_ = 0;
}

//---------------------------------------------------------
//   DurationElement
//---------------------------------------------------------

DurationElement::DurationElement(const DurationElement &e, bool link) : EngravingItem(e, link) {
    tuplet_ = 0; // e._tuplet;
    duration_ = e.duration_;
}

//---------------------------------------------------------
//   ~DurationElement
//---------------------------------------------------------

DurationElement::~DurationElement() {
    if (tuplet_) {
        set_tuplet(nullptr);
    }
}

//---------------------------------------------------------
//   topTuplet
//---------------------------------------------------------

Tuplet *DurationElement::top_tuplet() const {
    Tuplet *t = tuplet();
    if (t) {
        while (t->tuplet()) {
            t = t->tuplet();
        }
    }
    return t;
}

//---------------------------------------------------------
//   globalTicks
//---------------------------------------------------------

Fraction DurationElement::global_ticks() const {
    Fraction f(duration_);
    for (Tuplet *t = tuplet(); t; t = t->tuplet()) {
        f /= t->ratio();
    }
    return f;
}

float DurationElement::time_stretch_factor() const {
    int nominal_duration = duration_.ticks();
    int actual_duration = actual_ticks().ticks();

    return actual_duration / static_cast<float>(nominal_duration);
}

//---------------------------------------------------------
//   actualTicks
//---------------------------------------------------------

Fraction DurationElement::actual_ticks_at(const Fraction &tick) const {
    // Use when tick() is unreliable, for example when pasting
    return global_ticks() / staff()->time_stretch(tick);
}

Fraction DurationElement::actual_ticks() const { return actual_ticks_at(tick()); }

//---------------------------------------------------------
//   readAddTuplet
//---------------------------------------------------------

void DurationElement::read_add_tuplet(Tuplet *t) {
    set_tuplet(t);
    if (!score()->undo_stack()->has_active_transaction()) { // HACK, also added in Undo::AddElement()
        t->add(this);
    }
}

void DurationElement::set_tuplet(Tuplet *t) {
    if (tuplet_) {
        tuplet_->remove_duration_element(this);
    }

    tuplet_ = t;

    if (tuplet_) {
        tuplet_->add_duration_element(this);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue DurationElement::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::DURATION: return PropertyValue::from_value(duration_);
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool DurationElement::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::DURATION: {
            Fraction f(v.value<Fraction>());
            set_ticks(f);
            trigger_layout();
        } break;
        default: return EngravingItem::set_property(property_id, v);
    }
    return true;
}
} // namespace mu::engraving
