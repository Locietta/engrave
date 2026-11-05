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

#include "tremolotwochord.h"

#include "draw/types/transform.h"

#include "types/typesconv.h"

#include "style/style.h"

#include "rendering/score/beamtremololayout.h"

#include "chord.h"
#include "stem.h"
#include "system.h"
#include "stafftype.h"

#include "log.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   tremoloStyle
//---------------------------------------------------------

static const ElementStyle TREMOLO_STYLE{{Sid::tremoloStyle, Pid::TREMOLO_STYLE}};

//---------------------------------------------------------
//   Tremolo
//---------------------------------------------------------

TremoloTwoChord::TremoloTwoChord(DummyParentOr<Chord> parent) : BeamBase(ElementType::TREMOLO_TWOCHORD, parent, ElementFlag::MOVABLE) {
    initElementStyle(&TREMOLO_STYLE);
}

TremoloTwoChord::TremoloTwoChord(const TremoloTwoChord &t) : BeamBase(t) {
    setTremoloType(t.tremoloType());
    chord1_ = t.chord1();
    chord2_ = t.chord2();
    durationType_ = t.durationType_;
}

TremoloTwoChord::~TremoloTwoChord() {
    //
    // delete all references from chords
    //
    if (chord1_ && chord1_->tremoloTwoChord() == this) {
        chord1_->setTremoloTwoChord(nullptr);
    }
    if (chord2_ && chord2_->tremoloTwoChord() == this) {
        chord2_->setTremoloTwoChord(nullptr);
    }

    clearBeamSegments();
}

//---------------------------------------------------------
//   chordMag
//---------------------------------------------------------

double TremoloTwoChord::chordMag() const { return chord() ? chord()->intrinsicMag() : 1.0; }

//---------------------------------------------------------
//   minHeight
//---------------------------------------------------------

double TremoloTwoChord::minHeight() const {
    const double sw = style().styleS(Sid::tremoloLineWidth).val() * chordMag();
    const double td = style().styleS(Sid::tremoloDistance).val() * chordMag();
    return (lines() - 1) * td + sw;
}

//---------------------------------------------------------
//   chordBeamAnchor
//---------------------------------------------------------

PointF TremoloTwoChord::chordBeamAnchor(const ChordRest *chord, ChordBeamAnchorType anchorType) const {
    return rendering::score::BeamTremoloLayout::chordBeamAnchor(this->ldata(), chord, anchorType);
}

//---------------------------------------------------------
//   setTremoloType
//---------------------------------------------------------

