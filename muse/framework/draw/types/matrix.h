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

#ifndef MUSE_DRAW_MATRIX_H
#define MUSE_DRAW_MATRIX_H

#include "global/realfn.h"

namespace muse::draw {
struct Matrix {
    Matrix() {}

    Matrix(double m11, double m12, double m21, double m22, double dx, double dy)
        : m11_(m11), m12_(m12), m21_(m21), m22_(m22), dx_(dx), dy_(dy) {}

    Matrix inverted(bool *invertible) const {
        double dtr = determinant();
        if (muse::real_is_null(dtr)) {
            if (invertible) {
                *invertible = false;
            }
            return Matrix();
        } else {
            if (invertible) {
                *invertible = true;
            }
            double dinv = 1.0 / dtr;
            return Matrix((m22_ * dinv), (-m12_ * dinv), (-m21_ * dinv), (m11_ * dinv), ((m21_ * dy_ - m22_ * dx_) * dinv),
                          ((m12_ * dx_ - m11_ * dy_) * dinv));
        }
    }

    double determinant() const { return m11_ * m22_ - m12_ * m21_; }

    double m11() const { return m11_; }

    double m12() const { return m12_; }

    double m21() const { return m21_; }

    double m22() const { return m22_; }

    double dx() const { return dx_; }

    double dy() const { return dy_; }

    bool operator==(const Matrix &other) const {
        return real_is_equal(m11_, other.m11_) && real_is_equal(m12_, other.m12_) && real_is_equal(m21_, other.m21_) &&
               real_is_equal(m22_, other.m22_) && real_is_equal(dx_, other.dx_) && real_is_equal(dy_, other.dy_);
    }

private:
    friend struct Transform;

    double m11_ = 1;
    double m12_ = 0;
    double m21_ = 0;
    double m22_ = 1;

    double dx_ = 0;
    double dy_ = 0;
};
} // namespace muse::draw

#endif // MUSE_DRAW_MATRIX_H
