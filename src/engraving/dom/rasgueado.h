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

#ifndef MU_ENGRAVING_RASGUEADO_H
#define MU_ENGRAVING_RASGUEADO_H

#include "chordtextlinebase.h"

namespace mu::engraving {
struct Rasgueado;

//---------------------------------------------------------
//   @@ RasgueadoSegment
//---------------------------------------------------------

struct RasgueadoSegment final : TextLineBaseSegment {
private:
    OBJECT_ALLOCATOR(engraving, RasgueadoSegment)
    DECLARE_CLASSOF(ElementType::RASGUEADO_SEGMENT)

public:
    RasgueadoSegment(Rasgueado *sp);

    RasgueadoSegment *clone() const override { return new RasgueadoSegment(*this); }

    Rasgueado *rasgueado() const { return (Rasgueado *) spanner(); }

    friend struct Rasgueado;
};

//---------------------------------------------------------
//   @@ Rasgueado
//---------------------------------------------------------

struct Rasgueado final : ChordTextLineBase {
private:
    OBJECT_ALLOCATOR(engraving, Rasgueado)
    DECLARE_CLASSOF(ElementType::RASGUEADO)

public:
    Rasgueado(DummyParentOr<EngravingItem> parent);

    Rasgueado *clone() const override { return new Rasgueado(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;

    PropertyValue property_default(Pid property_id) const override;
    Sid get_property_style(Pid) const override;

protected:
    Sid default_pos_sid() const override;
};
} // namespace mu::engraving
#endif
