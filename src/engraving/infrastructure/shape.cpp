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
#include "shape.h"

#include <cfloat>

#include "dom/engravingitem.h"
#include "draw/painter.h"
#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;

Shape::Shape(const std::vector<RectF> &rects, const EngravingItem *p) {
    type_ = Type::COMPOSITE;
    elements_.reserve(rects.size());
    for (const RectF &rect : rects) {
        elements_.emplace_back(ShapeElement(rect, p));
    }
}

//---------------------------------------------------------
//   addHorizontalSpacing
//    This methods creates "walls". They are represented by
//    rectangles of zero height, and it is assumed that rectangles
//    of zero height vertically collide with everything. Use this
//    method ONLY when you want to create space that cannot tuck
//    above/below other elements of the staff.
//---------------------------------------------------------

void Shape::add_horizontal_spacing(EngravingItem *item, double left_edge, double right_edge) {
    constexpr double eps = 100 * std::numeric_limits<double>::epsilon();
    if (left_edge == right_edge) { // HACK zero-width shapes collide with everything currently.
        right_edge += eps;
    }
    add(RectF(left_edge, 0, right_edge - left_edge, 0), item);
}

//---------------------------------------------------------
//   translate
//---------------------------------------------------------

Shape &Shape::translate(const PointF &pt) {
    for (RectF &r : elements_) {
        r.translate(pt);
    }
    invalidate_b_box();
    return *this;
}

void Shape::translate_x(double xo) {
    for (RectF &r : elements_) {
        r.set_left(r.left() + xo);
        r.set_right(r.right() + xo);
    }
    invalidate_b_box();
}

void Shape::translate_y(double yo) {
    for (RectF &r : elements_) {
        r.set_top(r.top() + yo);
        r.set_bottom(r.bottom() + yo);
    }
    invalidate_b_box();
}

//---------------------------------------------------------
//   translated
//---------------------------------------------------------

Shape Shape::translated(const PointF &pt) const {
    Shape s;
    s.elements_.reserve(elements_.size());
    for (const ShapeElement &r : elements_) {
        s.add(r.translated(pt), r.item(), r.ignore_for_layout());
    }
    return s;
}

Shape &Shape::scale(const SizeF &mag) {
    for (RectF &r : elements_) {
        r.scale(mag);
    }
    invalidate_b_box();
    return *this;
}

Shape Shape::scaled(const SizeF &mag) const {
    Shape s;
    s.elements_.reserve(elements_.size());
    for (const ShapeElement &r : elements_) {
        s.add(r.scaled(mag), r.item(), r.ignore_for_layout());
    }
    return s;
}

Shape &Shape::adjust(double xp1, double yp1, double xp2, double yp2) {
    for (ShapeElement &element : elements_) {
        element.adjust(xp1, yp1, xp2, yp2);
    }
    return *this;
}

Shape Shape::adjusted(double xp1, double yp1, double xp2, double yp2) const {
    Shape s;
    s.elements_.reserve(elements_.size());
    for (const ShapeElement &element : elements_) {
        s.add(element.adjusted(xp1, yp1, xp2, yp2));
    }

    return s;
}

Shape &Shape::pad(double p) {
    for (ShapeElement &el : elements_) {
        el.pad(p);
    }
    return *this;
}

Shape Shape::padded(double p) const {
    Shape s;
    s.elements_.reserve(elements_.size());
    for (const ShapeElement &el : elements_) {
        s.add(el.padded(p));
    }
    return s;
}

mu::engraving::Shape &mu::engraving::Shape::transform(const muse::draw::Transform &t) {
    for (ShapeElement &el : elements_) {
        bool ignore_for_layout = el.ignore_for_layout();
        const EngravingItem *item = el.item();
        RectF rect = t.map(el);
        el = ShapeElement(rect, item, ignore_for_layout);
    }

    return *this;
}

Shape Shape::transformed(const muse::draw::Transform &t) {
    Shape s(*this);
    return s.transform(t);
}

void Shape::invalidate_b_box() { bbox_ = RectF(); }

const RectF &Shape::bbox() const {
    if (elements_.size() == 0) {
        static const RectF dummy;
        return dummy;
    } else if (elements_.size() == 1) {
        return elements_.at(0);
    } else {
        if (bbox_.is_null()) {
            for (const ShapeElement &e : elements_) {
                bbox_.unite(e);
            }
        }
        return bbox_;
    }
}

std::optional<ShapeElement> Shape::find_if(const std::function<bool(const ShapeElement &)> &func) const {
    auto it = std::find_if(elements_.begin(), elements_.end(), func);
    if (it == elements_.end()) {
        return std::nullopt;
    }
    return std::make_optional(*it);
}

std::optional<ShapeElement> Shape::find_first(ElementType type) const {
    for (const ShapeElement &i : elements_) {
        if (i.item() && i.item()->type() == type) {
            return std::make_optional(i);
        }
    }

    return std::nullopt;
}

std::optional<ShapeElement> Shape::get_first() const {
    if (elements_.empty()) {
        return std::nullopt;
    }
    return std::make_optional(elements_.at(0));
}

