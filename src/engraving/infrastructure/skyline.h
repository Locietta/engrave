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

#ifndef MU_ENGRAVING_SKYLINE_H
#define MU_ENGRAVING_SKYLINE_H

#include <cfloat>
#include <map>
#include <vector>

#include "draw/types/geometry.h"
#include "shape.h"

namespace muse::draw {
struct Painter;
} // namespace muse::draw

namespace mu::engraving {
struct Segment;

//---------------------------------------------------------
//   SkylineLine
//---------------------------------------------------------

struct SkylineLine {
    SkylineLine(bool n) : is_north_(n) {}

    void add(const ShapeElement &r);

    void add(const RectF &r, EngravingItem *item) { add(ShapeElement(r, item)); }

    void add(const Shape &s);

    template <typename Predicate>
    inline bool remove_if(Predicate p) {
        return shape_.remove_if(p);
    }

    SkylineLine get_filtered_copy(std::function<bool(const ShapeElement &)> filter_out) const;

    void clear();
    // TODO: avoid passing down minHorizontalClearance (in future it must be done
    // item-by-item similar to what we do for horizontal spacing)
    double min_distance(const SkylineLine &, double min_horizontal_clearance = 0.0) const;
    double min_distance_to_shape_above(const Shape &, double min_horizontal_clearance = 0.0) const;
    double min_distance_to_shape_below(const Shape &, double min_horizontal_clearance = 0.0) const;
    double vertical_clearance_above(const Shape &shape_above) const;
    double vertical_clearance_below(const Shape &shape_below) const;
    double max() const;
    bool valid() const;

    SkylineLine &translate_y(double y);

    double top(double start_x = -DBL_MAX, double end_x = DBL_MAX);
    double bottom(double start_x = -DBL_MAX, double end_x = DBL_MAX);

    bool is_north() const { return is_north_; }

    const std::vector<ShapeElement> &elements() const { return shape_.elements(); }

    std::vector<ShapeElement> &elements() { return shape_.elements(); }

private:
    double staff_lines_top_at_x(double x) const;
    double staff_lines_bottom_at_x(double x) const;

private:
    const bool is_north_;
    Shape shape_;

    struct StaffLineEdge {
        double top = 0.0;
        double bottom = 0.0;

        StaffLineEdge(double t, double b) : top(t), bottom(b) {}
    };

    std::map<double, StaffLineEdge> staff_line_edges_;

    bool has_valid_staff_line_edges() const { return !staff_line_edges_.empty(); }
};

//---------------------------------------------------------
//   Skyline
//---------------------------------------------------------

struct Skyline {
private:
    SkylineLine north_value;
    SkylineLine south_value;

public:
    Skyline() : north_value(true), south_value(false) {}

    void clear();
    void add(const Shape &s);
    void add(const ShapeElement &r);

    void add(const RectF &r, EngravingItem *item) { add(ShapeElement(r, item)); }

    // TODO: avoid passing down minHorizontalClearance (in future it must be done
    // item-by-item similar to what we do for horizontal spacing)
    double min_distance(const Skyline &, double min_horizontal_clearance = 0.0) const;

    SkylineLine &north() { return north_value; }

    SkylineLine &south() { return south_value; }

    const SkylineLine &north() const { return north_value; }

    const SkylineLine &south() const { return south_value; }

    void paint(muse::draw::Painter &painter, double line_width) const; // DEBUG only
};

void dump(const SkylineLine &slylineline, std::stringstream &ss);
std::string dump(const Skyline &skyline);
} // namespace mu::engraving

#endif
