/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#ifndef MUSE_DRAW_PAINTERPATH_H
#define MUSE_DRAW_PAINTERPATH_H

#include <cmath>

#include "bezier.h"
#include "geometry.h"
#include "global/realfn.h"

namespace muse::draw {
struct PainterPath {
    enum class ElementType { MOVE_TO_ELEMENT, LINE_TO_ELEMENT, CURVE_TO_ELEMENT, CURVE_TO_DATA_ELEMENT };

    enum class FillRule { ODD_EVEN_FILL, WINDING_FILL };

    struct Element {
        double x = 0.0;
        double y = 0.0;
        ElementType type = ElementType::MOVE_TO_ELEMENT;

        Element() {}

        Element(double x, double y, ElementType type) : x(x), y(y), type(type) {}

        bool is_move_to() const { return type == ElementType::MOVE_TO_ELEMENT; }

        bool is_line_to() const { return type == ElementType::LINE_TO_ELEMENT; }

        bool is_curve_to() const { return type == ElementType::CURVE_TO_ELEMENT; }

        operator PointF() const { return PointF(x, y); }

        bool operator==(const Element &e) const { return real_is_equal(x, e.x) && real_is_equal(y, e.y) && type == e.type; }

        inline bool operator!=(const Element &e) const { return !operator==(e); }
    };

    PainterPath() = default;

    bool operator==(const PainterPath &other) const { return other.elements_ == elements_; }

    bool operator!=(const PainterPath &other) const { return !operator==(other); }

    void move_to(const PointF &p);

    inline void move_to(double x, double y) { move_to(PointF(x, y)); }

    void line_to(const PointF &p);

    inline void line_to(double x, double y) { line_to(PointF(x, y)); }

    void cubic_to(const PointF &ctrl_pt1, const PointF &ctrl_pt2, const PointF &end_pt);

    inline void cubic_to(double ctrl_pt1x, double ctrl_pt1y, double ctrl_pt2x, double ctrl_pt2y, double end_ptx, double end_pty) {
        cubic_to(PointF(ctrl_pt1x, ctrl_pt1y), PointF(ctrl_pt2x, ctrl_pt2y), PointF(end_ptx, end_pty));
    }

    void translate(const PointF &offset);
    void translate(double dx, double dy);

    RectF bounding_rect() const;

    bool is_empty() const;
    size_t element_count() const;
    PainterPath::Element element_at(size_t i) const;
    void add_rect(const RectF &r);

    inline void add_rect(double x, double y, double w, double h) { add_rect(RectF(x, y, w, h)); }

    void add_ellipse(const RectF &bounding_rect);

    void add_rounded_rect(const RectF &rect, double x_radius, double y_radius);

    void arc_move_to(const RectF &rect, double angle);

    inline void arc_move_to(double x, double y, double w, double h, double angle) { arc_move_to(RectF(x, y, w, h), angle); }

    void arc_to(const RectF &rect, double start_angle, double sweep_length);

    inline void arc_to(double x, double y, double w, double h, double start_angle, double arc_length) {
        arc_to(RectF(x, y, w, h), start_angle, arc_length);
    }

    void close_subpath();

    PainterPath::FillRule fill_rule() const;
    void set_fill_rule(PainterPath::FillRule fill_rule);


private:

    void ensure_data();

    void compute_bounding_rect() const;

    inline void maybe_move_to() {
        if (require_move_to_) {
            Element e = elements_.back();
            e.type = ElementType::MOVE_TO_ELEMENT;
            elements_.push_back(e);
            require_move_to_ = false;
        }
    }

    inline bool is_closed() const {
        const PainterPath::Element &first = elements_.at(c_start_);
        const PainterPath::Element &last = elements_.back();
        return first.x == last.x && first.y == last.y;
    }

    static inline bool is_valid_coord(double c) { return std::isfinite(c) && std::fabs(c) < 1e128; }

    static bool has_valid_coords(const PointF &p);
    static bool has_valid_coords(const RectF &r);

    static RectF painterpath_bezier_extrema(const Bezier &b);

    void set_dirty();

    int c_start_ = 0;
    mutable RectF bounds_;
    bool require_move_to_ = false;
    mutable bool dirty_bounds_ = false;
    bool convex_ = false;
    FillRule fill_rule_ = FillRule::ODD_EVEN_FILL;

    std::vector<Element> elements_;

    friend struct Transform;
};

inline void dump(const PainterPath &p, std::stringstream &ss) {
    ss << "bbox: " << muse::dump(p.bounding_rect()) << "\n"
       << "fillRule: " << int(p.fill_rule()) << "\n"
       << "elements: " << p.element_count() << "\n";
    for (size_t i = 0; i < p.element_count(); ++i) {
        PainterPath::Element e = p.element_at(i);
        ss << " type: " << int(e.type) << ", x: " << e.x << ", y: " << e.y << "\n";
    }
}

inline std::string dump(const PainterPath &p) {
    std::stringstream ss;
    muse::draw::dump(p, ss);
    return ss.str();
}
} // namespace muse::draw

#endif // MUSE_DRAW_PAINTERPATH_H
