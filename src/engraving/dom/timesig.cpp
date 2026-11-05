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

#include "timesig.h"

#include <functional>

#include "style/style.h"

#include "../editing/edittimesig.h"
#include "../editing/transaction/transaction.h"

#include "measure.h"
#include "score.h"
#include "segment.h"
#include "staff.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle tsStyle{
    {Sid::timeSigNormalScale, Pid::SCALE},
};

//---------------------------------------------------------
//   TimeSig
//    Constructs an invalid time signature element.
//    After construction first call setTrack() then
//    call setSig().
//    Layout() is static and called in setSig().
//---------------------------------------------------------

TimeSig::TimeSig(DummyParentOr<Segment> parent)
    : EngravingItem(ElementType::TIMESIG, parent, ElementFlag::ON_STAFF | ElementFlag::MOVABLE | ElementFlag::PLACE_ABOVE) {
    initElementStyle(&tsStyle);

    showCourtesySig_ = true;
    stretch_.set(1, 1);
    sig_.set(0, 1); // initialize to invalid
    timeSigType_ = TimeSigType::NORMAL;
    largeParentheses_ = false;
    setMinDistance(0.5_sp); // TODO: style
}

void TimeSig::setOwnershipParent(DummyParentOr<Segment> parent) { EngravingItem::setOwnershipParent(parent); }

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double TimeSig::mag() const { return timeSigPlacement() == TimeSigPlacement::NORMAL && staff() ? staff()->staffMag(this) : 1.0; }

//---------------------------------------------------------
//   setSig
//    custom text has to be set after setSig()
//---------------------------------------------------------

void TimeSig::setSig(const Fraction &f, TimeSigType st) {
    sig_ = f;
    timeSigType_ = st;
    largeParentheses_ = false;
    numeratorString_.clear();
    denominatorString_.clear();
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool TimeSig::acceptDrop(EditData &data) const { return data.dropElement->isTimeSig(); }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *TimeSig::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.dropElement;
    if (e->isTimeSig()) {
        // change timesig applies to all staves, can't simply set subtype
        // for this one only
        // ownership of e is transferred to EditTimeSig::addTimeSig

        if (tick() != measure()->endTick()) {
            EditTimeSig::addTimeSig(tx, score(), measure(), staffIdx(), toTimeSig(e), false);
            return nullptr;
        }

        // This is a timesig at the end of a measure.
        if (*toTimeSig(e) == *this) {
            delete e;
            return nullptr;
        }

        if (!measure()->nextMeasure()) {
            return nullptr;
        }

        // Apply change to next measure
        EditTimeSig::addTimeSig(tx, score(), measure()->nextMeasure(), staffIdx(), toTimeSig(e), false);
        return nullptr;
    }
    delete e;
    return nullptr;
}

//---------------------------------------------------------
//   setNumeratorString
//    setSig() has to be called first
//---------------------------------------------------------

void TimeSig::setNumeratorString(const String &a) {
    if (timeSigType_ == TimeSigType::NORMAL) {
        numeratorString_ = a;
    }
}

//---------------------------------------------------------
//   setDenominatorString
//    setSig() has to be called first
//---------------------------------------------------------

void TimeSig::setDenominatorString(const String &a) {
    if (timeSigType_ == TimeSigType::NORMAL) {
        denominatorString_ = a;
    }
}

//---------------------------------------------------------
//   setFrom
//---------------------------------------------------------

void TimeSig::setFrom(const TimeSig *ts) {
    timeSigType_ = ts->timeSigType();
    numeratorString_ = ts->numeratorString_;
    denominatorString_ = ts->denominatorString_;
    sig_ = ts->sig_;
    stretch_ = ts->stretch_;
}

//---------------------------------------------------------
//   ssig
//---------------------------------------------------------

String TimeSig::ssig() const { return String(u"%1/%2").arg(sig_.numerator()).arg(sig_.denominator()); }

//---------------------------------------------------------
//   setSSig
//---------------------------------------------------------

