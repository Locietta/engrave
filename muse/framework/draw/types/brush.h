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
#ifndef MUSE_DRAW_BRUSH_H
#define MUSE_DRAW_BRUSH_H

#include <algorithm>
#include <vector>

#include "color.h"
#include "drawtypes.h"
#include "geometry.h"
#include "global/realfn.h"

namespace muse::draw {
class LinearGradient {
public:
    struct ColorStop {
        double position = 0.0;
        Color color = Color::BLACK;

        inline bool operator==(const ColorStop &o) const { return RealIsEqual(position, o.position) && color == o.color; }

        inline bool operator!=(const ColorStop &o) const { return !this->operator==(o); }
    };

    LinearGradient() = default;

    LinearGradient(const PointF &start, const PointF &finalStop) : start_(start), finalStop_(finalStop) {}

    inline const PointF &start() const { return start_; }

    void setStart(const PointF &start) { start_ = start; }

    inline const PointF &finalStop() const { return finalStop_; }

    void setFinalStop(const PointF &finalStop) { finalStop_ = finalStop; }

    void setColorAt(double position, const Color &color) {
        for (ColorStop &stop : colorStops_) {
            if (RealIsEqual(stop.position, position)) {
                stop.color = color;
                return;
            }
        }

        auto it = std::lower_bound(colorStops_.begin(), colorStops_.end(), position,
                                   [](const ColorStop &stop, double pos) { return stop.position < pos; });
        colorStops_.insert(it, {position, color});
    }

    inline const std::vector<ColorStop> &colorStops() const { return colorStops_; }

    inline bool operator==(const LinearGradient &o) const {
        return start_ == o.start_ && finalStop_ == o.finalStop_ && colorStops_ == o.colorStops_;
    }

    inline bool operator!=(const LinearGradient &o) const { return !this->operator==(o); }

private:
    PointF start_;
    PointF finalStop_;
    std::vector<ColorStop> colorStops_;
};

class Brush {
public:
    Brush() {}

    Brush(BrushStyle style) : style_(style) {}

    Brush(const Color &color) : color_(color) {}

    Brush(const LinearGradient &gradient) : style_(BrushStyle::LinearGradientPattern), linearGradient_(gradient) {}

    inline bool operator==(const Brush &o) const {
        if (style_ != o.style_) {
            return false;
        }

        if (style_ == BrushStyle::LinearGradientPattern) {
            return linearGradient_ == o.linearGradient_;
        }

        return color_ == o.color_;
    }

    inline bool operator!=(const Brush &o) const { return !this->operator==(o); }

    inline BrushStyle style() const { return style_; }

    void setStyle(BrushStyle style) { style_ = style; }

    inline const Color &color() const { return color_; }

    void setColor(const Color &color) { color_ = color; }

    inline const LinearGradient &linearGradient() const { return linearGradient_; }

    void setLinearGradient(const LinearGradient &gradient) {
        style_ = BrushStyle::LinearGradientPattern;
        linearGradient_ = gradient;
    }


private:
    Color color_ = Color::BLACK;
    BrushStyle style_ = BrushStyle::SolidPattern;
    LinearGradient linearGradient_;
};
} // namespace muse::draw

#endif // MUSE_DRAW_BRUSH_H
