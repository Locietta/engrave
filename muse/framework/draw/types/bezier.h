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

#ifndef MUSE_DRAW_BEZIER_H
#define MUSE_DRAW_BEZIER_H

#include "geometry.h"

namespace muse::draw {
class Bezier {
public:
    Bezier() = default;
    Bezier(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);

    static Bezier from_points(const PointF &p1, const PointF &p2, const PointF &p3, const PointF &p4);
    static void coefficients(double t, double &a, double &b, double &c, double &d);
    Bezier bezier_on_interval(double t0, double t1) const;

    PointF pt1() const { return PointF(x1_, y1_); }

    PointF pt2() const { return PointF(x2_, y2_); }

    PointF pt3() const { return PointF(x3_, y3_); }

    PointF pt4() const { return PointF(x4_, y4_); }

    PointF point_at(double t) const;

private:
    void parameter_split_left(double t, Bezier *left);

    friend class PainterPath;

    double x1_, y1_, x2_, y2_, x3_, y3_, x4_, y4_;
};
} // namespace muse::draw

#endif // MUSE_DRAW_BEZIER_H
