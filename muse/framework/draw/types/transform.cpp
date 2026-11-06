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

#include "transform.h"

#include "log.h"
#include "painterpath.h"

using namespace muse;
using namespace muse::draw;

static constexpr double near_clip = 0.000001;

Transform::Transform(double h11, double h12, double h21, double h22, double dx, double dy)
    : affine_(h11, h12, h21, h22, dx, dy), dirty_(TransformationType::SHEAR) {}

Transform::Transform(double h11, double h12, double h13, double h21, double h22, double h23, double h31, double h32, double h33)
    : affine_(h11, h12, h21, h22, h31, h32), m13_(h13), m23_(h23), m33_(h33), type_(TransformationType::NONE),
      dirty_(TransformationType::PROJECT) {}

Transform Transform::operator*(const Transform &m) const {
    const TransformationType other_type = m.inline_type();

    if (other_type == TransformationType::NONE) {
        return *this;
    }
    const TransformationType this_type = inline_type();
    if (this_type == TransformationType::NONE) {
        return m;
    }
    Transform t;
    TransformationType type = std::max(this_type, other_type);
    switch (type) {
        case TransformationType::NONE: break;
        case TransformationType::TRANSLATE:
            t.affine_.dx_ = affine_.dx_ + m.affine_.dx_;
            t.affine_.dy_ = affine_.dy_ + m.affine_.dy_;
            break;
        case TransformationType::SCALE: {
            double m11 = affine_.m11_ * m.affine_.m11_;
            double m22 = affine_.m22_ * m.affine_.m22_;
            double m31 = affine_.dx_ * m.affine_.m11_ + m.affine_.dx_;
            double m32 = affine_.dy_ * m.affine_.m22_ + m.affine_.dy_;
            t.affine_.m11_ = m11;
            t.affine_.m22_ = m22;
            t.affine_.dx_ = m31;
            t.affine_.dy_ = m32;
            break;
        }
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: {
            double m11 = affine_.m11_ * m.affine_.m11_ + affine_.m12_ * m.affine_.m21_;
            double m12 = affine_.m11_ * m.affine_.m12_ + affine_.m12_ * m.affine_.m22_;
            double m21 = affine_.m21_ * m.affine_.m11_ + affine_.m22_ * m.affine_.m21_;
            double m22 = affine_.m21_ * m.affine_.m12_ + affine_.m22_ * m.affine_.m22_;
            double m31 = affine_.dx_ * m.affine_.m11_ + affine_.dy_ * m.affine_.m21_ + m.affine_.dx_;
            double m32 = affine_.dx_ * m.affine_.m12_ + affine_.dy_ * m.affine_.m22_ + m.affine_.dy_;
            t.affine_.m11_ = m11;
            t.affine_.m12_ = m12;
            t.affine_.m21_ = m21;
            t.affine_.m22_ = m22;
            t.affine_.dx_ = m31;
            t.affine_.dy_ = m32;
            break;
        }
        case TransformationType::PROJECT: {
            double m11 = affine_.m11_ * m.affine_.m11_ + affine_.m12_ * m.affine_.m21_ + m13_ * m.affine_.dx_;
            double m12 = affine_.m11_ * m.affine_.m12_ + affine_.m12_ * m.affine_.m22_ + m13_ * m.affine_.dy_;
            double m13 = affine_.m11_ * m.m13_ + affine_.m12_ * m.m23_ + m13_ * m.m33_;
            double m21 = affine_.m21_ * m.affine_.m11_ + affine_.m22_ * m.affine_.m21_ + m23_ * m.affine_.dx_;
            double m22 = affine_.m21_ * m.affine_.m12_ + affine_.m22_ * m.affine_.m22_ + m23_ * m.affine_.dy_;
            double m23 = affine_.m21_ * m.m13_ + affine_.m22_ * m.m23_ + m23_ * m.m33_;
            double m31 = affine_.dx_ * m.affine_.m11_ + affine_.dy_ * m.affine_.m21_ + m33_ * m.affine_.dx_;
            double m32 = affine_.dx_ * m.affine_.m12_ + affine_.dy_ * m.affine_.m22_ + m33_ * m.affine_.dy_;
            double m33 = affine_.dx_ * m.m13_ + affine_.dy_ * m.m23_ + m33_ * m.m33_;
            t.affine_.m11_ = m11;
            t.affine_.m12_ = m12;
            t.m13_ = m13;
            t.affine_.m21_ = m21;
            t.affine_.m22_ = m22;
            t.m23_ = m23;
            t.affine_.dx_ = m31;
            t.affine_.dy_ = m32;
            t.m33_ = m33;
        }
    }
    t.dirty_ = type;
    t.type_ = type;
    return t;
}

