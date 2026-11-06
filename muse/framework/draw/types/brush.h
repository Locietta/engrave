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
        Color color = Color::black;

        inline bool operator==(const ColorStop &o) const { return real_is_equal(position, o.position) && color == o.color; }

        inline bool operator!=(const ColorStop &o) const { return !this->operator==(o); }
    };

    LinearGradient() = default;

    LinearGradient(const PointF &start, const PointF &final_stop) : start_(start), final_stop_(final_stop) {}

    inline const PointF &start() const { return start_; }

    void set_start(const PointF &start) { start_ = start; }

    inline const PointF &final_stop() const { return final_stop_; }

    void set_final_stop(const PointF &final_stop) { final_stop_ = final_stop; }

    void set_color_at(double position, const Color &color) {
        for (ColorStop &stop : color_stops_) {
            if (real_is_equal(stop.position, position)) {
                stop.color = color;
                return;
            }
        }

        auto it = std::lower_bound(color_stops_.begin(), color_stops_.end(), position,
                                   [](const ColorStop &stop, double pos) { return stop.position < pos; });
        color_stops_.insert(it, {position, color});
    }

    inline const std::vector<ColorStop> &color_stops() const { return color_stops_; }

    inline bool operator==(const LinearGradient &o) const {
        return start_ == o.start_ && final_stop_ == o.final_stop_ && color_stops_ == o.color_stops_;
    }

    inline bool operator!=(const LinearGradient &o) const { return !this->operator==(o); }

private:
    PointF start_;
    PointF final_stop_;
    std::vector<ColorStop> color_stops_;
};

class Brush {
public:
    Brush() {}

    Brush(BrushStyle style) : style_(style) {}

    Brush(const Color &color) : color_(color) {}

    Brush(const LinearGradient &gradient) : style_(BrushStyle::LINEAR_GRADIENT_PATTERN), linear_gradient_(gradient) {}

    inline bool operator==(const Brush &o) const {
        if (style_ != o.style_) {
            return false;
        }

        if (style_ == BrushStyle::LINEAR_GRADIENT_PATTERN) {
            return linear_gradient_ == o.linear_gradient_;
        }

        return color_ == o.color_;
    }

    inline bool operator!=(const Brush &o) const { return !this->operator==(o); }

    inline BrushStyle style() const { return style_; }

    void set_style(BrushStyle style) { style_ = style; }

    inline const Color &color() const { return color_; }

    void set_color(const Color &color) { color_ = color; }

    inline const LinearGradient &linear_gradient() const { return linear_gradient_; }

    void set_linear_gradient(const LinearGradient &gradient) {
        style_ = BrushStyle::LINEAR_GRADIENT_PATTERN;
        linear_gradient_ = gradient;
    }


private:
    Color color_ = Color::black;
    BrushStyle style_ = BrushStyle::SOLID_PATTERN;
    LinearGradient linear_gradient_;
};
} // namespace muse::draw

#endif // MUSE_DRAW_BRUSH_H
