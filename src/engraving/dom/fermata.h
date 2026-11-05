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

#ifndef MU_ENGRAVING_FERMATA_H
#define MU_ENGRAVING_FERMATA_H

#include "../types/types.h"

#include "engravingitem.h"

namespace mu::engraving {
class ChordRest;
class Factory;
class Measure;
class Page;
class Segment;
class System;

//---------------------------------------------------------
//    Fermata
//---------------------------------------------------------

class Fermata final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Fermata)
    DECLARE_CLASSOF(ElementType::FERMATA)

public:

    Fermata(const Fermata &) = default;
    Fermata &operator=(const Fermata &) = delete;

    Fermata *clone() const override { return new Fermata(*this); }

    double mag() const override;

    SymId symId() const { return symId_; }

    void setSymId(SymId id) { symId_ = id; }

    void setSymIdAndTimeStretch(SymId id);
    FermataType fermataType() const;
    int subtype() const override;
    TranslatableString subtypeUserName() const override;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;
    void resetProperty(Pid id) override;

    Segment *segment() const { return toSegment(ownershipParent()); }

    Measure *measure() const;
    System *system() const;
    Page *page() const;

    double timeStretch() const { return timeStretch_; }

    void setTimeStretch(double val) { timeStretch_ = val; }

    bool play() const { return play_; }

    void setPlay(bool val) { play_ = val; }

    Sid defaultPosSid() const override;

private:

    friend class Factory;
    Fermata(EngravingObject *parent);

    SymId symId_ = SymId::noSym;
    double timeStretch_ = -1.0;
    bool play_ = true;
};
} // namespace mu::engraving
#endif