Transform &Transform::operator*=(const Transform &o) {
    const TransformationType other_type = o.inline_type();
    if (other_type == TransformationType::NONE) {
        return *this;
    }
    const TransformationType this_type = inline_type();
    if (this_type == TransformationType::NONE) {
        return operator=(o);
    }
    TransformationType t = std::max(this_type, other_type);
    switch (t) {
        case TransformationType::NONE: break;
        case TransformationType::TRANSLATE:
            affine_.dx_ += o.affine_.dx_;
            affine_.dy_ += o.affine_.dy_;
            break;
        case TransformationType::SCALE: {
            double m11 = affine_.m11_ * o.affine_.m11_;
            double m22 = affine_.m22_ * o.affine_.m22_;
            double m31 = affine_.dx_ * o.affine_.m11_ + o.affine_.dx_;
            double m32 = affine_.dy_ * o.affine_.m22_ + o.affine_.dy_;
            affine_.m11_ = m11;
            affine_.m22_ = m22;
            affine_.dx_ = m31;
            affine_.dy_ = m32;
            break;
        }
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: {
            double m11 = affine_.m11_ * o.affine_.m11_ + affine_.m12_ * o.affine_.m21_;
            double m12 = affine_.m11_ * o.affine_.m12_ + affine_.m12_ * o.affine_.m22_;
            double m21 = affine_.m21_ * o.affine_.m11_ + affine_.m22_ * o.affine_.m21_;
            double m22 = affine_.m21_ * o.affine_.m12_ + affine_.m22_ * o.affine_.m22_;
            double m31 = affine_.dx_ * o.affine_.m11_ + affine_.dy_ * o.affine_.m21_ + o.affine_.dx_;
            double m32 = affine_.dx_ * o.affine_.m12_ + affine_.dy_ * o.affine_.m22_ + o.affine_.dy_;
            affine_.m11_ = m11;
            affine_.m12_ = m12;
            affine_.m21_ = m21;
            affine_.m22_ = m22;
            affine_.dx_ = m31;
            affine_.dy_ = m32;
            break;
        }

        case TransformationType::PROJECT: {
            double m11 = affine_.m11_ * o.affine_.m11_ + affine_.m12_ * o.affine_.m21_ + m13_ * o.affine_.dx_;
            double m12 = affine_.m11_ * o.affine_.m12_ + affine_.m12_ * o.affine_.m22_ + m13_ * o.affine_.dy_;
            double m13 = affine_.m11_ * o.m13_ + affine_.m12_ * o.m23_ + m13_ * o.m33_;
            double m21 = affine_.m21_ * o.affine_.m11_ + affine_.m22_ * o.affine_.m21_ + m23_ * o.affine_.dx_;
            double m22 = affine_.m21_ * o.affine_.m12_ + affine_.m22_ * o.affine_.m22_ + m23_ * o.affine_.dy_;
            double m23 = affine_.m21_ * o.m13_ + affine_.m22_ * o.m23_ + m23_ * o.m33_;
            double m31 = affine_.dx_ * o.affine_.m11_ + affine_.dy_ * o.affine_.m21_ + m33_ * o.affine_.dx_;
            double m32 = affine_.dx_ * o.affine_.m12_ + affine_.dy_ * o.affine_.m22_ + m33_ * o.affine_.dy_;
            double m33 = affine_.dx_ * o.m13_ + affine_.dy_ * o.m23_ + m33_ * o.m33_;
            affine_.m11_ = m11;
            affine_.m12_ = m12;
            m13_ = m13;
            affine_.m21_ = m21;
            affine_.m22_ = m22;
            m23_ = m23;
            affine_.dx_ = m31;
            affine_.dy_ = m32;
            m33_ = m33;
        }
    }
    dirty_ = t;
    type_ = t;
    return *this;
}

void Transform::set_matrix(double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33) {
    affine_.m11_ = m11;
    affine_.m12_ = m12;
    m13_ = m13;
    affine_.m21_ = m21;
    affine_.m22_ = m22;
    m23_ = m23;
    affine_.dx_ = m31;
    affine_.dy_ = m32;
    m33_ = m33;
    dirty_ = TransformationType::PROJECT;
}