void TimeSig::setSSig(const String &s) {
    StringList sl = s.split(u'/');
    if (sl.size() == 2) {
        sig_.setNumerator(sl[0].toInt());
        sig_.setDenominator(sl[1].toInt());
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue TimeSig::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::SHOW_COURTESY: return showCourtesySig();
        case Pid::NUMERATOR_STRING: return numeratorString();
        case Pid::DENOMINATOR_STRING: return denominatorString();
        case Pid::GROUP_NODES: return groups().nodes();
        case Pid::TIMESIG: return PropertyValue::fromValue(sig_);
        case Pid::TIMESIG_STRETCH: return PropertyValue::fromValue(stretch());
        case Pid::TIMESIG_TYPE: return int(timeSigType_);
        case Pid::SCALE: return scale_;
        case Pid::IS_COURTESY: return _isCourtesy;

        default: return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool TimeSig::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::SHOW_COURTESY:
            if (generated()) {
                return false;
            }
            setShowCourtesySig(v.toBool());
            break;
        case Pid::NUMERATOR_STRING: setNumeratorString(v.value<String>()); break;
        case Pid::DENOMINATOR_STRING: setDenominatorString(v.value<String>()); break;
        case Pid::GROUP_NODES: setGroups(v.value<GroupNodes>()); break;
        case Pid::TIMESIG: setSig(v.value<Fraction>()); break;
        case Pid::TIMESIG_STRETCH: setStretch(v.value<Fraction>()); break;
        case Pid::TIMESIG_TYPE: timeSigType_ = (TimeSigType) (v.toInt()); break;
        case Pid::SCALE: scale_ = v.value<ScaleF>(); break;
        case Pid::IS_COURTESY: _isCourtesy = v.toBool(); break;
        default:
            if (!EngravingItem::setProperty(propertyId, v)) {
                return false;
            }
            break;
    }
    triggerLayoutAll(); // TODO
    setGenerated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TimeSig::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::SHOW_COURTESY: return true;
        case Pid::NUMERATOR_STRING: return String();
        case Pid::DENOMINATOR_STRING: return String();
        case Pid::TIMESIG: return PropertyValue::fromValue(Fraction(4, 4));
        case Pid::TIMESIG_TYPE: return int(TimeSigType::NORMAL);
        case Pid::PLACEMENT: return PlacementV::ABOVE;
        case Pid::IS_COURTESY: return false;
        default: return EngravingItem::propertyDefault(id);
    }
}

