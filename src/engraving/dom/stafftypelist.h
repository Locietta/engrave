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

#ifndef MU_ENGRAVING_STAFFTYPELIST_H
#define MU_ENGRAVING_STAFFTYPELIST_H

#include "stafftype.h"

namespace mu::engraving {
//---------------------------------------------------------
//   StaffTypeList
//    this list is instantiated for every staff
//    to keep track of staff type changes
//---------------------------------------------------------

class StaffTypeList {
public:
    StaffType &staff_type(const Fraction &);
    const StaffType &staff_type(const Fraction &f) const;
    StaffType *set_staff_type(const Fraction &, const StaffType &);
    bool remove_staff_type(const Fraction &);
    bool is_staff_type_start_from(const Fraction &) const;
    void move_staff_type(const Fraction &from, const Fraction &to);

    bool unique_staff_type() const { return staff_type_changes_.empty(); }

    std::pair<int, int> staff_type_range(const Fraction &) const;

    const std::map<int, StaffType> &staff_type_changes() const { return staff_type_changes_; }

private:
    StaffType first_staff_type_; // staff type at tick 0
    std::map<int, StaffType> staff_type_changes_;
};
} // namespace mu::engraving

#endif
