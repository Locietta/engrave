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

#ifndef MU_ENGRAVING_BREATH_H
#define MU_ENGRAVING_BREATH_H

#include "engravingitem.h"

namespace mu::engraving {
struct Factory;

//---------------------------------------------------------
//   BreathType
//---------------------------------------------------------

struct BreathType {
    SymId id = SymId::NO_SYM;
    bool is_caesura = false;
    double pause = false;
};

//---------------------------------------------------------
//   @@ Breath
//!    breathType() is index in symList
//---------------------------------------------------------

struct Breath final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Breath)
    DECLARE_CLASSOF(ElementType::BREATH)

public:

    Breath *clone() const override { return new Breath(*this); }

    double mag() const override;

    void set_sym_id(SymId id) { sym_id_ = id; }

    SymId sym_id() const { return sym_id_; }

    double pause() const { return pause_; }

    void set_pause(double v) { pause_ = v; }

    Segment *segment() const { return (Segment *) ownership_parent(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    EngravingItem *next_segment_element() override;
    EngravingItem *prev_segment_element() override;

    int subtype() const override { return int(sym_id_); }

    TranslatableString subtype_user_name() const override;

    bool is_caesura() const;

    static const std::vector<BreathType> breath_list;

private:

    friend struct Factory;
    Breath(DummyParentOr<Segment> parent);

    double pause_ = 0.0;
    SymId sym_id_ = SymId::BREATH_MARK_COMMA;
};
} // namespace mu::engraving
#endif
