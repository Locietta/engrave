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

#include "trill.h"

#include <cmath>

#include "../editing/addremoveelement.h"
#include "types/typesconv.h"

#include "iengravingfont.h"

#include "accidental.h"
#include "chord.h"
#include "clef.h"
#include "factory.h"
#include "measure.h"
#include "ornament.h"
#include "score.h"
#include "system.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   trillStyle
//---------------------------------------------------------

static const ElementStyle trillStyle{
    {Sid::trillPlacement, Pid::PLACEMENT},
};

TrillSegment::TrillSegment(Trill *sp) : LineSegment(ElementType::TRILL_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void TrillSegment::remove(EngravingItem *e) {
    if (trill()->accidental() == e) {
        // accidental is part of trill
        trill()->setAccidental(nullptr);
        e->removed();
    }
}

//---------------------------------------------------------
//   symbolLine
//---------------------------------------------------------

void TrillSegment::symbolLine(SymId start, SymId fill) {
    double x1 = 0.0;
    double x2 = pos2().x();
    double w = x2 - x1;
    double mag = magS();
    IEngravingFontPtr f = score()->engravingFont();

    symbols_.clear();
    symbols_.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    int n = lrint((w - w1) / w2);
    for (int i = 0; i < n; ++i) {
        symbols_.push_back(fill);
    }
    RectF r(f->bbox(symbols_, mag));
    setbbox(r);
}

void TrillSegment::symbolLine(SymId start, SymId fill, SymId end) {
    double x1 = 0.0;
    double x2 = pos2().x();
    double w = x2 - x1;
    double mag = magS();
    IEngravingFontPtr f = score()->engravingFont();

    symbols_.clear();
    symbols_.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    double w3 = f->advance(end, mag);
    int n = lrint((w - w1 - w3) / w2);
    for (int i = 0; i < n; ++i) {
        symbols_.push_back(fill);
    }
    symbols_.push_back(end);
    RectF r(f->bbox(symbols_, mag));
    setbbox(r);
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void TrillSegment::scanElements(std::function<void(EngravingItem *)> func) {
    func(this);
    if (isSingleType() || isBeginType()) {
        Accidental *a = trill()->accidental();
        if (a) {
            func(a);
        }
        Chord *cueNoteChord = trill()->cueNoteChord();
        if (cueNoteChord) {
            cueNoteChord->scanElements(func);
        }
    }
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *TrillSegment::propertyDelegate(Pid pid) const {
    if (pid == Pid::TRILL_TYPE || pid == Pid::ORNAMENT_STYLE || pid == Pid::PLACEMENT) {
        return spanner();
    }
    return LineSegment::propertyDelegate(pid);
}

//---------------------------------------------------------
//   Trill
//---------------------------------------------------------

Trill::Trill(DummyParentOr<EngravingItem> parent) : SLine(ElementType::TRILL, parent) {
    trillType_ = TrillType::TRILL_LINE;
    ornament_ = nullptr;
    accidental_ = nullptr;
    cueNoteChord_ = nullptr;
    ornamentStyle_ = OrnamentStyle::DEFAULT;
    initElementStyle(&trillStyle);
}

Trill::Trill(const Trill &t) : SLine(t) {
    trillType_ = t.trillType_;
    ornament_ = t.ornament_ ? t.ornament_->clone() : nullptr;
    ornamentStyle_ = t.ornamentStyle_;
    initElementStyle(&trillStyle);
}

EngravingItem *Trill::linkedClone() {
    Trill *linkedTrill = clone();
    Ornament *linkedOrnament = toOrnament(ornament_->linkedClone());
    linkedTrill->setOrnament(linkedOrnament);
    linkedTrill->setAutoplace(true);
    score()->undo(new Link(linkedTrill, this));
    return linkedTrill;
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Trill::remove(EngravingItem *e) {
    if (e == accidental_) {
        accidental_ = nullptr;
        e->removed();
    }
}

void Trill::setTrack(track_idx_t n) {
    EngravingItem::setTrack(n);

    for (SpannerSegment *ss : spannerSegments()) {
        ss->setTrack(n);
    }

    if (ornament_) {
        ornament_->setTrack(n);
    }
}

void Trill::setScore(Score *s) {
    Spanner::setScore(s);
    if (ornament_) {
        ornament_->setScore(s);
    }
}

void Trill::computeStartElement() {
    Spanner::computeStartElement();
    if (startElement() && startElement()->isChord() && ornament_) {
        ornament_->setOwnershipParent(startElement());

        Chord *cueChord = ornament_->cueNoteChord();
        if (cueChord) {
            cueChord->setOwnershipParent(toChord(startElement())->segment());
        }
    }
}

PointF Trill::trillLinePos(const SLine *line, Grip grip, System **system) {
    if (!line) {
        return PointF();
    }

    bool start = grip == Grip::START;
    bool mmRest = line->style().styleB(Sid::createMultiMeasureRests);
    double graceOffset = 0.0;
    double clefOffset = 0.0;

    Segment *segment = start ? line->startSegment() : line->endSegment();
    if (!segment) {
        return PointF();
    }

    if (start) {
        *system = segment->measure()->system();
        double x = segment->x() + segment->measure()->x();
        return PointF(x, 0.0);
    }

    Segment *clefSeg = segment->isClefType() ? segment : nullptr;
    Fraction curTick = segment->tick();
    while (true) {
        Segment *prevSeg = mmRest ? segment->prev1MM() : segment->prev1();
        if (prevSeg && prevSeg->tick() == curTick) {
            clefSeg = prevSeg->isClefType() ? prevSeg : clefSeg;
            segment = prevSeg;
        } else {
            break;
        }
    }

    // Stop line before clefs
    if (clefSeg) {
        EngravingItem *clefItem = clefSeg->element(line->track2());
        if (clefItem && clefItem->isClef()) {
            Clef *clef = toClef(clefItem);
            SymId clefSym = ClefInfo::symId(clef->clefType());
            Shape clefShape = line->symShapeWithCutouts(clefSym).translated(clef->pos());
            clefOffset = segment->pageX() - clef->pageX() + clefShape.leftMostEdgeAtTop();
        }
    }

    // Stop line before grace notes
    if (const EngravingItem *preAppendedItem = segment->preAppendedItem(line->track2());
        preAppendedItem && preAppendedItem->isGraceNotesGroup()) {
        // get x position of leftmost grace note
        const Chord *leftMostGraceChord = nullptr;
        const GraceNotesGroup *graceGroup = toGraceNotesGroup(preAppendedItem);
        for (const Chord *graceChord : *graceGroup) {
            leftMostGraceChord = leftMostGraceChord && leftMostGraceChord->x() < graceChord->x() ? leftMostGraceChord : graceChord;
        }
        if (leftMostGraceChord) {
            graceOffset = segment->pageX() - leftMostGraceChord->pageX();
        }
    }

    double offset = std::max(graceOffset, clefOffset);

    *system = segment->measure()->system();
    double x = segment->x() + segment->measure()->x() - line->spatium() - offset;
    return PointF(x, 0.0);
}

PointF Trill::linePos(Grip grip, System **system) const { return trillLinePos(this, grip, system); }

void Trill::setTrillType(TrillType tt) {
    trillType_ = tt;
    if (!ornament_) {
        // ornament parent will be explicitely set at layout stage
        ornament_ = Factory::createOrnament(score()->dummy());
    }
    ornament_->setTrack(track());
    ornament_->setSymId(Ornament::fromTrillType(tt));
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle trillSegmentStyle{
    {Sid::trillMinDistance, Pid::MIN_DISTANCE},
};

LineSegment *Trill::createLineSegment() {
    TrillSegment *seg = new TrillSegment(this);
    seg->setTrack(track());
    seg->setColor(lineColor());
    seg->initElementStyle(&trillSegmentStyle);
    return seg;
}

//---------------------------------------------------------
//   trillTypeName
//---------------------------------------------------------

String Trill::trillTypeUserName() const { return TConv::translatedUserName(trillType()); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Trill::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::TRILL_TYPE: return int(trillType());
        case Pid::ORNAMENT_STYLE: return ornamentStyle();
        default: break;
    }
    return SLine::getProperty(propertyId);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Trill::setProperty(Pid propertyId, const PropertyValue &val) {
    switch (propertyId) {
        case Pid::TRILL_TYPE: setTrillType(TrillType(val.toInt())); break;
        case Pid::ORNAMENT_STYLE: setOrnamentStyle(val.value<OrnamentStyle>()); break;
        case Pid::COLOR:
            setColor(val.value<Color>());
            setLineColor(val.value<Color>());
            break;
        default: return SLine::setProperty(propertyId, val);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Trill::propertyDefault(Pid propertyId) const {
    switch (propertyId) {
        case Pid::TRILL_TYPE: return static_cast<int>(TrillType::TRILL_LINE);
        case Pid::ORNAMENT_STYLE: return OrnamentStyle::DEFAULT;
        case Pid::PLACEMENT: return style().styleV(Sid::trillPlacement);

        default: return SLine::propertyDefault(propertyId);
    }
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Trill::subtypeUserName() const { return TConv::userName(trillType()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

Sid Trill::defaultPosSid() const { return placeAbove() ? Sid::trillPosAbove : Sid::trillPosBelow; }

void Trill::doComputeEndElement() {
    setEndElement(score()->findChordRestEndingBeforeTickInStaffAndVoice(tick2(), track2staff(track2()), voice()));
}
