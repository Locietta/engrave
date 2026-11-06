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

#ifndef MU_ENGRAVING_NOTEDOT_H
#define MU_ENGRAVING_NOTEDOT_H

#include "engravingitem.h"

namespace mu::engraving {
class Factory;
class Note;
class Rest;

//---------------------------------------------------------
//   @@ NoteDot
//---------------------------------------------------------

class NoteDot final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, NoteDot)
    DECLARE_CLASSOF(ElementType::NOTEDOT)

public:

    NoteDot *clone() const override { return new NoteDot(*this); }

    double mag() const override;

    Note *note() const { return ownership_parent()->is_note() ? to_note(ownership_parent()) : 0; }

    Rest *rest() const { return ownership_parent()->is_rest() ? to_rest(ownership_parent()) : 0; }

    EngravingItem *element_base() const override;

private:
    friend class Factory;
    NoteDot(DummyParentOr<Note> parent);
    NoteDot(Rest *parent);
};
} // namespace mu::engraving
#endif
