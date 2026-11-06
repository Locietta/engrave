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

#ifndef MU_ENGRAVING_HOOK_H
#define MU_ENGRAVING_HOOK_H

#include "global/allocator.h"
#include "symbol.h"

namespace mu::engraving {
struct Chord;

struct Hook final : Symbol {
private:
    OBJECT_ALLOCATOR(engraving, Hook)
    DECLARE_CLASSOF(ElementType::HOOK)

public:
    Hook(DummyParentOr<Chord> parent);

    Hook *clone() const override { return new Hook(*this); }

    double mag() const override { return layout_parent()->mag(); }

    EngravingItem *element_base() const override;

    void set_hook_type(int v);

    int hook_type() const { return hook_type_; }

    Chord *chord() const { return to_chord(ownership_parent()); }

    PointF smufl_anchor() const;

    //! @p index: the number of flags (positive: upwards, negative: downwards)
    //! @p straight: whether to use straight flags
    static SymId sym_id_for_hook_index(int index, bool straight);

private:
    int hook_type_ = 0;
};
} // namespace mu::engraving
#endif
