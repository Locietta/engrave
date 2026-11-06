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

#include <cassert>
#include "painterpath.h"

#include "log.h"

namespace muse::draw {
static constexpr double pi = 3.14159265358979323846;
static constexpr double path_kappa = 0.5522847498;

static void find_ellipse_coords(const RectF &r, double angle, double length, PointF *start_point, PointF *end_point);

static PointF curves_for_arc(const RectF &rect, double start_angle, double sweep_length, PointF *curves, int *point_count);

static double angle_for_arc(double angle);

void PainterPath::move_to(const PointF &p) {
    if (!has_valid_coords(p)) {
        return;
    }
    ensure_data();
    set_dirty();
    assert(!elements_.empty());
    require_move_to_ = false;
    if (elements_.back().type == ElementType::MOVE_TO_ELEMENT) {
        elements_.back().x = p.x();
        elements_.back().y = p.y();
    } else {
        elements_.push_back({p.x(), p.y(), ElementType::MOVE_TO_ELEMENT});
    }
    c_start_ = static_cast<int>(elements_.size() - 1);
}

void PainterPath::line_to(const PointF &p) {
    if (!has_valid_coords(p)) {
        return;
    }
    ensure_data();
    set_dirty();

    assert(!elements_.empty());
    maybe_move_to();
    if (p == PointF(elements_.back())) {
        return;
    }

    elements_.push_back({p.x(), p.y(), ElementType::LINE_TO_ELEMENT});
    convex_ = elements_.size() == 3 || (elements_.size() == 4 && is_closed());
}

void PainterPath::cubic_to(const PointF &ctrl_pt1, const PointF &ctrl_pt2, const PointF &end_pt) {
    if (!has_valid_coords(ctrl_pt1) || !has_valid_coords(ctrl_pt2) || !has_valid_coords(end_pt)) {
        return;
    }
    ensure_data();
    set_dirty();

    assert(!elements_.empty());
    // Abort on empty curve as a stroker cannot handle this and the
    // curve is irrelevant anyway.

    if (ctrl_pt1 == elements_.back() && ctrl_pt1 == ctrl_pt2 && ctrl_pt2 == end_pt) {
        return;
    }
    maybe_move_to();

    elements_.push_back({ctrl_pt1.x(), ctrl_pt1.y(), ElementType::CURVE_TO_ELEMENT});
    elements_.push_back({ctrl_pt2.x(), ctrl_pt2.y(), ElementType::CURVE_TO_DATA_ELEMENT});
    elements_.push_back({end_pt.x(), end_pt.y(), ElementType::CURVE_TO_DATA_ELEMENT});
}

void PainterPath::translate(double dx, double dy) {
    if (real_is_null(dx) && real_is_null(dy)) {
        return;
    }
    auto elements_left = elements_.size();
    if (elements_left <= 0) {
        return;
    }

    set_dirty();
    PainterPath::Element *element = elements_.data();
    assert(element);
    while (elements_left--) {
        element->x += dx;
        element->y += dy;
        ++element;
    }
}

void PainterPath::translate(const PointF &offset) { translate(offset.x(), offset.y()); }

RectF PainterPath::bounding_rect() const {
    if (dirty_bounds_) {
        compute_bounding_rect();
    }
    return bounds_;
}

bool PainterPath::is_empty() const {
    return elements_.empty() || (elements_.size() == 1 && elements_.front().type == ElementType::MOVE_TO_ELEMENT);
}

size_t PainterPath::element_count() const { return elements_.size(); }

PainterPath::Element PainterPath::element_at(size_t i) const {
    assert(i < element_count());
    return elements_.at(i);
}

void PainterPath::add_rect(const RectF &r) {
    if (!has_valid_coords(r)) {
        return;
    }
    if (r.is_null()) {
        return;
    }
    ensure_data();
    set_dirty();

    bool first = elements_.size() < 2;
    move_to(r.x(), r.y());
    elements_.push_back({r.x() + r.width(), r.y(), ElementType::LINE_TO_ELEMENT});
    elements_.push_back({r.x() + r.width(), r.y() + r.height(), ElementType::LINE_TO_ELEMENT});
    elements_.push_back({r.x(), r.y() + r.height(), ElementType::LINE_TO_ELEMENT});
    elements_.push_back({r.x(), r.y(), ElementType::LINE_TO_ELEMENT});

    require_move_to_ = true;
    convex_ = first;
}

void PainterPath::add_ellipse(const RectF &bounding_rect) {
    if (!has_valid_coords(bounding_rect)) {
        return;
    }
    if (bounding_rect.is_null()) {
        return;
    }
    ensure_data();
    set_dirty();

    bool first = elements_.size() < 2;
    PointF pts[12];
    int point_count;
    PointF start = curves_for_arc(bounding_rect, 0, -360, pts, &point_count);
    move_to(start);
    cubic_to(pts[0], pts[1], pts[2]);   // 0 -> 270
    cubic_to(pts[3], pts[4], pts[5]);   // 270 -> 180
    cubic_to(pts[6], pts[7], pts[8]);   // 180 -> 90
    cubic_to(pts[9], pts[10], pts[11]); // 90 - >0
    require_move_to_ = true;
    convex_ = first;
}

void PainterPath::add_rounded_rect(const RectF &rect, double x_radius, double y_radius) {
    RectF r = rect.normalized();
    if (r.is_null()) {
        return;
    }

    {
        double w = r.width() / 2;
        double h = r.height() / 2;
        if (real_is_null(w)) {
            x_radius = 0.0;
        } else {
            x_radius = 100 * std::min(x_radius, w) / w;
        }
        if (real_is_null(h)) {
            y_radius = 0.0;
        } else {
            y_radius = 100 * std::min(y_radius, h) / h;
        }
    }

    if (x_radius <= 0 || y_radius <= 0) { // add normal rectangle
        add_rect(r);
        return;
    }
    double x = r.x();
    double y = r.y();
    double w = r.width();
    double h = r.height();
    double rxx2 = w * x_radius / 100;
    double ryy2 = h * y_radius / 100;
    ensure_data();
    set_dirty();

    bool first = elements_.size() < 2;
    arc_move_to(x, y, rxx2, ryy2, 180);
    arc_to(x, y, rxx2, ryy2, 180, -90);
    arc_to(x + w - rxx2, y, rxx2, ryy2, 90, -90);
    arc_to(x + w - rxx2, y + h - ryy2, rxx2, ryy2, 0, -90);
    arc_to(x, y + h - ryy2, rxx2, ryy2, 270, -90);
    close_subpath();
    require_move_to_ = true;
    convex_ = first;
}

void PainterPath::arc_move_to(const RectF &rect, double angle) {
    if (rect.is_null()) {
        return;
    }
    PointF pt;
    find_ellipse_coords(rect, angle, 0, &pt, 0);
    move_to(pt);
}

void PainterPath::arc_to(const RectF &rect, double start_angle, double sweep_length) {
    if (!has_valid_coords(rect) || !is_valid_coord(start_angle) || !is_valid_coord(sweep_length)) {
        return;
    }
    if (rect.is_null()) {
        return;
    }
    ensure_data();
    set_dirty();

    int point_count;
    PointF pts[15];
    PointF curve_start = curves_for_arc(rect, start_angle, sweep_length, pts, &point_count);
    line_to(curve_start);
    for (int i = 0; i < point_count; i += 3) {
        cubic_to(pts[i].x(), pts[i].y(), pts[i + 1].x(), pts[i + 1].y(), pts[i + 2].x(), pts[i + 2].y());
    }
}

void PainterPath::close_subpath() {
    if (is_empty()) {
        return;
    }
    set_dirty();
    require_move_to_ = true;
    const Element &first = elements_.at(c_start_);
    Element &last = elements_.back();
    if (first.x != last.x || first.y != last.y) {
        if (real_is_equal(first.x, last.x) && real_is_equal(first.y, last.y)) {
            last.x = first.x;
            last.y = first.y;
        } else {
            elements_.push_back({first.x, first.y, ElementType::LINE_TO_ELEMENT});
        }
    }
}

PainterPath::FillRule PainterPath::fill_rule() const { return is_empty() ? FillRule::ODD_EVEN_FILL : fill_rule_; }

void PainterPath::set_fill_rule(PainterPath::FillRule fill_rule) {
    ensure_data();
    if (fill_rule_ == fill_rule) {
        return;
    }
    set_dirty();
    fill_rule_ = fill_rule;
}

void PainterPath::ensure_data() {
    if (elements_.empty()) {
        elements_.reserve(16);
        elements_.push_back({0, 0, ElementType::MOVE_TO_ELEMENT});
    }
}

bool PainterPath::has_valid_coords(const PointF &p) { return is_valid_coord(p.x()) && is_valid_coord(p.y()); }

bool PainterPath::has_valid_coords(const RectF &r) {
    return is_valid_coord(r.x()) && is_valid_coord(r.y()) && is_valid_coord(r.width()) && is_valid_coord(r.height());
}

void PainterPath::compute_bounding_rect() const {
    dirty_bounds_ = false;

    double maxx, maxy;
    double minx = maxx = elements_.at(0).x;
    double miny = maxy = elements_.at(0).y;
    for (size_t i = 1; i < elements_.size(); ++i) {
        const Element &e = elements_.at(i);
        switch (e.type) {
            case ElementType::MOVE_TO_ELEMENT:
            case ElementType::LINE_TO_ELEMENT:
                if (e.x > maxx) {
                    maxx = e.x;
                } else if (e.x < minx) {
                    minx = e.x;
                }
                if (e.y > maxy) {
                    maxy = e.y;
                } else if (e.y < miny) {
                    miny = e.y;
                }
                break;
            case ElementType::CURVE_TO_ELEMENT: {
                Bezier b = Bezier::from_points(elements_.at(i - 1), e, elements_.at(i + 1), elements_.at(i + 2));
                RectF r = painterpath_bezier_extrema(b);
                double right = r.right();
                double bottom = r.bottom();
                if (r.x() < minx) {
                    minx = r.x();
                }
                if (right > maxx) {
                    maxx = right;
                }
                if (r.y() < miny) {
                    miny = r.y();
                }
                if (bottom > maxy) {
                    maxy = bottom;
                }
                i += 2;
            } break;
            default: break;
        }
    }
    bounds_ = RectF(minx, miny, maxx - minx, maxy - miny);
}

static void find_ellipse_coords(const RectF &r, double angle, double length, PointF *start_point, PointF *end_point) {
    if (r.is_null()) {
        if (start_point) {
            *start_point = PointF();
        }
        if (end_point) {
            *end_point = PointF();
        }
        return;
    }
    double w2 = r.width() / 2;
    double h2 = r.height() / 2;
    double angles[2] = {angle, angle + length};
    PointF *points[2] = {start_point, end_point};
    for (int i = 0; i < 2; ++i) {
        if (!points[i]) {
            continue;
        }
        double theta = angles[i] - 360 * static_cast<int>(std::floor(angles[i] / 360));
        double t = theta / 90;
        // truncate
        int quadrant = int(t);
        t -= quadrant;
        t = angle_for_arc(90 * t);
        // swap x and y?
        if (quadrant & 1) {
            t = 1 - t;
        }
        double a, b, c, d;
        Bezier::coefficients(t, a, b, c, d);
        PointF p(a + b + c * ::muse::draw::path_kappa, d + c + b * ::muse::draw::path_kappa);
        // left quadrants
        if (quadrant == 1 || quadrant == 2) {
            p.rx() = -p.x();
        }
        // top quadrants
        if (quadrant == 0 || quadrant == 1) {
            p.ry() = -p.y();
        }
        *points[i] = r.center() + PointF(w2 * p.x(), h2 * p.y());
    }
}

static PointF curves_for_arc(const RectF &rect, double start_angle, double sweep_length, PointF *curves, int *point_count) {
    assert(point_count);
    assert(curves);
    *point_count = 0;
    if (std::isnan(rect.x()) || std::isnan(rect.y()) || std::isnan(rect.width()) || std::isnan(rect.height()) || std::isnan(start_angle) ||
        std::isnan(sweep_length)) {
        LOGW("PainterPath::arcTo: Adding arc where a parameter is NaN, results are undefined");
        return PointF();
    }
    if (rect.is_null()) {
        return PointF();
    }
    double x = rect.x();
    double y = rect.y();
    double w = rect.width();
    double w2 = rect.width() / 2;
    double w2k = w2 * ::muse::draw::path_kappa;
    double h = rect.height();
    double h2 = rect.height() / 2;
    double h2k = h2 * ::muse::draw::path_kappa;
    PointF points[16] = {// start point
                         PointF(x + w, y + h2),
                         // 0 -> 270 degrees
                         PointF(x + w, y + h2 + h2k), PointF(x + w2 + w2k, y + h), PointF(x + w2, y + h),
                         // 270 -> 180 degrees
                         PointF(x + w2 - w2k, y + h), PointF(x, y + h2 + h2k), PointF(x, y + h2),
                         // 180 -> 90 degrees
                         PointF(x, y + h2 - h2k), PointF(x + w2 - w2k, y), PointF(x + w2, y),
                         // 90 -> 0 degrees
                         PointF(x + w2 + w2k, y), PointF(x + w, y + h2 - h2k), PointF(x + w, y + h2)};
    if (sweep_length > 360) {
        sweep_length = 360;
    } else if (sweep_length < -360) {
        sweep_length = -360;
    }
    // Special case fast paths
    if (real_is_null(start_angle)) {
        if (real_is_equal(sweep_length, 360.0)) {
            for (int i = 11; i >= 0; --i) {
                curves[(*point_count)++] = points[i];
            }
            return points[12];
        } else if (real_is_equal(sweep_length, -360.0)) {
            for (int i = 1; i <= 12; ++i) {
                curves[(*point_count)++] = points[i];
            }
            return points[0];
        }
    }
    int start_segment = int(std::floor(start_angle / 90));
    int end_segment = int(std::floor((start_angle + sweep_length) / 90));
    double start_t = (start_angle - start_segment * 90) / 90;
    double end_t = (start_angle + sweep_length - end_segment * 90) / 90;
    int delta = sweep_length > 0 ? 1 : -1;
    if (delta < 0) {
        start_t = 1 - start_t;
        end_t = 1 - end_t;
    }
    // avoid empty start segment
    if (real_is_null(start_t - double(1))) {
        start_t = 0.0;
        start_segment += delta;
    }
    // avoid empty end segment
    if (real_is_null(end_t)) {
        end_t = 1;
        end_segment -= delta;
    }
    start_t = angle_for_arc(start_t * 90);
    end_t = angle_for_arc(end_t * 90);
    const bool split_at_start = !real_is_null(start_t);
    const bool split_at_end = !real_is_null(end_t - double(1));
    const int end = end_segment + delta;
    // empty arc?
    if (start_segment == end) {
        const int quadrant = 3 - ((start_segment % 4) + 4) % 4;
        const int j = 3 * quadrant;
        return delta > 0 ? points[j + 3] : points[j];
    }
    PointF start_point, end_point;
    find_ellipse_coords(rect, start_angle, sweep_length, &start_point, &end_point);
    for (int i = start_segment; i != end; i += delta) {
        const int quadrant = 3 - ((i % 4) + 4) % 4;
        const int j = 3 * quadrant;
        Bezier b;
        if (delta > 0) {
            b = Bezier::from_points(points[j + 3], points[j + 2], points[j + 1], points[j]);
        } else {
            b = Bezier::from_points(points[j], points[j + 1], points[j + 2], points[j + 3]);
        }
        // empty arc?
        if (start_segment == end_segment && real_is_equal(start_t, end_t)) {
            return start_point;
        }
        if (i == start_segment) {
            if (i == end_segment && split_at_end) {
                b = b.bezier_on_interval(start_t, end_t);
            } else if (split_at_start) {
                b = b.bezier_on_interval(start_t, 1);
            }
        } else if (i == end_segment && split_at_end) {
            b = b.bezier_on_interval(0, end_t);
        }
        // push control points
        curves[(*point_count)++] = b.pt2();
        curves[(*point_count)++] = b.pt3();
        curves[(*point_count)++] = b.pt4();
    }
    assert(*point_count > 0);
    curves[*(point_count) -1] = end_point;
    return start_point;
}

static double angle_for_arc(double angle) {
    if (real_is_null(angle)) {
        return 0;
    }
    if (real_is_equal(angle, double(90))) {
        return 1;
    }
    double radians = angle * (pi / 180);
    double cos_angle = std::cos(radians);
    double sin_angle = std::sin(radians);
    // initial guess
    double tc = angle / 90;
    // do some iterations of newton's method to approximate cosAngle
    // finds the zero of the function b.pointAt(tc).x() - cosAngle
    tc -= ((((2 - 3 * ::muse::draw::path_kappa) * tc + 3 * (::muse::draw::path_kappa - 1)) * tc) * tc + 1 - cos_angle) // value
          / (((6 - 9 * ::muse::draw::path_kappa) * tc + 6 * (::muse::draw::path_kappa - 1)) * tc);                     // derivative
    tc -= ((((2 - 3 * ::muse::draw::path_kappa) * tc + 3 * (::muse::draw::path_kappa - 1)) * tc) * tc + 1 - cos_angle) // value
          / (((6 - 9 * ::muse::draw::path_kappa) * tc + 6 * (::muse::draw::path_kappa - 1)) * tc);                     // derivative
    // initial guess
    double ts = tc;
    // do some iterations of newton's method to approximate sinAngle
    // finds the zero of the function b.pointAt(tc).y() - sinAngle
    ts -= ((((3 * ::muse::draw::path_kappa - 2) * ts - 6 * ::muse::draw::path_kappa + 3) * ts + 3 * ::muse::draw::path_kappa) * ts -
           sin_angle) /
          (((9 * ::muse::draw::path_kappa - 6) * ts + 12 * ::muse::draw::path_kappa - 6) * ts + 3 * ::muse::draw::path_kappa);
    ts -= ((((3 * ::muse::draw::path_kappa - 2) * ts - 6 * ::muse::draw::path_kappa + 3) * ts + 3 * ::muse::draw::path_kappa) * ts -
           sin_angle) /
          (((9 * ::muse::draw::path_kappa - 6) * ts + 12 * ::muse::draw::path_kappa - 6) * ts + 3 * ::muse::draw::path_kappa);
    // use the average of the t that best approximates cosAngle
    // and the t that best approximates sinAngle
    double t = 0.5 * (tc + ts);

    return t;
}

#define BEZIER_A(bezier, coord) 3 * (-bezier.coord##1_ + 3 * bezier.coord##2_ - 3 * bezier.coord##3_ + bezier.coord##4_)
#define BEZIER_B(bezier, coord) 6 * (bezier.coord##1_ - 2 * bezier.coord##2_ + bezier.coord##3_)
#define BEZIER_C(bezier, coord) 3 * (-bezier.coord##1_ + bezier.coord##2_)

#define BEZIER_CHECK_T(bezier, t) \
    if (t >= 0 && t <= 1) {       \
        PointF p(b.point_at(t));  \
        if (p.x() < minx)         \
            minx = p.x();         \
        else if (p.x() > maxx)    \
            maxx = p.x();         \
        if (p.y() < miny)         \
            miny = p.y();         \
        else if (p.y() > maxy)    \
            maxy = p.y();         \
    }

RectF PainterPath::painterpath_bezier_extrema(const Bezier &b) {
    double minx, miny, maxx, maxy;
    // initialize with end points
    if (b.x1_ < b.x4_) {
        minx = b.x1_;
        maxx = b.x4_;
    } else {
        minx = b.x4_;
        maxx = b.x1_;
    }
    if (b.y1_ < b.y4_) {
        miny = b.y1_;
        maxy = b.y4_;
    } else {
        miny = b.y4_;
        maxy = b.y1_;
    }
    // Update for the X extrema
    {
        double ax = BEZIER_A(b, x);
        double bx = BEZIER_B(b, x);
        double cx = BEZIER_C(b, x);
        // specialcase quadratic curves to avoid div by zero
        if (real_is_null(ax)) {
            // linear curves are covered by initialization.
            if (!real_is_null(bx)) {
                double t = -cx / bx;
                BEZIER_CHECK_T(b, t);
            }
        } else {
            const double tx = bx * bx - 4 * ax * cx;
            if (tx >= 0) {
                double temp = std::sqrt(tx);
                double rcp = 1 / (2 * ax);
                double t1 = (-bx + temp) * rcp;
                BEZIER_CHECK_T(b, t1);
                double t2 = (-bx - temp) * rcp;
                BEZIER_CHECK_T(b, t2);
            }
        }
    }
    // Update for the Y extrema
    {
        double ay = BEZIER_A(b, y);
        double by = BEZIER_B(b, y);
        double cy = BEZIER_C(b, y);
        // specialcase quadratic curves to avoid div by zero
        if (real_is_null(ay)) {
            // linear curves are covered by initialization.
            if (!real_is_null(by)) {
                double t = -cy / by;
                BEZIER_CHECK_T(b, t);
            }
        } else {
            const double ty = by * by - 4 * ay * cy;
            if (ty > 0) {
                double temp = std::sqrt(ty);
                double rcp = 1 / (2 * ay);
                double t1 = (-by + temp) * rcp;
                BEZIER_CHECK_T(b, t1);
                double t2 = (-by - temp) * rcp;
                BEZIER_CHECK_T(b, t2);
            }
        }
    }
    return RectF(minx, miny, maxx - minx, maxy - miny);
}

void PainterPath::set_dirty() {
    dirty_bounds_ = true;
    convex_ = false;
}
} // namespace muse::draw
