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

#include "skyline.h"

#include "../dom/arpeggio.h"
#include "../dom/beam.h"
#include "../dom/chord.h"
#include "../dom/stem.h"
#include "../dom/tremolotwochord.h"
#include "draw/painter.h"
#include "realfn.h"
#include "shape.h"

using namespace mu;
using namespace muse::draw;

namespace mu::engraving {
void Skyline::add(const ShapeElement &r) {
    if (r.ignore_for_layout()) {
        return;
    }
    const EngravingItem *item = r.item();
    bool cross_south = false;
    bool cross_north = false;
    if (item && item->is_stem()) {
        Chord *chord = to_stem(item)->chord();
        if (chord) {
            Beam *beam = chord->beam();
            TremoloTwoChord *tremolo = chord->tremolo_two_chord();
            bool is_cross = (beam && beam->cross()) || (tremolo && tremolo->chord1()->staff_move() != tremolo->chord2()->staff_move());
            if (is_cross) {
                std::vector<ChordRest *> elements;
                if (beam) {
                    elements = beam->elements();
                } else if (tremolo) {
                    elements = {tremolo->chord1(), tremolo->chord2()};
                }
                int this_staff_move = chord->staff_move();
                if (this_staff_move < 0) {
                    cross_north = true;
                } else if (this_staff_move > 0) {
                    cross_south = true;
                }
                for (ChordRest *element : elements) {
                    int staff_move = element->staff_move();
                    if (staff_move < this_staff_move) {
                        cross_north = true;
                    }
                    if (staff_move > this_staff_move) {
                        cross_south = true;
                    }
                }
            }
        }
    }
    if (item && (item->is_arpeggio() || item->is_chord_bracket())) {
        const Arpeggio *arpeggio = to_arpeggio(item);
        if (arpeggio->cross_staff()) {
            cross_south = true;
        }
    }
    if (!cross_north) {
        north_value.add(r);
    }
    if (!cross_south) {
        south_value.add(r);
    }
}

void SkylineLine::add(const Shape &s) {
    for (const ShapeElement &se : s.elements()) {
        add(se);
    }
}

SkylineLine SkylineLine::get_filtered_copy(std::function<bool(const ShapeElement &)> filter_out) const {
    SkylineLine new_skyline_line(*this);

    new_skyline_line.shape_.clear();

    for (const ShapeElement &shape_el : shape_.elements()) {
        if (filter_out(shape_el)) {
            continue;
        }
        new_skyline_line.shape_.add(shape_el);
    }

    return new_skyline_line;
}

void SkylineLine::add(const ShapeElement &r) {
    if (r.ignore_for_layout()) {
        return;
    }

    double x = r.x();
    double top = r.top();
    double bottom = r.bottom();
    double staff_lines_top = staff_lines_top_at_x(x);
    double staff_lines_bottom = staff_lines_bottom_at_x(x);

    if (r.item() && r.item()->is_staff_lines()) {
        if (!(muse::real_is_equal(top, staff_lines_top) && muse::real_is_equal(bottom, staff_lines_bottom))) {
            staff_line_edges_.emplace(round(x), StaffLineEdge(top, bottom));
        }
    } else if (has_valid_staff_line_edges()) {
        if (is_north_ && muse::real_is_equal_or_more(top, staff_lines_top)) {
            return; // Only add if it pokes above the staff
        }
        if (!is_north_ && muse::real_is_equal_or_less(bottom, staff_lines_bottom)) {
            return; // Only add if it pokes below the staff
        }
    }

    shape_.add(r);
}

double SkylineLine::staff_lines_top_at_x(double x) const {
    if (staff_line_edges_.empty()) {
        return 0.0;
    }

    auto upper_bound_value = staff_line_edges_.upper_bound(std::round(x)); // iterator to first element in map with key larger than x
    if (upper_bound_value != staff_line_edges_.begin()) {
        --upper_bound_value; // back by one (i.e. get last element with key smaller than x)
    }

    return (*upper_bound_value).second.top;
}

double SkylineLine::staff_lines_bottom_at_x(double x) const {
    if (staff_line_edges_.empty()) {
        return 0.0;
    }

    auto upper_bound_value = staff_line_edges_.upper_bound(std::round(x)); // iterator to first element in map with key larger than x
    if (upper_bound_value != staff_line_edges_.begin()) {
        --upper_bound_value; // back by one (i.e. get last element with key smaller than x)
    }

    return (*upper_bound_value).second.bottom;
}

void Skyline::add(const Shape &s) {
    for (const auto &r : s.elements()) {
        add(r);
    }
}

void Skyline::clear() {
    north_value.clear();
    south_value.clear();
}

void SkylineLine::clear() {
    staff_line_edges_.clear();
    shape_.clear();
}

//-------------------------------------------------------------------
//   minDistance
//    a is located below this skyline.
//    Calculates the minimum distance between two skylines
//-------------------------------------------------------------------

double Skyline::min_distance(const Skyline &s, double min_horizontal_clearance) const {
    return south().min_distance(s.north(), min_horizontal_clearance);
}

double SkylineLine::min_distance(const SkylineLine &sl, double min_horizontal_clearance) const {
    return shape_.min_vertical_distance(sl.shape_, min_horizontal_clearance);
}

double SkylineLine::min_distance_to_shape_above(const Shape &shape_above, double min_horizontal_clearance) const {
    return shape_above.min_vertical_distance(shape_, min_horizontal_clearance);
}

double SkylineLine::min_distance_to_shape_below(const Shape &shape_below, double min_horizontal_clearance) const {
    return shape_.min_vertical_distance(shape_below, min_horizontal_clearance);
}

double SkylineLine::vertical_clearance_above(const Shape &shape_above) const { return shape_above.vertical_clearance(shape_); }

double SkylineLine::vertical_clearance_below(const Shape &shape_below) const { return shape_.vertical_clearance(shape_below); }

void Skyline::paint(Painter &painter, double line_width) const // DEBUG only
{
    painter.save();

    painter.set_brush(BrushStyle::NO_BRUSH);

    for (const ShapeElement &north_shape_el : north_value.elements()) {
        painter.set_pen(Pen(Color(144, 238, 144), line_width));
        for (const ShapeElement &south_shape_el : south_value.elements()) {
            if (north_shape_el.item() == south_shape_el.item()) {
                // Paint in different color items that belong to both north and south skyline
                painter.set_pen(Pen(Color(144, 191, 191), line_width));
                break;
            }
        }
        painter.draw_rect(north_shape_el);
    }

    painter.set_pen(Pen(Color(144, 144, 238), line_width));
    for (const ShapeElement &south_shape_el : south_value.elements()) {
        bool already_painted = false;
        for (const ShapeElement &north_shape_el : north_value.elements()) {
            if (north_shape_el.item() == south_shape_el.item()) {
                already_painted = true;
                break;
            }
        }
        if (!already_painted) {
            painter.draw_rect(south_shape_el);
        }
    }

    painter.restore();
}

bool SkylineLine::valid() const { return !shape_.empty(); }

SkylineLine &SkylineLine::translate_y(double y) {
    shape_.translate_y(y);
    return *this;
}

double SkylineLine::top(double start_x, double end_x) {
    double top = DBL_MAX;
    for (const ShapeElement &element : shape_.elements()) {
        if (element.right() > start_x && element.left() < end_x) {
            top = std::min(top, element.top());
        }
    }

    if (top == DBL_MAX) {
        top = 0.0;
    }

    return top;
}

double SkylineLine::bottom(double start_x, double end_x) {
    double bottom = -DBL_MAX;
    for (const ShapeElement &element : shape_.elements()) {
        if (element.right() > start_x && element.left() < end_x) {
            bottom = std::max(bottom, element.bottom());
        }
    }

    if (bottom == -DBL_MAX) {
        bottom = 0.0;
    }

    return bottom;
}

double SkylineLine::max() const { return is_north_ ? shape_.top() : shape_.bottom(); }
} // namespace mu::engraving

std::string mu::engraving::dump(const Skyline &skyline) {
    std::stringstream ss;
    ss << "Skyline dump.\n";
    dump(skyline.north(), ss);
    dump(skyline.south(), ss);
    return ss.str();
}

void mu::engraving::dump(const SkylineLine &skyline_line, std::stringstream &ss) {
    ss << "North: " << skyline_line.is_north();
    for (const ShapeElement &se : skyline_line.elements()) {
        dump(se, ss);
    }
}
