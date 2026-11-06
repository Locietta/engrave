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

#include "connector.h"

#include <climits>

#include "engravingitem.h"
#include "engravingobject.h"
#include "log.h"
#include "score.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   ConnectorInfo
//---------------------------------------------------------

ConnectorInfo::ConnectorInfo(const EngravingItem *current, int track, Fraction frac)
    : current_loc_(Location::absolute()), current_(current), score_(current->score()) {
    IF_ASSERT_FAILED(current) { return; }

    // It is not always possible to determine the track number correctly from
    // the current element (for example, in case of a Segment).
    // If the caller does not know the track number and passes -1
    // it may be corrected later.
    if (track >= 0) {
        current_loc_.set_track(track);
    }
    if (frac >= Fraction(0, 1)) {
        current_loc_.set_frac(frac);
    }
}

//---------------------------------------------------------
//   ConnectorInfo
//---------------------------------------------------------

ConnectorInfo::ConnectorInfo(const Score *score, const Location &current_location) : current_loc_(current_location), score_(score) {}

//---------------------------------------------------------
//   ConnectorInfo::updateLocation
//---------------------------------------------------------

void ConnectorInfo::update_location(const EngravingItem *e, Location &l, bool clipboardmode) { l.fill_for_element(e, clipboardmode); }

//---------------------------------------------------------
//   ConnectorInfo::updateCurrentInfo
//---------------------------------------------------------

void ConnectorInfo::update_current_info(bool clipboardmode) {
    if (!current_updated() && current_) {
        update_location(current_, current_loc_, clipboardmode);
    }
    set_current_updated(true);
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
    if (has_previous() && prev_ == nullptr && other->has_next() && other->next_ == nullptr) {
        if ((prev_loc_ == other->current_loc_) && (current_loc_ == other->next_loc_)) {
            prev_ = other;
            other->next_ = this;
            return true;
        }
    }
    if (has_next() && next_ == nullptr && other->has_previous() && other->prev_ == nullptr) {
        if ((next_loc_ == other->current_loc_) && (current_loc_ == other->prev_loc_)) {
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

void ConnectorInfo::force_connect(ConnectorInfo *other) {
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
    constexpr int common_denominator = 1000;
    Fraction dfrac = (l2.frac() - l1.frac()).abs_value();
    int dpos = dfrac.numerator() * common_denominator / dfrac.denominator();
    dpos += 10000 * std::abs(l2.measure() - l1.measure());
    return 1000 * dpos + 100 * std::abs(l2.track() - l1.track()) + 10 * std::abs(l2.note() - l1.note()) +
           std::abs(l2.grace_index() - l1.grace_index());
}

//---------------------------------------------------------
//   ConnectorInfo::orderedConnectionDistance
//---------------------------------------------------------

int ConnectorInfo::ordered_connection_distance(const ConnectorInfo &c1, const ConnectorInfo &c2) {
    Location c1_next = c1.next_loc_;
    c1_next.to_relative(c1.current_loc_);
    Location c2_prev = c2.current_loc_; // inversed order to get equal signs
    c2_prev.to_relative(c2.prev_loc_);
    if (c1_next == c2_prev) {
        return distance(c1.next_loc_, c2.current_loc_);
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

int ConnectorInfo::connection_distance(const ConnectorInfo &other) const {
    if (type_ != other.type_ || score_ != other.score_) {
        return INT_MAX;
    }
    int dist_this_other = INT_MAX;
    int dist_other_this = INT_MAX;
    if (has_next() && next_ == nullptr && other.has_previous() && other.prev_ == nullptr) {
        dist_this_other = ordered_connection_distance(*this, other);
    }
    if (has_previous() && prev_ == nullptr && other.has_next() && other.next_ == nullptr) {
        dist_other_this = ordered_connection_distance(other, *this);
    }
    if (dist_other_this < dist_this_other) {
        return -dist_other_this;
    }
    return dist_this_other;
}

//---------------------------------------------------------
//   ConnectorInfo::findFirst
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::find_first() {
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

const ConnectorInfo *ConnectorInfo::find_first() const { return const_cast<ConnectorInfo *>(this)->find_first(); }

//---------------------------------------------------------
//   ConnectorInfo::findLast
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::find_last() {
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

const ConnectorInfo *ConnectorInfo::find_last() const { return const_cast<ConnectorInfo *>(this)->find_last(); }

//---------------------------------------------------------
//   ConnectorInfo::finished
//---------------------------------------------------------

bool ConnectorInfo::finished() const { return finished_left() && finished_right(); }

//---------------------------------------------------------
//   ConnectorInfo::finishedLeft
//---------------------------------------------------------

bool ConnectorInfo::finished_left() const {
    const ConnectorInfo *i = find_first();
    return i && !i->has_previous();
}

//---------------------------------------------------------
//   ConnectorInfo::finishedRight
//---------------------------------------------------------

bool ConnectorInfo::finished_right() const {
    const ConnectorInfo *i = find_last();
    return i && !i->has_next();
}

//---------------------------------------------------------
//   ConnectorInfo::start
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::start() {
    ConnectorInfo *i = find_first();
    if (i && i->has_previous()) {
        return nullptr;
    }
    return i;
}

//---------------------------------------------------------
//   ConnectorInfo::end
//---------------------------------------------------------

ConnectorInfo *ConnectorInfo::end() {
    ConnectorInfo *i = find_last();
    if (i && i->has_next()) {
        return nullptr;
    }
    return i;
}
} // namespace mu::engraving
