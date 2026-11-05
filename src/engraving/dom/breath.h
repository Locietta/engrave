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
class Factory;

//---------------------------------------------------------
//   BreathType
//---------------------------------------------------------

struct BreathType {
    SymId id = SymId::noSym;
    bool isCaesura = false;
    double pause = false;
};

//---------------------------------------------------------
//   @@ Breath
//!    breathType() is index in symList
//---------------------------------------------------------

class Breath final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Breath)
    DECLARE_CLASSOF(ElementType::BREATH)

public:

    Breath *clone() const override { return new Breath(*this); }

    double mag() const override;

    void setSymId(SymId id) { symId_ = id; }

    SymId symId() const { return symId_; }

    double pause() const { return pause_; }

    void setPause(double v) { pause_ = v; }

    Segment *segment() const { return (Segment *) ownershipParent(); }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    EngravingItem *nextSegmentElement() override;
    EngravingItem *prevSegmentElement() override;

    int subtype() const override { return int(symId_); }

    TranslatableString subtypeUserName() const override;

    bool isCaesura() const;

    static const std::vector<BreathType> BREATH_LIST;

private:

    friend class Factory;
    Breath(DummyParentOr<Segment> parent);

    double pause_ = 0.0;
    SymId symId_ = SymId::breathMarkComma;
};
} // namespace mu::engraving
#endif