PointF TimeSig::staffOffset() const {
    const Segment *seg = segment();
    const Measure *meas = seg ? seg->measure() : nullptr;
    const Fraction tsTick = meas ? meas->tick() : tick();
    const StaffType *st = staff()->constStaffType(tsTick);
    const double yOffset = st ? st->yoffset().val() * spatium() : 0.0;
    return PointF(0.0, yOffset);
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *TimeSig::nextSegmentElement() { return segment()->firstInNextSegments(staffIdx()); }

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *TimeSig::prevSegmentElement() { return segment()->lastInPrevSegments(staffIdx()); }

//---------------------------------------------------------
//   subtype
//---------------------------------------------------------

int TimeSig::subtype() const {
    size_t h1 = std::hash<int>{}(numerator());
    size_t h2 = std::hash<int>{}(denominator());
    size_t h3 = std::hash<TimeSigType>{}(timeSigType());

    return static_cast<int>(h1 ^ (h2 << 1) ^ (h3 << 2));
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString TimeSig::subtypeUserName() const {
    switch (timeSigType()) {
        case TimeSigType::FOUR_FOUR: return TranslatableString("engraving/timesig", "Common time");
        case TimeSigType::ALLA_BREVE: return TranslatableString("engraving/timesig", "Cut time");
        case TimeSigType::CUT_BACH: return TranslatableString("engraving/timesig", "Cut time (Bach)");
        case TimeSigType::CUT_TRIPLE: return TranslatableString("engraving/timesig", "Cut triple time (9/8)");
        default: return TranslatableString("engraving/timesig", "%1/%2 time").arg(numerator(), denominator());
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

void TimeSig::initElementStyle(const ElementStyle *elementStype) {
    EngravingItem::initElementStyle(elementStype);

    scale_ = propertyDefault(Pid::SCALE).value<ScaleF>();
}

void TimeSig::styleChanged() {
    if (isStyled(Pid::SCALE)) {
        scale_ = propertyDefault(Pid::SCALE).value<ScaleF>();
    }
    EngravingItem::styleChanged();
}

Sid TimeSig::getPropertyStyle(Pid id) const {
    if (id == Pid::SCALE) {
        switch (timeSigPlacement()) {
            case TimeSigPlacement::NORMAL: return Sid::timeSigNormalScale;
            case TimeSigPlacement::ABOVE_STAVES: return Sid::timeSigAboveScale;
            case TimeSigPlacement::ACROSS_STAVES: return Sid::timeSigAcrossScale;
            default: return Sid::NOSTYLE;
        }
    }

    return EngravingItem::getPropertyStyle(id);
}

TimeSigPlacement TimeSig::timeSigPlacement() const { return style().styleV(Sid::timeSigPlacement).value<TimeSigPlacement>(); }

TimeSigStyle TimeSig::timeSigStyle() const {
    switch (timeSigPlacement()) {
        case TimeSigPlacement::NORMAL: return style().styleV(Sid::timeSigNormalStyle).value<TimeSigStyle>();
        case TimeSigPlacement::ABOVE_STAVES: return style().styleV(Sid::timeSigAboveStyle).value<TimeSigStyle>();
        case TimeSigPlacement::ACROSS_STAVES: return style().styleV(Sid::timeSigAcrossStyle).value<TimeSigStyle>();
        default: return TimeSigStyle::NORMAL;
    }
}

double TimeSig::numDist() const {
    switch (timeSigPlacement()) {
        case TimeSigPlacement::NORMAL: return style().styleAbsolute(Sid::timeSigNormalNumDist);
        case TimeSigPlacement::ABOVE_STAVES: return style().styleAbsolute(Sid::timeSigAboveNumDist);
        case TimeSigPlacement::ACROSS_STAVES: return style().styleAbsolute(Sid::timeSigAcrossNumDist);
        default: return 0.0;
    }
}

double TimeSig::yPos() const {
    switch (timeSigPlacement()) {
        case TimeSigPlacement::NORMAL: return style().styleAbsolute(Sid::timeSigNormalY);
        case TimeSigPlacement::ABOVE_STAVES:
            return (staff()->hasSystemObjectsBelowBottomStaff() ? -1.0 : 1.0) * style().styleAbsolute(Sid::timeSigAboveY);
        case TimeSigPlacement::ACROSS_STAVES: return style().styleAbsolute(Sid::timeSigAcrossY);
        default: return 0.0;
    }
}

bool TimeSig::showOnThisStaff() const {
    return timeSigPlacement() == TimeSigPlacement::NORMAL || staffIdx() == 0 || staff()->isSystemObjectStaff();
}

bool TimeSig::isAboveStaves() const { return timeSigPlacement() == TimeSigPlacement::ABOVE_STAVES; }

bool TimeSig::isAcrossStaves() const { return timeSigPlacement() == TimeSigPlacement::ACROSS_STAVES; }

//---------------------------------------------------------
//   operator==
//---------------------------------------------------------

bool TimeSig::operator==(const TimeSig &ts) const {
    return (timeSigType() == ts.timeSigType()) && (sig().identical(ts.sig())) && (stretch() == ts.stretch()) && (groups() == ts.groups()) &&
           (numeratorString_ == ts.numeratorString_) && (denominatorString_ == ts.denominatorString_);
}

void TimeSig::added() {
    IF_ASSERT_FAILED(score()) { return; }

    score()->updateTicksAndTimeSigMapLater();
}

void TimeSig::removed() {
    IF_ASSERT_FAILED(score()) { return; }

    score()->updateTicksAndTimeSigMapLater();
}
} // namespace mu::engraving