void Transform::reset() {
    affine_.m11_ = affine_.m22_ = m33_ = 1.0;
    affine_.m12_ = m13_ = affine_.m21_ = m23_ = affine_.dx_ = affine_.dy_ = 0.0;
}

Transform Transform::adjoint() const {
    double h11, h12, h13, h21, h22, h23, h31, h32, h33;
    h11 = affine_.m22_ * m33_ - m23_ * affine_.dy_;
    h21 = m23_ * affine_.dx_ - affine_.m21_ * m33_;
    h31 = affine_.m21_ * affine_.dy_ - affine_.m22_ * affine_.dx_;
    h12 = m13_ * affine_.dy_ - affine_.m12_ * m33_;
    h22 = affine_.m11_ * m33_ - m13_ * affine_.dx_;
    h32 = affine_.m12_ * affine_.dx_ - affine_.m11_ * affine_.dy_;
    h13 = affine_.m12_ * m23_ - m13_ * affine_.m22_;
    h23 = m13_ * affine_.m21_ - affine_.m11_ * m23_;
    h33 = affine_.m11_ * affine_.m22_ - affine_.m12_ * affine_.m21_;
    return Transform(h11, h12, h13, h21, h22, h23, h31, h32, h33);
}

Transform::TransformationType Transform::type() const {
    if (dirty_ == TransformationType::NONE || dirty_ < type_) {
        return type_;
    }
    switch (dirty_) {
        case TransformationType::PROJECT:
            if (!real_is_null(m13_) || !real_is_null(m23_) || !real_is_null(m33_ - 1)) {
                type_ = TransformationType::PROJECT;
                break;
            }
        // fall through
        case TransformationType::SHEAR:
        case TransformationType::ROTATE:
            if (!real_is_null(affine_.m12_) || !real_is_null(affine_.m21_)) {
                const double dot = affine_.m11_ * affine_.m12_ + affine_.m21_ * affine_.m22_;
                if (real_is_null(dot)) {
                    type_ = TransformationType::ROTATE;
                } else {
                    type_ = TransformationType::SHEAR;
                }
                break;
            }
        // fall through
        case TransformationType::SCALE:
            if (!real_is_null(affine_.m11_ - 1) || !real_is_null(affine_.m22_ - 1)) {
                type_ = TransformationType::SCALE;
                break;
            }
        // fall through
        case TransformationType::TRANSLATE:
            if (!real_is_null(affine_.dx_) || !real_is_null(affine_.dy_)) {
                type_ = TransformationType::TRANSLATE;
                break;
            }
        // fall through
        case TransformationType::NONE: type_ = TransformationType::NONE; break;
    }
    dirty_ = TransformationType::NONE;
    return type_;
}

PointF Transform::map(const PointF &p) const {
    double fx = p.x();
    double fy = p.y();
    double x = 0.0, y = 0.0;
    TransformationType t = inline_type();
    switch (t) {
        case TransformationType::NONE:
            x = fx;
            y = fy;
            break;
        case TransformationType::TRANSLATE:
            x = fx + affine_.dx_;
            y = fy + affine_.dy_;
            break;
        case TransformationType::SCALE:
            x = affine_.m11_ * fx + affine_.dx_;
            y = affine_.m22_ * fy + affine_.dy_;
            break;
        case TransformationType::ROTATE:
        case TransformationType::SHEAR:
        case TransformationType::PROJECT:
            x = affine_.m11_ * fx + affine_.m21_ * fy + affine_.dx_;
            y = affine_.m12_ * fx + affine_.m22_ * fy + affine_.dy_;
            if (t == TransformationType::PROJECT) {
                double w = 1. / (m13_ * fx + m23_ * fy + m33_);
                x *= w;
                y *= w;
            }
    }
    return PointF(x, y);
}

