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

#include "arpeggio.h"
#include "engravingitem.h"

namespace mu::engraving {
class ChordBracket final : public Arpeggio {
    OBJECT_ALLOCATOR(engraving, ChordBracket)
    DECLARE_CLASSOF(ElementType::CHORD_BRACKET)

public:
    ChordBracket *clone() const override { return new ChordBracket(*this); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;

    void reset() override;

    Spatium hook_length() const { return hook_length_; }

    DirectionV hook_pos() const { return hook_pos_; }

    bool right_side() const { return right_side_; }

private:
    friend class Factory;

    ChordBracket(DummyParentOr<Chord> parent);

    Spatium hook_length_ = Spatium(1);
    DirectionV hook_pos_ = DirectionV::AUTO; // AUTO == both up and down hooks
    bool right_side_ = false;
};
} // namespace mu::engraving
