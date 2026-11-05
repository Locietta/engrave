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

#include "bezier.h"
#include "global/realfn.h"

using namespace muse;
using namespace muse::draw;

Bezier::Bezier(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
    : x1_(x1), y1_(y1), x2_(x2), y2_(y2), x3_(x3), y3_(y3), x4_(x4), y4_(y4) {}

Bezier Bezier::fromPoints(const PointF &p1, const PointF &p2, const PointF &p3, const PointF &p4) {
    return {p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y()};
}

void Bezier::coefficients(double t, double &a, double &b, double &c, double &d) {
    double t1 = 1. - t;
    b = t1 * t1;
    c = t * t;
    d = c * t;
    a = b * t1;
    b *= 3. * t;
    c *= 3. * t1;
}

Bezier Bezier::bezierOnInterval(double t0, double t1) const {
    if (RealIsNull(t0) && RealIsEqual(t1, 1)) {
        return *this;
    }
    Bezier bezier = *this;
    Bezier result;
    bezier.parameterSplitLeft(t0, &result);
    double trueT = (t1 - t0) / (1 - t0);
    bezier.parameterSplitLeft(trueT, &result);
    return result;
}

void Bezier::parameterSplitLeft(double t, Bezier *left) {
    left->x1_ = x1_;
    left->y1_ = y1_;
    left->x2_ = x1_ + t * (x2_ - x1_);
    left->y2_ = y1_ + t * (y2_ - y1_);
    left->x3_ = x2_ + t * (x3_ - x2_); // temporary holding spot
    left->y3_ = y2_ + t * (y3_ - y2_); // temporary holding spot
    x3_ = x3_ + t * (x4_ - x3_);
    y3_ = y3_ + t * (y4_ - y3_);
    x2_ = left->x3_ + t * (x3_ - left->x3_);
    y2_ = left->y3_ + t * (y3_ - left->y3_);
    left->x3_ = left->x2_ + t * (left->x3_ - left->x2_);
    left->y3_ = left->y2_ + t * (left->y3_ - left->y2_);
    left->x4_ = x1_ = left->x3_ + t * (x2_ - left->x3_);
    left->y4_ = y1_ = left->y3_ + t * (y2_ - left->y3_);
}

PointF Bezier::pointAt(double t) const {
    // numerically more stable:
    double x, y;
    double t1 = 1. - t;
    {
        double a = x1_ * t1 + x2_ * t;
        double b = x2_ * t1 + x3_ * t;
        double c = x3_ * t1 + x4_ * t;
        a = a * t1 + b * t;
        b = b * t1 + c * t;
        x = a * t1 + b * t;
    }
    {
        double a = y1_ * t1 + y2_ * t;
        double b = y2_ * t1 + y3_ * t;
        double c = y3_ * t1 + y4_ * t;
        a = a * t1 + b * t;
        b = b * t1 + c * t;
        y = a * t1 + b * t;
    }
    return PointF(x, y);
}