LineF Transform::map(const LineF &l) const {
    double fx1 = l.x1();
    double fy1 = l.y1();
    double fx2 = l.x2();
    double fy2 = l.y2();
    double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
    TransformationType t = inline_type();
    switch (t) {
        case TransformationType::NONE:
            x1 = fx1;
            y1 = fy1;
            x2 = fx2;
            y2 = fy2;
            break;
        case TransformationType::TRANSLATE:
            x1 = fx1 + affine_.dx_;
            y1 = fy1 + affine_.dy_;
            x2 = fx2 + affine_.dx_;
            y2 = fy2 + affine_.dy_;
            break;
        case TransformationType::SCALE:
            x1 = affine_.m11_ * fx1 + affine_.dx_;
            y1 = affine_.m22_ * fy1 + affine_.dy_;
            x2 = affine_.m11_ * fx2 + affine_.dx_;
            y2 = affine_.m22_ * fy2 + affine_.dy_;
            break;
        case TransformationType::ROTATE:
        case TransformationType::SHEAR:
        case TransformationType::PROJECT:
            x1 = affine_.m11_ * fx1 + affine_.m21_ * fy1 + affine_.dx_;
            y1 = affine_.m12_ * fx1 + affine_.m22_ * fy1 + affine_.dy_;
            x2 = affine_.m11_ * fx2 + affine_.m21_ * fy2 + affine_.dx_;
            y2 = affine_.m12_ * fx2 + affine_.m22_ * fy2 + affine_.dy_;
            if (t == TransformationType::PROJECT) {
                double w = 1. / (m13_ * fx1 + m23_ * fy1 + m33_);
                x1 *= w;
                y1 *= w;
                w = 1. / (m13_ * fx2 + m23_ * fy2 + m33_);
                x2 *= w;
                y2 *= w;
            }
    }
    return LineF(x1, y1, x2, y2);
}

static inline bool needs_perspective_clipping(const RectF &rect, const Transform &transform) {
    const double wx = std::min(transform.m13() * rect.left(), transform.m13() * rect.right());
    const double wy = std::min(transform.m23() * rect.top(), transform.m23() * rect.bottom());

    return wx + wy + transform.m33() < ::near_clip;
}

RectF Transform::map(const RectF &rect) const {
    TransformationType t = inline_type();
    if (t <= TransformationType::TRANSLATE) {
        return rect.translated(affine_.dx_, affine_.dy_);
    }

    if (t <= TransformationType::SCALE) {
        double x = affine_.m11_ * rect.x() + affine_.dx_;
        double y = affine_.m22_ * rect.y() + affine_.dy_;
        double w = affine_.m11_ * rect.width();
        double h = affine_.m22_ * rect.height();
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return RectF(x, y, w, h);
    } else if (t < TransformationType::PROJECT || !needs_perspective_clipping(rect, *this)) {
        double x = rect.x(), y = rect.y();
        map_element(x, y, t);
        double xmin = x;
        double ymin = y;
        double xmax = x;
        double ymax = y;

        x = rect.x() + rect.width();
        y = rect.y();
        map_element(x, y, t);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);

        x = rect.x() + rect.width();
        y = rect.y() + rect.height();
        map_element(x, y, t);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);

        x = rect.x();
        y = rect.y() + rect.height();
        map_element(x, y, t);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);

        return RectF(xmin, ymin, xmax - xmin, ymax - ymin);
    } else {
        PainterPath path;
        path.add_rect(rect);
        return map(path).bounding_rect();
    }
}

PainterPath Transform::map(const PainterPath &path) const {
    TransformationType t = inline_type();
    if (t == TransformationType::NONE || path.element_count() == 0) {
        return path;
    }

    // TransformationType::Project is not supported here, it was not used while mapping PainterPath and it needs a lot of code to handle
    PainterPath copy = path;

    if (t == TransformationType::TRANSLATE) {
        copy.translate(affine_.dx_, affine_.dy_);
    } else {
        // Full xform
        for (size_t i = 0; i < path.element_count(); ++i) {
            PainterPath::Element &e = copy.elements_[i];
            map_element(e.x, e.y, t);
        }

        copy.set_dirty();
    }

    return copy;
}

