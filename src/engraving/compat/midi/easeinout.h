/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#ifndef MU_ENGRAVING_EASEINOUT_H
#define MU_ENGRAVING_EASEINOUT_H

#include <vector>

#include "draw/types/geometry.h"

namespace mu::engraving {
//---------------------------------------------------------
//   @@ EaseInOut
///   \brief Specialized transfer curve using an underlying Bezier curve.
///
///   The second and third control points of the Bezier curve are moveable
///   along the x axis according to two parameters 'EaseIn' and 'EaseOut',
///   which results in a transfer curve that goes from a straight line to an S
///   shaped curve.
//---------------------------------------------------------

class EaseInOut final {
public:
    EaseInOut() : ease_in_(0.0), ease_out_(1.0) {}

    EaseInOut(double ease_in, double ease_out) : ease_in_(ease_in), ease_out_(ease_out) {}

    void set_eases(double ease_in, double ease_out) {
        ease_in_ = ease_in;
        ease_out_ = ease_out;
    }

    double eval_x(const double t) const {
        double t_compl = 1.0 - t;
        return (3.0 * ease_in_ * t_compl * t_compl + (3.0 - 3.0 * ease_out_ * t_compl - 2.0 * t) * t) * t;
    }

    double eval_y(const double t) const { return -(t * t) * (2.0 * t - 3.0); }

    muse::PointF eval(const double t) const { return {eval_x(t), eval_y(t)}; }

    double t_from_x(const double x) const;
    double t_from_y(const double y) const;

    double yfrom_x(const double x) const { return eval_y(t_from_x(x)); }

    double xfrom_y(const double y) const { return eval_x(t_from_y(y)); }

    void time_list(const int nb_notes, const int duration, std::vector<int> *times) const;

private:
    double ease_in_ = 0.0;
    double ease_out_ = 0.0;
};
} // namespace mu::engraving
#endif