//-------------------------------------------------------------------
//   minVerticalDistance
//    a is located below this shape.
//    Calculates the minimum distance between two shapes.
//-------------------------------------------------------------------

double Shape::min_vertical_distance(const Shape &a, double min_horizontal_clearance) const {
    if (empty() || a.empty()) {
        return 0.0;
    }

    double dist = -DBL_MAX; // min real
    for (const RectF &r2 : a.elements_) {
        if (r2.height() <= 0.0) {
            continue;
        }
        double bx1 = r2.left();
        double bx2 = r2.right();
        for (const RectF &r1 : elements_) {
            if (r1.height() <= 0.0) {
                continue;
            }
            double ax1 = r1.left();
            double ax2 = r1.right();
            if (mu::engraving::intersects(ax1, ax2, bx1, bx2, min_horizontal_clearance)) {
                dist = std::max(dist, r1.bottom() - r2.top());
            }
        }
    }
    return dist;
}

//-------------------------------------------------------------------
//   verticalClearance
//    a is located below this shape.
//    Claculates the amount of clearance between the two shapes.
//    If there is an overlap, returns a negative value corresponging
//    to the amount of overlap.
//-------------------------------------------------------------------

double Shape::vertical_clearance(const Shape &a, double min_horizontal_distance) const {
    if (empty() || a.empty()) {
        return 0.0;
    }

    double dist = DBL_MAX; // max real
    for (const RectF &r2 : a.elements_) {
        if (r2.height() <= 0.0) {
            continue;
        }
        double bx1 = r2.left();
        double bx2 = r2.right();
        for (const RectF &r1 : elements_) {
            if (r1.height() <= 0.0) {
                continue;
            }
            double ax1 = r1.left();
            double ax2 = r1.right();
            if (mu::engraving::intersects(ax1, ax2, bx1, bx2, min_horizontal_distance)) {
                dist = std::min(dist, r2.top() - r1.bottom());
            }
        }
    }
    return dist;
}

//----------------------------------------------------------------
// clearsVertically()
// a is located below this shape
// returns true if, within the horizontal width of both shapes,
// all parts of this shape are above all parts of a
//----------------------------------------------------------------
bool Shape::clears_vertically(const Shape &a) const {
    for (const RectF &r1 : a.elements_) {
        for (const RectF &r2 : elements_) {
            if (mu::engraving::intersects(r1.left(), r1.right(), r2.left(), r2.right())) {
                if (std::min(r1.top(), r1.bottom()) <= std::max(r2.top(), r2.bottom())) {
                    return false;
                }
            }
        }
    }
    return true;
}

//---------------------------------------------------------
//   left
//    compute left border
//---------------------------------------------------------

double Shape::left() const {
    double dist = DBL_MAX;
    for (const ShapeElement &r : elements_) {
        // TURBO-HACK: the only purpose of this is to ignore fingering for spacing the first CR segment after the header.
        if (!real_is_null(r.height()) && !(r.item() && r.item()->is_fingering()) && r.left() < dist) {
            dist = r.left();
        }
    }
    return dist;
}

//---------------------------------------------------------
//   right
//    compute right border
//---------------------------------------------------------

double Shape::right() const {
    double dist = -DBL_MAX;
    for (const RectF &r : elements_) {
        if (r.right() > dist) {
            dist = r.right();
        }
    }
    return dist;
}

//---------------------------------------------------------
//   top
//---------------------------------------------------------

double Shape::top() const {
    double dist = DBL_MAX;
    for (const RectF &r : elements_) {
        if (r.top() < dist) {
            dist = r.top();
        }
    }
    return dist;
}

//---------------------------------------------------------
//   bottom
//---------------------------------------------------------

double Shape::bottom() const {
    double dist = -DBL_MAX;
    for (const RectF &r : elements_) {
        if (r.bottom() > dist) {
            dist = r.bottom();
        }
    }
    return dist;
}

double Shape::width() const {
    double left = DBL_MAX;
    double right = -DBL_MAX;
    for (const RectF &r : elements_) {
        left = std::min(left, r.left());
        right = std::max(right, r.right());
    }

    return right - left;
}

double Shape::height() const {
    double top = DBL_MAX;
    double bottom = -DBL_MAX;
    for (const RectF &r : elements_) {
        top = std::min(top, r.top());
        bottom = std::max(bottom, r.bottom());
    }

    return bottom - top;
}

double Shape::top_at_x(double x) const {
    double local_top = DBL_MAX;
    for (const ShapeElement &el : elements_) {
        if (el.left() < x && el.right() > x) {
            local_top = std::min(local_top, el.top());
        }
    }

    return local_top != DBL_MAX ? local_top : top();
}

double Shape::bottom_at_x(double x) const {
    double local_bottom = -DBL_MAX;
    for (const ShapeElement &el : elements_) {
        if (el.left() < x && el.right() > x) {
            local_bottom = std::max(local_bottom, el.bottom());
        }
    }

    return local_bottom != DBL_MAX ? local_bottom : bottom();
}

