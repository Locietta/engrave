/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "engravingitem.h"

namespace mu::engraving {
struct Parenthesis : EngravingItem {
private:
    M_PROPERTY2(DirectionH, direction, set_direction, DirectionH::LEFT)

public:
    static constexpr double parenthesis_end_width = 0.1;

    Parenthesis(DummyParentOr<EngravingItem> parent);
    Parenthesis(const Parenthesis &p);

    Parenthesis *clone() const override { return new Parenthesis(*this); }

    Segment *segment() const { return ownership_parent() && ownership_parent()->is_segment() ? to_segment(ownership_parent()) : nullptr; }

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    bool follow_parent_cur_color() const;
    void set_follow_parent_color(bool val);

    Color cur_color(const rendering::PaintOptions &opt) const override;

    struct LayoutData : public EngravingItem::LayoutData {
        ld_field<muse::draw::PainterPath> path = "[Parenthesis] path";
        ld_field<double> start_y = {"[Parenthesis] startY", 0.0};
        ld_field<double> height = {"[Parenthesis] height", 0.0};
        ld_field<double> mid_point_thickness = {"[Parenthesis] midPointThickness", 0.0};
        ld_field<double> end_point_thickness = {"[Parenthesis] endPointThickness", ::mu::engraving::Parenthesis::parenthesis_end_width};
        ld_field<double> shoulder_width = {"[Parenthesis] endPointThickness", 0.0};
        ld_field<double> intrinsic_mag = {"[Parenthesis] intrinsicMag", 1.0}; // Mag separate from staff scale
        ld_field<SymId> sym_id = {"[Parenthesis] symId", SymId::NO_SYM};
        ld_field<double> sym_scale = {"[Parenthesis] symScale", 1.0};
    };

    DECLARE_LAYOUTDATA_METHODS(Parenthesis);

private:
    bool follow_parent_color_ = false;
};
} // namespace mu::engraving
