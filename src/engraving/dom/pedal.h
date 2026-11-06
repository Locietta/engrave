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

#ifndef MU_ENGRAVING_PEDAL_H
#define MU_ENGRAVING_PEDAL_H

#include "textlinebase.h"

namespace mu::engraving {
struct Pedal;

//---------------------------------------------------------
//   @@ PedalSegment
//---------------------------------------------------------

struct PedalSegment final : TextLineBaseSegment {
private:
    OBJECT_ALLOCATOR(engraving, PedalSegment)
    DECLARE_CLASSOF(ElementType::PEDAL_SEGMENT)

public:
    PedalSegment(Pedal *sp);

    PedalSegment *clone() const override { return new PedalSegment(*this); }

    Pedal *pedal() const { return to_pedal(spanner()); }

    friend struct Pedal;
};

//---------------------------------------------------------
//   @@ Pedal
//---------------------------------------------------------

struct Pedal final : TextLineBase {
private:
    OBJECT_ALLOCATOR(engraving, Pedal)
    DECLARE_CLASSOF(ElementType::PEDAL)

    Sid get_property_style(Pid) const override;

protected:
    Sid default_pos_sid() const override;
    PointF line_pos(Grip, System **) const override;

public:
    static const String pedal_symbol;
    static const String star_symbol;

    Pedal(DummyParentOr<EngravingItem> parent);

    Pedal *clone() const override { return new Pedal(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;
    PropertyValue property_default(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;

    Pedal *find_next_in_staff() const;
    bool connect45_hook_to_next() const;

    friend struct PedalLine;
};
} // namespace mu::engraving
#endif