Transform &Transform::rotate(double a) {
    constexpr double deg2rad = 0.017453292519943295769; // pi/180

    if (real_is_null(a)) {
        return *this;
    }

    double sina = 0.0;
    double cosa = 0.0;
    if (real_is_equal(a, 90.) || real_is_equal(a, -270.)) {
        sina = 1.;
    } else if (real_is_equal(a, 270.) || real_is_equal(a, -90.)) {
        sina = -1.;
    } else if (real_is_equal(a, 180.)) {
        cosa = -1.;
    } else {
        double b = deg2rad * a;
        sina = std::sin(b);
        cosa = std::cos(b);
    }
    switch (inline_type()) {
        case TransformationType::NONE:
        case TransformationType::TRANSLATE:
            affine_.m11_ = cosa;
            affine_.m12_ = sina;
            affine_.m21_ = -sina;
            affine_.m22_ = cosa;
            break;
        case TransformationType::SCALE: {
            double tm11 = cosa * affine_.m11_;
            double tm12 = sina * affine_.m22_;
            double tm21 = -sina * affine_.m11_;
            double tm22 = cosa * affine_.m22_;
            affine_.m11_ = tm11;
            affine_.m12_ = tm12;
            affine_.m21_ = tm21;
            affine_.m22_ = tm22;
            break;
        }
        case TransformationType::PROJECT: {
            double tm13 = cosa * m13_ + sina * m23_;
            double tm23 = -sina * m13_ + cosa * m23_;
            m13_ = tm13;
            m23_ = tm23;
        }
        // fallthrough
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: {
            double tm11 = cosa * affine_.m11_ + sina * affine_.m21_;
            double tm12 = cosa * affine_.m12_ + sina * affine_.m22_;
            double tm21 = -sina * affine_.m11_ + cosa * affine_.m21_;
            double tm22 = -sina * affine_.m12_ + cosa * affine_.m22_;
            affine_.m11_ = tm11;
            affine_.m12_ = tm12;
            affine_.m21_ = tm21;
            affine_.m22_ = tm22;
            break;
        }
    }
    if (dirty_ < TransformationType::ROTATE) {
        dirty_ = TransformationType::ROTATE;
    }

    return *this;
}

Transform &Transform::rotate_radians(double a) {
    double sina = std::sin(a);
    double cosa = std::cos(a);
    switch (inline_type()) {
        case TransformationType::NONE:
        case TransformationType::TRANSLATE:
            affine_.m11_ = cosa;
            affine_.m12_ = sina;
            affine_.m21_ = -sina;
            affine_.m22_ = cosa;
            break;
        case TransformationType::SCALE: {
            double tm11 = cosa * affine_.m11_;
            double tm12 = sina * affine_.m22_;
            double tm21 = -sina * affine_.m11_;
            double tm22 = cosa * affine_.m22_;
            affine_.m11_ = tm11;
            affine_.m12_ = tm12;
            affine_.m21_ = tm21;
            affine_.m22_ = tm22;
            break;
        }
        case TransformationType::PROJECT: {
            double tm13 = cosa * m13_ + sina * m23_;
            double tm23 = -sina * m13_ + cosa * m23_;
            m13_ = tm13;
            m23_ = tm23;
        }
        // fallthrough
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: {
            double tm11 = cosa * affine_.m11_ + sina * affine_.m21_;
            double tm12 = cosa * affine_.m12_ + sina * affine_.m22_;
            double tm21 = -sina * affine_.m11_ + cosa * affine_.m21_;
            double tm22 = -sina * affine_.m12_ + cosa * affine_.m22_;
            affine_.m11_ = tm11;
            affine_.m12_ = tm12;
            affine_.m21_ = tm21;
            affine_.m22_ = tm22;
            break;
        }
    }
    if (dirty_ < TransformationType::ROTATE) {
        dirty_ = TransformationType::ROTATE;
    }

    return *this;
}

Transform &Transform::translate(double dx, double dy) {
    if (real_is_null(dx) && real_is_null(dy)) {
        return *this;
    }
    switch (inline_type()) {
        case TransformationType::NONE:
            affine_.dx_ = dx;
            affine_.dy_ = dy;
            break;
        case TransformationType::TRANSLATE:
            affine_.dx_ += dx;
            affine_.dy_ += dy;
            break;
        case TransformationType::SCALE:
            affine_.dx_ += dx * affine_.m11_;
            affine_.dy_ += dy * affine_.m22_;
            break;
        case TransformationType::PROJECT: m33_ += dx * m13_ + dy * m23_;
        // fallthrough
        case TransformationType::SHEAR:
        case TransformationType::ROTATE:
            affine_.dx_ += dx * affine_.m11_ + dy * affine_.m21_;
            affine_.dy_ += dy * affine_.m22_ + dx * affine_.m12_;
            break;
    }
    if (dirty_ < TransformationType::TRANSLATE) {
        dirty_ = TransformationType::TRANSLATE;
    }
    return *this;
}