double Shape::right_most_edge_at_height(double y_above, double y_below) const {
    double edge = -DBL_MAX;
    for (const ShapeElement &sh : elements_) {
        if (sh.bottom() > y_above && sh.top() < y_below) {
            edge = std::max(edge, sh.right());
        }
    }

    return edge;
}

double Shape::left_most_edge_at_height(double y_above, double y_below) const {
    double edge = DBL_MAX;
    for (const ShapeElement &sh : elements_) {
        if (sh.bottom() > y_above && sh.top() < y_below) {
            edge = std::min(edge, sh.left());
        }
    }

    return edge;
}

double Shape::left_most_edge_at_top() const {
    double edge = DBL_MAX;
    const double shape_top = top();
    for (const ShapeElement &sh : elements_) {
        if (muse::real_is_equal(sh.top(), shape_top)) {
            edge = std::min(edge, sh.left());
        }
    }

    return edge;
}

//---------------------------------------------------------
//   topDistance
//    p is on top of shape
//    returns negative values if there is an overlap
//---------------------------------------------------------

double Shape::top_distance(const PointF &p) const {
    double dist = DBL_MAX;
    for (const RectF &r : elements_) {
        if (p.x() >= r.left() && p.x() < r.right()) {
            dist = std::min(dist, r.top() - p.y());
        }
    }
    return dist;
}

//---------------------------------------------------------
//   bottomDistance
//    p is below the shape
//    returns negative values if there is an overlap
//---------------------------------------------------------

double Shape::bottom_distance(const PointF &p) const {
    double dist = DBL_MAX;
    for (const RectF &r : elements_) {
        if (p.x() >= r.left() && p.x() < r.right()) {
            dist = std::min(dist, p.y() - r.bottom());
        }
    }
    return dist;
}

void Shape::set_b_box(const RectF &r, const EngravingItem *p) {
    IF_ASSERT_FAILED(type() == Type::FIXED) { return; }

    if (elements_.empty()) {
        elements_.push_back(ShapeElement(r, p));
    } else {
        elements_[0] = ShapeElement(r, p);
    }
}

void Shape::add_b_box(const RectF &r) {
    //    IF_ASSERT_FAILED(type() == Type::Fixed) {
    //        return;
    //    }

    if (elements_.empty()) {
        elements_.push_back(RectF());
    }

    elements_[0].unite(r);
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Shape::add(const Shape &s) {
    type_ = Type::COMPOSITE;
    elements_.insert(elements_.end(), s.elements_.begin(), s.elements_.end());
    invalidate_b_box();
}

void Shape::add(const ShapeElement &shape_el) {
    type_ = Type::COMPOSITE;
    elements_.push_back(shape_el);
    invalidate_b_box();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Shape::remove(const RectF &r) {
    for (auto i = elements_.begin(); i != elements_.end(); ++i) {
        if (*i == r) {
            elements_.erase(i);
            return;
        }
    }

    ASSERT_X("Shape::remove: RectF not found in Shape");

    invalidate_b_box();
}

void Shape::remove(const Shape &s) {
    for (const RectF &r : s.elements_) {
        remove(r);
    }

    invalidate_b_box();
}

std::vector<RectF> Shape::to_rects() const {
    std::vector<RectF> rects;
    rects.reserve(elements_.size());

    for (const RectF &shape_el : elements_) {
        rects.push_back(shape_el);
    }

    return rects;
}

void Shape::remove_invisibles() {
    muse::remove_if(elements_, [](ShapeElement &shape_element) { return !shape_element.item() || !shape_element.item()->visible(); });
    invalidate_b_box();
}

void Shape::remove_types(const std::set<ElementType> &types) {
    muse::remove_if(elements_, [&types](ShapeElement &shape_element) {
        return shape_element.item() && muse::contains(types, shape_element.item()->type());
    });
    invalidate_b_box();
}

//---------------------------------------------------------
//   contains
//---------------------------------------------------------

bool Shape::contains(const PointF &p) const {
    for (const RectF &r : elements_) {
        if (r.contains(p)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   intersects
//---------------------------------------------------------

bool Shape::intersects(const RectF &rr) const {
    for (const RectF &r : elements_) {
        if (r.intersects(rr)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   intersects
//---------------------------------------------------------

bool Shape::intersects(const Shape &other) const {
    for (const RectF &r : other.elements_) {
        if (intersects(r)) {
            return true;
        }
    }
    return false;
}

void Shape::paint(Painter &painter) const {
    for (const RectF &r : elements_) {
        painter.draw_rect(r);
    }
}

void mu::engraving::dump(const ShapeElement &se, std::stringstream &ss) {
    const RectF *r = &se;
    ss << "item: " << (se.item() ? se.item()->type_name() : "no") << " rect: ";
    muse::dump(*r, ss);
}

void mu::engraving::dump(const Shape &sh, std::stringstream &ss) {
    ss << "Shape size: " << sh.size() << "\n";
    for (const ShapeElement &r : sh.elements()) {
        ss << "  ";
        dump(r, ss);
        ss << "\n";
    }
}

std::string mu::engraving::dump(const Shape &sh) {
    std::stringstream ss;
    dump(sh, ss);
    return ss.str();
}
