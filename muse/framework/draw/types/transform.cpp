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

#include "painterpath.h"

#include "log.h"

using namespace muse;
using namespace muse::draw;

static constexpr double NEAR_CLIP = 0.000001;

Transform::Transform(double h11, double h12, double h21, double h22, double dx, double dy)
    : affine_(h11, h12, h21, h22, dx, dy), dirty_(TransformationType::Shear) {}

Transform::Transform(double h11, double h12, double h13, double h21, double h22, double h23, double h31, double h32, double h33)
    : affine_(h11, h12, h21, h22, h31, h32), m13_(h13), m23_(h23), m33_(h33), type_(TransformationType::None),
      dirty_(TransformationType::Project) {}

Transform Transform::operator*(const Transform &m) const {
    const TransformationType otherType = m.inline_type();

    if (otherType == TransformationType::None) {
        return *this;
    }
    const TransformationType thisType = inline_type();
    if (thisType == TransformationType::None) {
        return m;
    }
    Transform t;
    TransformationType type = std::max(thisType, otherType);
    switch (type) {
        case TransformationType::None: break;
        case TransformationType::Translate:
            t.affine_.dx_ = affine_.dx_ + m.affine_.dx_;
            t.affine_.dy_ = affine_.dy_ + m.affine_.dy_;
            break;
        case TransformationType::Scale: {
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
        case TransformationType::Rotate:
        case TransformationType::Shear: {
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
        case TransformationType::Project: {
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
    const TransformationType otherType = o.inline_type();
    if (otherType == TransformationType::None) {
        return *this;
    }
    const TransformationType thisType = inline_type();
    if (thisType == TransformationType::None) {
        return operator=(o);
    }
    TransformationType t = std::max(thisType, otherType);
    switch (t) {
        case TransformationType::None: break;
        case TransformationType::Translate:
            affine_.dx_ += o.affine_.dx_;
            affine_.dy_ += o.affine_.dy_;
            break;
        case TransformationType::Scale: {
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
        case TransformationType::Rotate:
        case TransformationType::Shear: {
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

        case TransformationType::Project: {
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

void Transform::setMatrix(double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33) {
    affine_.m11_ = m11;
    affine_.m12_ = m12;
    m13_ = m13;
    affine_.m21_ = m21;
    affine_.m22_ = m22;
    m23_ = m23;
    affine_.dx_ = m31;
    affine_.dy_ = m32;
    m33_ = m33;
    dirty_ = TransformationType::Project;
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
    if (dirty_ == TransformationType::None || dirty_ < type_) {
        return type_;
    }
    switch (dirty_) {
        case TransformationType::Project:
            if (!RealIsNull(m13_) || !RealIsNull(m23_) || !RealIsNull(m33_ - 1)) {
                type_ = TransformationType::Project;
                break;
            }
        // fall through
        case TransformationType::Shear:
        case TransformationType::Rotate:
            if (!RealIsNull(affine_.m12_) || !RealIsNull(affine_.m21_)) {
                const double dot = affine_.m11_ * affine_.m12_ + affine_.m21_ * affine_.m22_;
                if (RealIsNull(dot)) {
                    type_ = TransformationType::Rotate;
                } else {
                    type_ = TransformationType::Shear;
                }
                break;
            }
        // fall through
        case TransformationType::Scale:
            if (!RealIsNull(affine_.m11_ - 1) || !RealIsNull(affine_.m22_ - 1)) {
                type_ = TransformationType::Scale;
                break;
            }
        // fall through
        case TransformationType::Translate:
            if (!RealIsNull(affine_.dx_) || !RealIsNull(affine_.dy_)) {
                type_ = TransformationType::Translate;
                break;
            }
        // fall through
        case TransformationType::None: type_ = TransformationType::None; break;
    }
    dirty_ = TransformationType::None;
    return type_;
}

PointF Transform::map(const PointF &p) const {
    double fx = p.x();
    double fy = p.y();
    double x = 0.0, y = 0.0;
    TransformationType t = inline_type();
    switch (t) {
        case TransformationType::None:
            x = fx;
            y = fy;
            break;
        case TransformationType::Translate:
            x = fx + affine_.dx_;
            y = fy + affine_.dy_;
            break;
        case TransformationType::Scale:
            x = affine_.m11_ * fx + affine_.dx_;
            y = affine_.m22_ * fy + affine_.dy_;
            break;
        case TransformationType::Rotate:
        case TransformationType::Shear:
        case TransformationType::Project:
            x = affine_.m11_ * fx + affine_.m21_ * fy + affine_.dx_;
            y = affine_.m12_ * fx + affine_.m22_ * fy + affine_.dy_;
            if (t == TransformationType::Project) {
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
        case TransformationType::None:
            x1 = fx1;
            y1 = fy1;
            x2 = fx2;
            y2 = fy2;
            break;
        case TransformationType::Translate:
            x1 = fx1 + affine_.dx_;
            y1 = fy1 + affine_.dy_;
            x2 = fx2 + affine_.dx_;
            y2 = fy2 + affine_.dy_;
            break;
        case TransformationType::Scale:
            x1 = affine_.m11_ * fx1 + affine_.dx_;
            y1 = affine_.m22_ * fy1 + affine_.dy_;
            x2 = affine_.m11_ * fx2 + affine_.dx_;
            y2 = affine_.m22_ * fy2 + affine_.dy_;
            break;
        case TransformationType::Rotate:
        case TransformationType::Shear:
        case TransformationType::Project:
            x1 = affine_.m11_ * fx1 + affine_.m21_ * fy1 + affine_.dx_;
            y1 = affine_.m12_ * fx1 + affine_.m22_ * fy1 + affine_.dy_;
            x2 = affine_.m11_ * fx2 + affine_.m21_ * fy2 + affine_.dx_;
            y2 = affine_.m12_ * fx2 + affine_.m22_ * fy2 + affine_.dy_;
            if (t == TransformationType::Project) {
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

static inline bool needsPerspectiveClipping(const RectF &rect, const Transform &transform) {
    const double wx = std::min(transform.m13() * rect.left(), transform.m13() * rect.right());
    const double wy = std::min(transform.m23() * rect.top(), transform.m23() * rect.bottom());

    return wx + wy + transform.m33() < NEAR_CLIP;
}

RectF Transform::map(const RectF &rect) const {
    TransformationType t = inline_type();
    if (t <= TransformationType::Translate) {
        return rect.translated(affine_.dx_, affine_.dy_);
    }

    if (t <= TransformationType::Scale) {
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
    } else if (t < TransformationType::Project || !needsPerspectiveClipping(rect, *this)) {
        double x = rect.x(), y = rect.y();
        mapElement(x, y, t);
        double xmin = x;
        double ymin = y;
        double xmax = x;
        double ymax = y;

        x = rect.x() + rect.width();
        y = rect.y();
        mapElement(x, y, t);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);

        x = rect.x() + rect.width();
        y = rect.y() + rect.height();
        mapElement(x, y, t);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);

        x = rect.x();
        y = rect.y() + rect.height();
        mapElement(x, y, t);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);

        return RectF(xmin, ymin, xmax - xmin, ymax - ymin);
    } else {
        PainterPath path;
        path.addRect(rect);
        return map(path).boundingRect();
    }
}

PainterPath Transform::map(const PainterPath &path) const {
    TransformationType t = inline_type();
    if (t == TransformationType::None || path.elementCount() == 0) {
        return path;
    }

    // TransformationType::Project is not supported here, it was not used while mapping PainterPath and it needs a lot of code to handle
    PainterPath copy = path;

    if (t == TransformationType::Translate) {
        copy.translate(affine_.dx_, affine_.dy_);
    } else {
        // Full xform
        for (size_t i = 0; i < path.elementCount(); ++i) {
            PainterPath::Element &e = copy.elements_[i];
            mapElement(e.x, e.y, t);
        }

        copy.setDirty();
    }

    return copy;
}

Transform &Transform::rotate(double a) {
    constexpr double deg2rad = 0.017453292519943295769; // pi/180

    if (RealIsNull(a)) {
        return *this;
    }

    double sina = 0.0;
    double cosa = 0.0;
    if (RealIsEqual(a, 90.) || RealIsEqual(a, -270.)) {
        sina = 1.;
    } else if (RealIsEqual(a, 270.) || RealIsEqual(a, -90.)) {
        sina = -1.;
    } else if (RealIsEqual(a, 180.)) {
        cosa = -1.;
    } else {
        double b = deg2rad * a;
        sina = std::sin(b);
        cosa = std::cos(b);
    }
    switch (inline_type()) {
        case TransformationType::None:
        case TransformationType::Translate:
            affine_.m11_ = cosa;
            affine_.m12_ = sina;
            affine_.m21_ = -sina;
            affine_.m22_ = cosa;
            break;
        case TransformationType::Scale: {
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
        case TransformationType::Project: {
            double tm13 = cosa * m13_ + sina * m23_;
            double tm23 = -sina * m13_ + cosa * m23_;
            m13_ = tm13;
            m23_ = tm23;
        }
        // fallthrough
        case TransformationType::Rotate:
        case TransformationType::Shear: {
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
    if (dirty_ < TransformationType::Rotate) {
        dirty_ = TransformationType::Rotate;
    }

    return *this;
}

Transform &Transform::rotateRadians(double a) {
    double sina = std::sin(a);
    double cosa = std::cos(a);
    switch (inline_type()) {
        case TransformationType::None:
        case TransformationType::Translate:
            affine_.m11_ = cosa;
            affine_.m12_ = sina;
            affine_.m21_ = -sina;
            affine_.m22_ = cosa;
            break;
        case TransformationType::Scale: {
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
        case TransformationType::Project: {
            double tm13 = cosa * m13_ + sina * m23_;
            double tm23 = -sina * m13_ + cosa * m23_;
            m13_ = tm13;
            m23_ = tm23;
        }
        // fallthrough
        case TransformationType::Rotate:
        case TransformationType::Shear: {
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
    if (dirty_ < TransformationType::Rotate) {
        dirty_ = TransformationType::Rotate;
    }

    return *this;
}

Transform &Transform::translate(double dx, double dy) {
    if (RealIsNull(dx) && RealIsNull(dy)) {
        return *this;
    }
    switch (inline_type()) {
        case TransformationType::None:
            affine_.dx_ = dx;
            affine_.dy_ = dy;
            break;
        case TransformationType::Translate:
            affine_.dx_ += dx;
            affine_.dy_ += dy;
            break;
        case TransformationType::Scale:
            affine_.dx_ += dx * affine_.m11_;
            affine_.dy_ += dy * affine_.m22_;
            break;
        case TransformationType::Project: m33_ += dx * m13_ + dy * m23_;
        // fallthrough
        case TransformationType::Shear:
        case TransformationType::Rotate:
            affine_.dx_ += dx * affine_.m11_ + dy * affine_.m21_;
            affine_.dy_ += dy * affine_.m22_ + dx * affine_.m12_;
            break;
    }
    if (dirty_ < TransformationType::Translate) {
        dirty_ = TransformationType::Translate;
    }
    return *this;
}

Transform &Transform::scale(double sx, double sy) {
    if (RealIsEqual(sx, 1) && RealIsEqual(sy, 1)) {
        return *this;
    }
    switch (inline_type()) {
        case TransformationType::None:
        case TransformationType::Translate:
            affine_.m11_ = sx;
            affine_.m22_ = sy;
            break;
        case TransformationType::Project: m13_ *= sx; m23_ *= sy;
        // fall through
        case TransformationType::Rotate:
        case TransformationType::Shear: affine_.m12_ *= sx; affine_.m21_ *= sy;
        // fall through
        case TransformationType::Scale:
            affine_.m11_ *= sx;
            affine_.m22_ *= sy;
            break;
    }
    if (dirty_ < TransformationType::Scale) {
        dirty_ = TransformationType::Scale;
    }
    return *this;
}

Transform &Transform::shear(double sh, double sv) {
    if (RealIsNull(sh) && RealIsNull(sv)) {
        return *this;
    }
    switch (inline_type()) {
        case TransformationType::None:
        case TransformationType::Translate:
            affine_.m12_ = sv;
            affine_.m21_ = sh;
            break;
        case TransformationType::Scale:
            affine_.m12_ = sv * affine_.m22_;
            affine_.m21_ = sh * affine_.m11_;
            break;
        case TransformationType::Project: {
            double tm13 = sv * m23_;
            double tm23 = sh * m13_;
            m13_ += tm13;
            m23_ += tm23;
        }
        // fallthrough
        case TransformationType::Rotate:
        case TransformationType::Shear: {
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
    if (dirty_ < TransformationType::Shear) {
        dirty_ = TransformationType::Shear;
    }
    return *this;
}

Transform Transform::inverted() const {
    Transform invert;
    bool inv = true;
    switch (inline_type()) {
        case TransformationType::None: break;
        case TransformationType::Translate:
            invert.affine_.dx_ = -affine_.dx_;
            invert.affine_.dy_ = -affine_.dy_;
            break;
        case TransformationType::Scale:
            inv = !RealIsNull(affine_.m11_);
            inv &= !RealIsNull(affine_.m22_);
            if (inv) {
                invert.affine_.m11_ = 1. / affine_.m11_;
                invert.affine_.m22_ = 1. / affine_.m22_;
                invert.affine_.dx_ = -affine_.dx_ * invert.affine_.m11_;
                invert.affine_.dy_ = -affine_.dy_ * invert.affine_.m22_;
            }
            break;
        case TransformationType::Rotate:
        case TransformationType::Shear: invert.affine_ = affine_.inverted(&inv); break;
        default:
            // general case
            double det = determinant();
            inv = !RealIsNull(det);
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

void Transform::mapElement(double &nx, double &ny, TransformationType t) const {
    double FX_ = nx;
    double FY_ = ny;
    switch (t) {
        case TransformationType::None:
            nx = FX_;
            ny = FY_;
            break;
        case TransformationType::Translate:
            nx = FX_ + affine_.dx_;
            ny = FY_ + affine_.dy_;
            break;
        case TransformationType::Scale:
            nx = affine_.m11_ * FX_ + affine_.dx_;
            ny = affine_.m22_ * FY_ + affine_.dy_;
            break;
        case TransformationType::Rotate:
        case TransformationType::Shear:
        case TransformationType::Project:
            nx = affine_.m11_ * FX_ + affine_.m21_ * FY_ + affine_.dx_;
            ny = affine_.m12_ * FX_ + affine_.m22_ * FY_ + affine_.dy_;
            if (t == TransformationType::Project) {
                double w = (m13_ * FX_ + m23_ * FY_ + m33_);
                if (w < NEAR_CLIP) {
                    w = NEAR_CLIP;
                }
                w = 1. / w;
                nx *= w;
                ny *= w;
            }
    }
}
