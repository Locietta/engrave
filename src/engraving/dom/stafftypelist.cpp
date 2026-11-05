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

#include "stafftypelist.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   staffType
//---------------------------------------------------------

const StaffType &StaffTypeList::staffType(const Fraction &tick) const {
    if (tick.negative()) {
        static const StaffType st;
        return st;
    }

    if (staffTypeChanges_.empty()) {
        return firstStaffType_;
    }

    auto i = staffTypeChanges_.upper_bound(tick.ticks());
    if (i == staffTypeChanges_.begin()) {
        return firstStaffType_;
    }

    return (--i)->second;
}

//---------------------------------------------------------
//   staffType
//---------------------------------------------------------

StaffType &StaffTypeList::staffType(const Fraction &tick) {
    assert(!tick.negative());

    if (staffTypeChanges_.empty()) {
        return firstStaffType_;
    }

    auto i = staffTypeChanges_.upper_bound(tick.ticks());
    if (i == staffTypeChanges_.begin()) {
        return firstStaffType_;
    }
    return (--i)->second;
}

//---------------------------------------------------------
//   setStaffType
//---------------------------------------------------------

StaffType *StaffTypeList::setStaffType(const Fraction &tick, const StaffType &st) {
    assert(!tick.negative());

    if (tick.isZero()) {
        firstStaffType_ = st;
        return &firstStaffType_;
    }

    auto i = staffTypeChanges_.find(tick.ticks());
    StaffType *nst;
    if (i == staffTypeChanges_.end()) {
        auto k = staffTypeChanges_.insert(std::pair<int, StaffType>(tick.ticks(), st));
        nst = &(k.first->second);
    } else {
        i->second = st;
        nst = &i->second;
    }
    return nst;
}

//---------------------------------------------------------
//   removeStaffType
//---------------------------------------------------------

bool StaffTypeList::removeStaffType(const Fraction &tick) {
    assert(!tick.negative());
    if (tick.isZero()) {
        firstStaffType_ = StaffType();
        return true;
    }

    auto i = staffTypeChanges_.find(tick.ticks());

    if (i == staffTypeChanges_.end()) {
        return false;
    }

    staffTypeChanges_.erase(i);
    return true;
}

bool StaffTypeList::isStaffTypeStartFrom(const Fraction &tick) const {
    return staffTypeChanges_.find(tick.ticks()) != staffTypeChanges_.end();
}

void StaffTypeList::moveStaffType(const Fraction &from, const Fraction &to) {
    StaffType tmp = staffType(from);
    removeStaffType(from);
    setStaffType(to, tmp);
}

//---------------------------------------------------------
//   staffTypeRange
//---------------------------------------------------------

std::pair<int, int> StaffTypeList::staffTypeRange(const Fraction &tick) const {
    if (tick.negative()) {
        return {-1, -1};
    }

    auto i = staffTypeChanges_.upper_bound(tick.ticks());
    const int end = (i == staffTypeChanges_.end()) ? -1 : i->first;
    const int start = (i == staffTypeChanges_.begin()) ? 0 : (--i)->first;

    return {start, end};
}
} // namespace mu::engraving
