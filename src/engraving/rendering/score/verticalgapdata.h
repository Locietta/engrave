/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#ifndef MU_ENGRAVING_VERTICALGAPDATALIST_DEV_H
#define MU_ENGRAVING_VERTICALGAPDATALIST_DEV_H

#include <vector>

namespace mu::engraving {
class MStyle;
class Spacer;
class Staff;
class SysStaff;
class System;
} // namespace mu::engraving

namespace mu::engraving::rendering::score {
//---------------------------------------------------------
//   VerticalStretchData
//    helper class for spreading staves over a page
//---------------------------------------------------------

class VerticalGapData {
public:
    const MStyle *style = nullptr;
    System *system = nullptr;
    SysStaff *sys_staff = nullptr;
    const Staff *staff = nullptr;

    VerticalGapData(const MStyle *style, bool first, System *sys, const Staff *st, SysStaff *sst, Spacer *next_spacer, double y);

    void add_space_between_sections();
    void add_space_around_v_box(bool above);
    void add_space_around_normal_bracket();
    void add_space_around_curly_bracket();
    void inside_curly_bracket();

    double factor() const;
    double spacing() const;
    double actual_added_space() const;

    double add_spacing(double step);
    bool is_fixed_height() const;
    void undo_last_add_spacing();
    double add_fill_spacing(double step, double max_fill);

    void set_normalised_spacing(double new_normalised_spacing);

private:
    void update_factor(double factor);

    bool fixed_height_ = false;
    bool fixed_spacer_ = false;
    double factor_ = 1.0;
    double normalised_spacing_ = 0.0;
    double max_actual_spacing_ = 0.0;
    double added_normalised_space_ = 0.0;
    double fill_spacing_ = 0.0;
    double last_step_ = 0.0;
};

//---------------------------------------------------------
//   VerticalStretchDataList
//    helper class for spreading staves over a page
//---------------------------------------------------------

class VerticalGapDataList : public std::vector<VerticalGapData *> {
public:
    void delete_all();
    double sum_stretch_factor() const;
    double smallest(double limit = -1.0) const;
};
} // namespace mu::engraving::rendering::score

#endif // MU_ENGRAVING_VERTICALGAPDATALIST_DEV_H
