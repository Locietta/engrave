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

#include "engravingitem.h"
#include "draw/types/painterpath.h"

namespace mu::engraving {
class Factory;

//---------------------------------------------------------
//   SpacerType
//---------------------------------------------------------

enum class SpacerType : char { UP, DOWN, FIXED };

//-------------------------------------------------------------------
//   @@ Spacer
///    Vertical spacer element to adjust the distance of staves.
//-------------------------------------------------------------------

class Spacer final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Spacer)
    DECLARE_CLASSOF(ElementType::SPACER)

public:

    Spacer *clone() const override { return new Spacer(*this); }

    Measure *measure() const { return to_measure(ownership_parent()); }

    SpacerType spacer_type() const { return spacer_type_; }

    void set_spacer_type(SpacerType t) { spacer_type_ = t; }

    int subtype() const override { return int(spacer_type_); }

    TranslatableString subtype_user_name() const override;

    void set_gap(Spatium sp);

    Spatium gap() const { return gap_; }

    double absolute_gap() const { return absolute_from_spatium(gap_); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;
    void trigger_layout() const override;

    struct LayoutData : public EngravingItem::LayoutData {
        PainterPath path;
    };
    DECLARE_LAYOUTDATA_METHODS(Spacer)

private:

    friend class Factory;
    Spacer(DummyParentOr<Measure> parent);
    Spacer(const Spacer &);

    SpacerType spacer_type_ = SpacerType::UP;
    Spatium gap_;
};
} // namespace mu::engraving
