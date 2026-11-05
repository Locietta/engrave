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

#ifndef MU_ENGRAVING_TRILL_H
#define MU_ENGRAVING_TRILL_H

#include "line.h"

namespace mu::engraving {
class Trill;
class Accidental;

//---------------------------------------------------------
//   @@ TrillSegment
//---------------------------------------------------------

class TrillSegment final : public LineSegment {
    OBJECT_ALLOCATOR(engraving, TrillSegment)
    DECLARE_CLASSOF(ElementType::TRILL_SEGMENT)

public:
    TrillSegment(Trill *sp);

    Trill *trill() const { return (Trill *) spanner(); }

    TrillSegment *clone() const override { return new TrillSegment(*this); }

    void scanElements(std::function<void(EngravingItem *)> func) override;

    EngravingObject *propertyDelegate(Pid) const override;

    void remove(EngravingItem *) override;

    const SymIdList &symbols() const { return symbols_; }

    void setSymbols(const SymIdList &s) { symbols_ = s; }

    void symbolLine(SymId start, SymId fill);
    void symbolLine(SymId start, SymId fill, SymId end);

protected:

private:
    SymIdList symbols_;
};

//---------------------------------------------------------
//   @@ Trill
//   @P trillType  enum (Trill.DOWNPRALL_LINE, .PRALLPRALL_LINE, .PURE_LINE, .TRILL_LINE, .UPPRALL_LINE)
//---------------------------------------------------------

class Trill final : public SLine {
    OBJECT_ALLOCATOR(engraving, Trill)
    DECLARE_CLASSOF(ElementType::TRILL)

public:
    Trill(DummyParentOr<EngravingItem> parent);
    Trill(const Trill &t);

    Trill *clone() const override { return new Trill(*this); }

    EngravingItem *linkedClone() override;

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *createLineSegment() override;
    void remove(EngravingItem *) override;

    void setTrack(track_idx_t n) override;
    void setScore(Score *s) override;
    void computeStartElement() override;
    static PointF trillLinePos(const SLine *line, Grip grip, System **system);
    PointF linePos(Grip grip, System **system) const override;

    void setTrillType(TrillType tt);

    TrillType trillType() const { return trillType_; }

    int subtype() const override { return int(trillType_); }

    TranslatableString subtypeUserName() const override;

    void setOrnamentStyle(OrnamentStyle val) { ornamentStyle_ = val; }

    OrnamentStyle ornamentStyle() const { return ornamentStyle_; }

    String trillTypeUserName() const;

    Accidental *accidental() const { return accidental_; }

    void setAccidental(Accidental *a) { accidental_ = a; }

    Chord *cueNoteChord() const { return cueNoteChord_; }

    void setCueNoteChord(Chord *c) { cueNoteChord_ = c; }

    Segment *segment() const { return (Segment *) ownershipParent(); }

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    Ornament *ornament() const { return ornament_; }

    void setOrnament(Ornament *o) { ornament_ = o; }

    Sid defaultPosSid() const override;

protected:
    void doComputeEndElement() override;

private:
    TrillType trillType_ = TrillType::TRILL_LINE;
    Accidental *accidental_ = nullptr;
    Chord *cueNoteChord_ = nullptr;
    OrnamentStyle ornamentStyle_ = OrnamentStyle::DEFAULT; // for use in ornaments such as trill
    Ornament *ornament_ = nullptr;
};
} // namespace mu::engraving

#endif