Transform &Transform::scale(double sx, double sy) {
    if (real_is_equal(sx, 1) && real_is_equal(sy, 1)) {
        return *this;
    }
    switch (inline_type()) {
        case TransformationType::NONE:
        case TransformationType::TRANSLATE:
            affine_.m11_ = sx;
            affine_.m22_ = sy;
            break;
        case TransformationType::PROJECT: m13_ *= sx; m23_ *= sy;
        // fall through
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: affine_.m12_ *= sx; affine_.m21_ *= sy;
        // fall through
        case TransformationType::SCALE:
            affine_.m11_ *= sx;
            affine_.m22_ *= sy;
            break;
    }
    if (dirty_ < TransformationType::SCALE) {
        dirty_ = TransformationType::SCALE;
    }
    return *this;
}

Transform &Transform::shear(double sh, double sv) {
    if (real_is_null(sh) && real_is_null(sv)) {
        return *this;
    }
    switch (inline_type()) {
        case TransformationType::NONE:
        case TransformationType::TRANSLATE:
            affine_.m12_ = sv;
            affine_.m21_ = sh;
            break;
        case TransformationType::SCALE:
            affine_.m12_ = sv * affine_.m22_;
            affine_.m21_ = sh * affine_.m11_;
            break;
        case TransformationType::PROJECT: {
            double tm13 = sv * m23_;
            double tm23 = sh * m13_;
            m13_ += tm13;
            m23_ += tm23;
        }
        // fallthrough
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: {
            double tm11 = sv * affine_.m21_;
            double tm22 = sh * affine_.m12_;
            double tm12 = sv * affine_.m22_;
            double tm21 = sh * affine_.m11_;
            affine_.m11_ += tm11;
            affine_.m12_ += tm12;
            affine_.m21_ += tm21;
            affine_.m22_ += tm22;
            break;
        }
    }
    if (dirty_ < TransformationType::SHEAR) {
        dirty_ = TransformationType::SHEAR;
    }
    return *this;
}

Transform Transform::inverted() const {
    Transform invert;
    bool inv = true;
    switch (inline_type()) {
        case TransformationType::NONE: break;
        case TransformationType::TRANSLATE:
            invert.affine_.dx_ = -affine_.dx_;
            invert.affine_.dy_ = -affine_.dy_;
            break;
        case TransformationType::SCALE:
            inv = !real_is_null(affine_.m11_);
            inv &= !real_is_null(affine_.m22_);
            if (inv) {
                invert.affine_.m11_ = 1. / affine_.m11_;
                invert.affine_.m22_ = 1. / affine_.m22_;
                invert.affine_.dx_ = -affine_.dx_ * invert.affine_.m11_;
                invert.affine_.dy_ = -affine_.dy_ * invert.affine_.m22_;
            }
            break;
        case TransformationType::ROTATE:
        case TransformationType::SHEAR: invert.affine_ = affine_.inverted(&inv); break;
        default:
            // general case
            double det = determinant();
            inv = !real_is_null(det);
            if (inv) {
                invert = adjoint() / det;
            }
            break;
    }

    if (inv) {
        // inverting doesn't change the type
        invert.type_ = type_;
        invert.dirty_ = dirty_;
    }
    return invert;
}

void Transform::map_element(double &nx, double &ny, TransformationType t) const {
    double fx_value = nx;
    double fy_value = ny;
    switch (t) {
        case TransformationType::NONE:
            nx = fx_value;
            ny = fy_value;
            break;
        case TransformationType::TRANSLATE:
            nx = fx_value + affine_.dx_;
            ny = fy_value + affine_.dy_;
            break;
        case TransformationType::SCALE:
            nx = affine_.m11_ * fx_value + affine_.dx_;
            ny = affine_.m22_ * fy_value + affine_.dy_;
            break;
        case TransformationType::ROTATE:
        case TransformationType::SHEAR:
        case TransformationType::PROJECT:
            nx = affine_.m11_ * fx_value + affine_.m21_ * fy_value + affine_.dx_;
            ny = affine_.m12_ * fx_value + affine_.m22_ * fy_value + affine_.dy_;
            if (t == TransformationType::PROJECT) {
                double w = (m13_ * fx_value + m23_ * fy_value + m33_);
                if (w < ::near_clip) {
                    w = ::near_clip;
                }
                w = 1. / w;
                nx *= w;
                ny *= w;
            }
    }
}
