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

#ifndef MU_ENGRAVING_DURATION_H
#define MU_ENGRAVING_DURATION_H

#include "engravingitem.h"
#include "durationtype.h"

namespace mu::engraving {
struct Beam;
struct Tuplet;

//---------------------------------------------------------
//   @@ DurationElement
///    Virtual base class for Chord, Rest and Tuplet.
//
//   @P duration       Fraction  duration (as written)
//   @P globalDuration Fraction  played duration
//---------------------------------------------------------

struct DurationElement : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, DurationElement)
public:
    ~DurationElement();

    virtual Measure *measure() const { return (Measure *) (ownership_parent()); }

    void read_add_tuplet(Tuplet *t);

    void set_tuplet(Tuplet *t);

    Tuplet *tuplet() const { return tuplet_; }

    Tuplet *top_tuplet() const;

    virtual Beam *beam() const { return nullptr; }

    Fraction actual_ticks_at(const Fraction &tick) const;
    Fraction actual_ticks() const;

    Fraction end_tick() const { return tick() + actual_ticks(); }

    // Length expressed as a fraction of a whole note
    virtual Fraction ticks() const { return duration_; }

    Fraction global_ticks() const;
    float time_stretch_factor() const;

    void set_ticks(const Fraction &f) { duration_ = f; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;

protected:
    DurationElement(const ElementType &type, EngravingObject *parent = nullptr,
                    ElementFlags = ElementFlag::MOVABLE | ElementFlag::ON_STAFF);
    DurationElement(const DurationElement &e, bool link = false);

private:
    Fraction duration_;
    Tuplet *tuplet_ = nullptr;
};
} // namespace mu::engraving

#endif