void TremoloTwoChord::setTremoloType(TremoloType t) {
    IF_ASSERT_FAILED(isTremoloTwoChord(t)) { return; }
    tremoloType_ = t;
    switch (tremoloType()) {
        case TremoloType::R16:
        case TremoloType::C16: lines_ = 2; break;
        case TremoloType::R32:
        case TremoloType::C32: lines_ = 3; break;
        case TremoloType::R64:
        case TremoloType::C64: lines_ = 4; break;
        case TremoloType::R128:
        case TremoloType::C128: lines_ = 5; break;
        case TremoloType::R256:
        case TremoloType::C256: lines_ = 6; break;
        default: lines_ = 1; break;
    }

    styleChanged();
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void TremoloTwoChord::reset() {
    EngravingItem::reset();
    if (userModified()) {
        //undoChangeProperty(Pid::BEAM_POS, PropertyValue::fromValue(beamPos()));
        undoChangeProperty(Pid::USER_MODIFIED, false);
    }
    undoChangeProperty(Pid::STEM_DIRECTION, DirectionV::AUTO);
    resetProperty(Pid::BEAM_NO_SLOPE);
}

//---------------------------------------------------------
//   setBeamDirection
//---------------------------------------------------------

void TremoloTwoChord::setDirection(DirectionV d) {
    if (direction() == d) {
        return;
    }

    doSetDirection(d);

    if (d != DirectionV::AUTO) {
        setUp(d == DirectionV::UP);
    }
    if (chord1_) {
        chord1_->setStemDirection(d);
    }
    if (chord2_) {
        chord2_->setStemDirection(d);
    }
}

//---------------------------------------------------------
//   crossStaffBeamBetween
//    Return true if tremolo is two-note cross-staff and beams between staves
//---------------------------------------------------------

bool TremoloTwoChord::crossStaffBeamBetween() const {
    return ((chord1_->staffMove() > chord2_->staffMove()) && chord1_->up() && !chord2_->up()) ||
           ((chord1_->staffMove() < chord2_->staffMove()) && !chord1_->up() && chord2_->up());
}

TDuration TremoloTwoChord::durationType() const { return durationType_; }

void TremoloTwoChord::setDurationType(TDuration d) {
    if (durationType_ == d) {
        return;
    }

    durationType_ = d;
    styleChanged();
}

//---------------------------------------------------------
//   tremoloLen
//---------------------------------------------------------

Fraction TremoloTwoChord::tremoloLen() const {
    Fraction f;
    switch (lines()) {
        case 1: f.set(1, 8); break;
        case 2: f.set(1, 16); break;
        case 3: f.set(1, 32); break;
        case 4: f.set(1, 64); break;
        case 5: f.set(1, 128); break;
        case 6: f.set(1, 256); break;
    }
    return f;
}

//---------------------------------------------------------
//   setBeamPos
//---------------------------------------------------------

void TremoloTwoChord::setBeamPos(const PairF &bp) {
    int idx = directionIdx();
    setUserModified(true);
    setGenerated(false);

    double _spatium = spatium();
    beamFragment_.py1[idx] = bp.first * _spatium;
    beamFragment_.py2[idx] = bp.second * _spatium;
}

//---------------------------------------------------------
//   beamPos
//---------------------------------------------------------

PairF TremoloTwoChord::beamPos() const {
    int idx = directionIdx();
    double _spatium = spatium();
    return PairF(beamFragment_.py1[idx] / _spatium, beamFragment_.py2[idx] / _spatium);
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void TremoloTwoChord::triggerLayout() const {
    if (chord1_ && chord2_) {
        toChordRest(chord1_)->triggerLayout();
        toChordRest(chord2_)->triggerLayout();
    } else {
        EngravingItem::triggerLayout();
    }
}

//---------------------------------------------------------
//   subtypeName
//---------------------------------------------------------

TranslatableString TremoloTwoChord::subtypeUserName() const { return TConv::userName(tremoloType()); }

//---------------------------------------------------------
//   customStyleApplicable
//---------------------------------------------------------

bool TremoloTwoChord::customStyleApplicable() const {
    return (durationType().type() == DurationType::V_HALF) && (staffType()->group() != StaffGroup::TAB);
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TremoloTwoChord::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::TREMOLO_TYPE: return int(tremoloType_);
        case Pid::TREMOLO_STYLE: return int(style_);
        case Pid::PLAY: return playTremolo_;
        default: break;
    }
    return BeamBase::getProperty(propertyId);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TremoloTwoChord::setProperty(Pid propertyId, const PropertyValue &val) {
    switch (propertyId) {
        case Pid::TREMOLO_TYPE: setTremoloType(TremoloType(val.toInt())); break;
        case Pid::TREMOLO_STYLE:
            if (customStyleApplicable()) {
                setTremoloStyle(TremoloStyle(val.toInt()));
            }
            break;
        case Pid::BEAM_POS:
            if (userModified()) {
                setBeamPos(val.value<PairF>());
            }
            break;
        case Pid::PLAY: setPlayTremolo(val.toBool()); break;
        default: return BeamBase::setProperty(propertyId, val);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TremoloTwoChord::propertyDefault(Pid propertyId) const {
    switch (propertyId) {
        case Pid::TREMOLO_STYLE: return style().styleI(Sid::tremoloStyle);
        case Pid::PLAY: return true;
        default: return BeamBase::propertyDefault(propertyId);
    }
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void TremoloTwoChord::scanElements(std::function<void(EngravingItem *)> func) {
    if (chord() && chord()->tremoloChordType() == TremoloChordType::TremoloSecondChord) {
        return;
    }
    EngravingItem::scanElements(func);
}

void TremoloTwoChord::clearBeamSegments() {
    BeamSegment *chord1Segment = chord1_ ? chord1_->beamlet() : nullptr;
    BeamSegment *chord2Segment = chord2_ ? chord2_->beamlet() : nullptr;

    if (chord1Segment || chord2Segment) {
        for (BeamSegment *segment : beamSegments_) {
            if (chord1Segment && chord1Segment == segment) {
                chord1_->setBeamlet(nullptr);
            } else if (chord2Segment && chord2Segment == segment) {
                chord2_->setBeamlet(nullptr);
            }
        }
    }

    BeamBase::clearBeamSegments();
}

int TremoloTwoChord::maxCRMove() const { return std::max(chord1_->staffMove(), chord2_->staffMove()); }

int TremoloTwoChord::minCRMove() const { return std::min(chord1_->staffMove(), chord2_->staffMove()); }

// used for palettes
PainterPath TremoloTwoChord::basePath(double stretch) const {
    bool tradAlternate = style_ == TremoloStyle::TRADITIONAL_ALTERNATE;
    if (tradAlternate && muse::RealIsEqual(stretch, 0.)) {
        // this shape will have to be constructed after the stretch
        // is known
        return PainterPath();
    }

    // TODO: This should be a style setting, to replace tremoloStrokeLengthMultiplier
    static constexpr Spatium stemGapSp = 0.65_sp;

    const double sp = spatium() * chordMag();

    // overall width of two-note tremolos should not be changed if chordMag() isn't 1.0
    double w2 = sp * style().styleS(Sid::tremoloWidth).val() * .5 / chordMag();
    double lw = sp * style().styleS(Sid::tremoloLineWidth).val();
    double td = sp * style().styleS(Sid::tremoloDistance).val();

    PainterPath ppath;

    // first line
    ppath.addRect(-w2, 0.0, 2.0 * w2, lw);
    double ty = td;

    // other lines
    for (int i = 1; i < lines_; i++) {
        if (tradAlternate) {
            double stemWidth1 = chord1_->stem()->lineWidthMag() / stretch;
            double stemWidth2 = chord2_->stem()->lineWidthMag() / stretch;
            double inset = stemGapSp.toAbsolute(spatium()) / stretch;

            ppath.addRect(-w2 + inset + stemWidth1, ty, 2.0 * w2 - (inset * 2.) - (stemWidth2 + stemWidth1), lw);
        } else {
            ppath.addRect(-w2, ty, 2.0 * w2, lw);
        }
        ty += td;
    }

    if (!chord()) {
        // for the palette or for one-note tremolos
        Transform shearTransform;
        shearTransform.shear(0.0, -(lw / 2.0) / w2);
        ppath = shearTransform.map(ppath);
    }

    return ppath;
}

void TremoloTwoChord::computeShape() {
    path_ = basePath();
    setbbox(path_.boundingRect());
}
} // namespace mu::engraving
