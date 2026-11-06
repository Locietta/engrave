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

#ifndef MUSE_DRAW_TRANSFORM_H
#define MUSE_DRAW_TRANSFORM_H

#include "geometry.h"
#include "global/realfn.h"
#include "matrix.h"

namespace muse::draw {
struct PainterPath;

struct Transform {

    enum class TransformationType { NONE = 0x00, TRANSLATE = 0x01, SCALE = 0x02, ROTATE = 0x04, SHEAR = 0x08, PROJECT = 0x10 };

    Transform() = default;

    Transform(double h11, double h12, double h21, double h22, double dx, double dy);

    Transform operator*(const Transform &m) const;
    Transform &operator*=(const Transform &o);
    Transform &operator*=(double num);
    Transform &operator/=(double div);

    bool operator==(const Transform &other) const {
        return affine_ == other.affine_ && real_is_equal(m13_, other.m13_) && real_is_equal(m23_, other.m23_) &&
               real_is_equal(m33_, other.m33_);
    }

    bool operator!=(const Transform &other) const { return !this->operator==(other); }

    double m11() const { return affine_.m11_; }

    double m12() const { return affine_.m12_; }

    double m13() const { return m13_; }

    double m21() const { return affine_.m21_; }

    double m22() const { return affine_.m22_; }

    double m23() const { return m23_; }

    double m31() const { return affine_.dx_; }

    double m32() const { return affine_.dy_; }

    double m33() const { return m33_; }

    double dx() const { return affine_.dx_; }

    double dy() const { return affine_.dy_; }

    PointF map(const PointF &p) const;
    LineF map(const LineF &l) const;
    RectF map(const RectF &rect) const;
    PainterPath map(const PainterPath &path) const;

    const Matrix &to_affine() const { return affine_; }

    void set_matrix(double m11, double m12, double m13, double m21, double m22, double m23, double m31, double m32, double m33);

    void reset();

    Transform &rotate(double a);
    Transform &rotate_radians(double a);
    Transform &translate(double dx, double dy);
    Transform &scale(double sx, double sy);
    Transform &shear(double sh, double sv);
    Transform inverted() const;


private:

    Transform(double h11, double h12, double h13, double h21, double h22, double h23, double h31, double h32, double h33);

    inline double determinant() const {
        return affine_.m11_ * (m33_ * affine_.m22_ - affine_.dy_ * m23_) - affine_.m21_ * (m33_ * affine_.m12_ - affine_.dy_ * m13_) +
               affine_.dx_ * (m23_ * affine_.m12_ - affine_.m22_ * m13_);
    }

    Transform adjoint() const;

    Transform::TransformationType type() const;

    inline Transform::TransformationType inline_type() const {
        if (dirty_ == TransformationType::NONE) {
            return type_;
        }
        return type();
    }

    void map_element(double &nx, double &ny, TransformationType t) const;

    Matrix affine_{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    double m13_ = 0.0;
    double m23_ = 0.0;
    double m33_ = 1.0;

    mutable TransformationType type_ = TransformationType::NONE;
    mutable TransformationType dirty_ = TransformationType::NONE;
};

inline Transform &Transform::operator*=(double num) {
    if (real_is_equal(num, 1.)) {
        return *this;
    }
    affine_.m11_ *= num;
    affine_.m12_ *= num;
    m13_ *= num;
    affine_.m21_ *= num;
    affine_.m22_ *= num;
    m23_ *= num;
    affine_.dx_ *= num;
    affine_.dy_ *= num;
    m33_ *= num;
    if (dirty_ < TransformationType::SCALE) {
        dirty_ = TransformationType::SCALE;
    }
    return *this;
}

inline Transform &Transform::operator/=(double div) {
    if (real_is_null(div)) {
        return *this;
    }
    div = 1.0 / div;
    return operator*=(div);
}

inline PointF operator*(const PointF &p, const Transform &m) { return m.map(p); }

inline Transform operator/(const Transform &a, double n) {
    Transform t(a);
    t /= n;
    return t;
}
} // namespace muse::draw

#endif // MUSE_DRAW_TRANSFORM_H
