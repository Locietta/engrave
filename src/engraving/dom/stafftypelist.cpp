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

const StaffType &StaffTypeList::staff_type(const Fraction &tick) const {
    if (tick.negative()) {
        static const StaffType st;
        return st;
    }

    if (staff_type_changes_.empty()) {
        return first_staff_type_;
    }

    auto i = staff_type_changes_.upper_bound(tick.ticks());
    if (i == staff_type_changes_.begin()) {
        return first_staff_type_;
    }

    return (--i)->second;
}

//---------------------------------------------------------
//   staffType
//---------------------------------------------------------

StaffType &StaffTypeList::staff_type(const Fraction &tick) {
    assert(!tick.negative());

    if (staff_type_changes_.empty()) {
        return first_staff_type_;
    }

    auto i = staff_type_changes_.upper_bound(tick.ticks());
    if (i == staff_type_changes_.begin()) {
        return first_staff_type_;
    }
    return (--i)->second;
}

//---------------------------------------------------------
//   setStaffType
//---------------------------------------------------------

StaffType *StaffTypeList::set_staff_type(const Fraction &tick, const StaffType &st) {
    assert(!tick.negative());

    if (tick.is_zero()) {
        first_staff_type_ = st;
        return &first_staff_type_;
    }

    auto i = staff_type_changes_.find(tick.ticks());
    StaffType *nst;
    if (i == staff_type_changes_.end()) {
        auto k = staff_type_changes_.insert(std::pair<int, StaffType>(tick.ticks(), st));
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

bool StaffTypeList::remove_staff_type(const Fraction &tick) {
    assert(!tick.negative());
    if (tick.is_zero()) {
        first_staff_type_ = StaffType();
        return true;
    }

    auto i = staff_type_changes_.find(tick.ticks());

    if (i == staff_type_changes_.end()) {
        return false;
    }

    staff_type_changes_.erase(i);
    return true;
}

bool StaffTypeList::is_staff_type_start_from(const Fraction &tick) const {
    return staff_type_changes_.find(tick.ticks()) != staff_type_changes_.end();
}

void StaffTypeList::move_staff_type(const Fraction &from, const Fraction &to) {
    StaffType tmp = staff_type(from);
    remove_staff_type(from);
    set_staff_type(to, tmp);
}

//---------------------------------------------------------
//   staffTypeRange
//---------------------------------------------------------

std::pair<int, int> StaffTypeList::staff_type_range(const Fraction &tick) const {
    if (tick.negative()) {
        return {-1, -1};
    }

    auto i = staff_type_changes_.upper_bound(tick.ticks());
    const int end = (i == staff_type_changes_.end()) ? -1 : i->first;
    const int start = (i == staff_type_changes_.begin()) ? 0 : (--i)->first;

    return {start, end};
}
} // namespace mu::engraving
