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

#include "beambase.h"
#include "engravingitem.h"

#include "durationtype.h"
#include "draw/types/painterpath.h"
#include "../types/types.h"
#include "beam.h"
#include "chord.h"

namespace mu::engraving {
class Chord;

class TremoloTwoChord final : public BeamBase {
    OBJECT_ALLOCATOR(engraving, TremoloTwoChord)
    DECLARE_CLASSOF(ElementType::TREMOLO_TWOCHORD)

public:

    TremoloTwoChord &operator=(const TremoloTwoChord &) = delete;

    TremoloTwoChord *clone() const override { return new TremoloTwoChord(*this); }

    ~TremoloTwoChord() override;

    Chord *chord() const { return toChord(ownershipParent()); }

    int subtype() const override { return static_cast<int>(tremoloType_); }

    TranslatableString subtypeUserName() const override;

    void scanElements(std::function<void(EngravingItem *)> func) override;

    void setTremoloType(TremoloType t);

    TremoloType tremoloType() const { return tremoloType_; }

    double minHeight() const;
    void reset() override;

    PointF chordBeamAnchor(const ChordRest *chord, ChordBeamAnchorType anchorType) const;

    double chordMag() const;

    Chord *chord1() const { return chord1_; }

    void setChord1(Chord *ch) { chord1_ = ch; }

    Chord *chord2() const { return chord2_; }

    void setChord2(Chord *ch) { chord2_ = ch; }

    void setChords(Chord *c1, Chord *c2) {
        chord1_ = c1;
        chord2_ = c2;
    }

    PairF beamPos() const;

    TDuration durationType() const;
    void setDurationType(TDuration d);

    Fraction tremoloLen() const;

    int lines() const { return lines_; }

    bool crossStaffBeamBetween() const;

    void triggerLayout() const override;

    TremoloStyle tremoloStyle() const { return style_; }

    void setTremoloStyle(TremoloStyle v) { style_ = v; }

    void setDirection(DirectionV v) override;

    void setBeamFragment(const BeamFragment &bf) { beamFragment_ = bf; }

    const BeamFragment &beamFragment() const { return beamFragment_; }

    BeamFragment &beamFragment() { return beamFragment_; }

    bool playTremolo() const { return playTremolo_; }

    void setPlayTremolo(bool v) { playTremolo_ = v; }

    bool customStyleApplicable() const;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid propertyId) const override;

    bool isMovable() const override { return true; }

    void clearBeamSegments() override;

    int maxCRMove() const override;
    int minCRMove() const override;

    //! NOTE for palettes
    muse::draw::PainterPath basePath(double stretch = 0) const;

    const muse::draw::PainterPath &path() const { return path_; }

    void setPath(const muse::draw::PainterPath &p) { path_ = p; }

    void computeShape();
    //! -----------------

private:
    friend class Factory;

    TremoloTwoChord(DummyParentOr<Chord> parent);
    TremoloTwoChord(const TremoloTwoChord &);

    void setBeamPos(const PairF &bp);

    TremoloType tremoloType_ = TremoloType::INVALID_TREMOLO;
    Chord *chord1_ = nullptr;
    Chord *chord2_ = nullptr;
    TDuration durationType_;
    bool playTremolo_ = true;

    int lines_ = 0; // derived from _subtype
    TremoloStyle style_ = TremoloStyle::DEFAULT;
    // for _startAnchor and _slope, once we decide to change trems so that they can
    // continue from one system to the other (or indeed, one measure to the other)
    // we will want a vector of fragments similar to Beam's _beamFragments structure.
    // for now, a single fragment is sufficient
    BeamFragment beamFragment_;

    //! NOTE for palette
    PainterPath path_;
};
} // namespace mu::engraving
