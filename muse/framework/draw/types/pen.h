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
#ifndef MUSE_DRAW_PEN_H
#define MUSE_DRAW_PEN_H

#include "global/realfn.h"

#include "color.h"
#include "drawtypes.h"

namespace muse::draw {
class Pen {
public:

    Pen(PenStyle style) : style_(style) {}

    Pen(const Color &color = Color::BLACK, double width = 1, PenStyle s = PenStyle::SolidLine, PenCapStyle c = PenCapStyle::SquareCap,
        PenJoinStyle j = PenJoinStyle::BevelJoin)
        : color_(color), width_(width), style_(s), capStyle_(c), joinStyle_(j) {}

    inline bool operator==(const Pen &o) const {
        return color_ == o.color_ && RealIsEqual(width_, o.width_) && style_ == o.style_ && capStyle_ == o.capStyle_ &&
               joinStyle_ == o.joinStyle_ && RealIsEqual(miterLimit_, o.miterLimit_) && RealIsEqual(dashPattern_, o.dashPattern_);
    }

    inline bool operator!=(const Pen &o) const { return !this->operator==(o); }

    PenStyle style() const { return style_; }

    void setStyle(PenStyle style) {
        if (style_ == style) {
            return;
        }
        style_ = style;
        dashPattern_.clear();
    }

    std::vector<double> dashPattern() const {
        if (style_ == PenStyle::SolidLine || style_ == PenStyle::NoPen) {
            return std::vector<double>();
        } else if (dashPattern_.empty()) {
            const double space = 2;
            const double dot = 1;
            const double dash = 4;
            switch (style_) {
                case PenStyle::DashLine: dashPattern_ = {dash, space}; break;
                case PenStyle::DotLine: dashPattern_ = {dot, space}; break;
                case PenStyle::DashDotLine: dashPattern_ = {dash, space, dot, space}; break;
                case PenStyle::DashDotDotLine: dashPattern_ = {dash, space, dot, space, dot, space}; break;
                default: break;
            }
        }
        return dashPattern_;
    }

    void setDashPattern(const std::vector<double> &pattern) {
        if (pattern.empty()) {
            return;
        }
        dashPattern_ = pattern;
        style_ = PenStyle::CustomDashLine;
        if ((dashPattern_.size() % 2) == 1) {
            dashPattern_.push_back(1);
        }
    }

    double widthF() const { return width_; }

    void setWidthF(double width) { width_ = width; }

    Color color() const { return color_; }

    void setColor(const Color &color) { color_ = color; }

    PenCapStyle capStyle() const { return capStyle_; }

    void setCapStyle(PenCapStyle pcs) { capStyle_ = pcs; }

    PenJoinStyle joinStyle() const { return joinStyle_; }

    void setJoinStyle(PenJoinStyle pcs) { joinStyle_ = pcs; }

    double miterLimit() const { return miterLimit_; }

    void setMiterLimit(double limit) { miterLimit_ = limit; }


private:

    Color color_ = Color::BLACK;
    double width_ = 1.0;
    PenStyle style_ = PenStyle::SolidLine;
    PenCapStyle capStyle_ = PenCapStyle::SquareCap;
    PenJoinStyle joinStyle_ = PenJoinStyle::BevelJoin;
    double miterLimit_ = 2.0;
    mutable std::vector<double> dashPattern_;
};
} // namespace muse::draw

#endif // MUSE_DRAW_PEN_H
