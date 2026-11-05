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

#include <climits>

#include "connector.h"

#include "engravingitem.h"
#include "engravingobject.h"
#include "score.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   ConnectorInfo
//---------------------------------------------------------

ConnectorInfo::ConnectorInfo(const EngravingItem *current, int track, Fraction frac)
    : currentLoc_(Location::absolute()), current_(current), score_(current->score()) {
    IF_ASSERT_FAILED(current) { return; }

    // It is not always possible to determine the track number correctly from
    // the current element (for example, in case of a Segment).
    // If the caller does not know the track number and passes -1
    // it may be corrected later.
    if (track >= 0) {
        currentLoc_.setTrack(track);
    }
    if (frac >= Fraction(0, 1)) {
        currentLoc_.setFrac(frac);
    }
}

//---------------------------------------------------------
//   ConnectorInfo
//---------------------------------------------------------

ConnectorInfo::ConnectorInfo(const Score *score, const Location &currentLocation) : currentLoc_(currentLocation), score_(score) {}

//---------------------------------------------------------
//   ConnectorInfo::updateLocation
//---------------------------------------------------------

void ConnectorInfo::updateLocation(const EngravingItem *e, Location &l, bool clipboardmode) { l.fillForElement(e, clipboardmode); }

//---------------------------------------------------------
//   ConnectorInfo::updateCurrentInfo
//---------------------------------------------------------

void ConnectorInfo::updateCurrentInfo(bool clipboardmode) {
    if (!currentUpdated() && current_) {
        updateLocation(current_, currentLoc_, clipboardmode);
    }
    setCurrentUpdated(true);
}

//---------------------------------------------------------
//   ConnectorInfo::connect
//---------------------------------------------------------

bool ConnectorInfo::connect(ConnectorInfo *other) {
    if (!other || (this == other)) {
        return false;
    }
    if (type_ != other->type_ || score_ != other->score_) {
        return false;
    }
    if (hasPrevious() && prev_ == nullptr && other->hasNext() && other->next_ == nullptr) {
        if ((prevLoc_ == other->currentLoc_) && (currentLoc_ == other->nextLoc_)) {
            prev_ = other;
            other->next_ = this;
            return true;
        }
    }
    if (hasNext() && next_ == nullptr && other->hasPrevious() && other->prev_ == nullptr) {
        if ((nextLoc_ == other->currentLoc_) && (currentLoc_ == other->prevLoc_)) {
            next_ = other;
            other->prev_ = this;
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   ConnectorInfo::forceConnect
//---------------------------------------------------------

void ConnectorInfo::forceConnect(ConnectorInfo *other) {
    if (!other || (this == other)) {
        return;
    }
    next_ = other;
    other->prev_ = this;
}

//---------------------------------------------------------
//   distance
//---------------------------------------------------------

static int distance(const Location &l1, const Location &l2) {
    constexpr int commonDenominator = 1000;
    Fraction dfrac = (l2.frac() - l1.frac()).absValue();
    int dpos = dfrac.numerator() * commonDenominator / dfrac.denominator();
    dpos += 10000 * std::abs(l2.measure() - l1.measure());
    return 1000 * dpos + 100 * std::abs(l2.track() - l1.track()) + 10 * std::abs(l2.note() - l1.note()) +
           std::abs(l2.graceIndex() - l1.graceIndex());
}

//---------------------------------------------------------
//   ConnectorInfo::orderedConnectionDistance
//---------------------------------------------------------

int ConnectorInfo::orderedConnectionDistance(const ConnectorInfo &c1, const ConnectorInfo &c2) {
    Location c1Next = c1.nextLoc_;
    c1Next.toRelative(c1.currentLoc_);
    Location c2Prev = c2.currentLoc_; // inversed order to get equal signs
    c2Prev.toRelative(c2.prevLoc_);
    if (c1Next == c2Prev) {
        return distance(c1.nextLoc_, c2.currentLoc_);
    }
    return INT_MAX;
}

//---------------------------------------------------------
//   ConnectorInfo::connectionDistance
//    Returns a "distance" representing a likelihood of
//    that the checked connectors should be connected.
//    Returns 0 if can be readily connected via connect(),
//    < 0 if other is likely to be the first,
//    INT_MAX if cannot be connected
//---------------------------------------------------------

int ConnectorInfo::connectionDistance(const ConnectorInfo &other) const {
    if (type_ != other.type_ || score_ != other.score_) {
        return INT_MAX;
    }
    int distThisOther = INT_MAX;
    int distOtherThis = INT_MAX;
    if (hasNext() && next_ == nullptr && other.hasPrevious() && other.prev_ == nullptr) {
        distThisOther = orderedConnectionDistance(*this, other);
    }
    if (hasPrevious() && prev_ == nullptr && other.hasNext() && other.next_ == nullptr) {
        distOtherThis = orderedConnectionDistance(other, *this);
    }
    if (distOtherThis < distThisOther) {
        return -distOtherThis;
    }
    return distThisOther;
}

//---------------------------------------------------------
//   ConnectorInfo::findFirst
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::findFirst() {
    ConnectorInfo *i = this;
    while (i->prev_) {
        i = i->prev_;
        if (i == this) {
            LOGW("ConnectorInfo::findFirst: circular connector %p", this);
            return nullptr;
        }
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::findFirst
//---------------------------------------------------------

const ConnectorInfo *ConnectorInfo::findFirst() const { return const_cast<ConnectorInfo *>(this)->findFirst(); }

//---------------------------------------------------------
//   ConnectorInfo::findLast
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::findLast() {
    ConnectorInfo *i = this;
    while (i->next_) {
        i = i->next_;
        if (i == this) {
            LOGW("ConnectorInfo::findLast: circular connector %p", this);
            return nullptr;
        }
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::findLast
//---------------------------------------------------------

const ConnectorInfo *ConnectorInfo::findLast() const { return const_cast<ConnectorInfo *>(this)->findLast(); }

//---------------------------------------------------------
//   ConnectorInfo::finished
//---------------------------------------------------------

bool ConnectorInfo::finished() const { return finishedLeft() && finishedRight(); }

//---------------------------------------------------------
//   ConnectorInfo::finishedLeft
//---------------------------------------------------------

bool ConnectorInfo::finishedLeft() const {
    const ConnectorInfo *i = findFirst();
    return i && !i->hasPrevious();
}

//---------------------------------------------------------
//   ConnectorInfo::finishedRight
//---------------------------------------------------------

bool ConnectorInfo::finishedRight() const {
    const ConnectorInfo *i = findLast();
    return i && !i->hasNext();
}

//---------------------------------------------------------
//   ConnectorInfo::start
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::start() {
    ConnectorInfo *i = findFirst();
    if (i && i->hasPrevious()) {
        return nullptr;
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::end
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::end() {
    ConnectorInfo *i = findLast();
    if (i && i->hasNext()) {
        return nullptr;
    }
    return i;
}
} // namespace mu::engraving
