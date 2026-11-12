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

#include "color.h"
#include "drawtypes.h"
#include "global/realfn.h"

namespace muse::draw {
struct Pen {

    Pen(PenStyle style) : style_(style) {}

    Pen(const Color &color = Color::black, double width = 1, PenStyle s = PenStyle::SOLID_LINE, PenCapStyle c = PenCapStyle::SQUARE_CAP,
        PenJoinStyle j = PenJoinStyle::BEVEL_JOIN)
        : color_(color), width_(width), style_(s), cap_style_(c), join_style_(j) {}

    inline bool operator==(const Pen &o) const {
        return color_ == o.color_ && real_is_equal(width_, o.width_) && style_ == o.style_ && cap_style_ == o.cap_style_ &&
               join_style_ == o.join_style_ && real_is_equal(miter_limit_, o.miter_limit_) && real_is_equal(dash_pattern_, o.dash_pattern_);
    }

    inline bool operator!=(const Pen &o) const { return !this->operator==(o); }

    PenStyle style() const { return style_; }

    void set_style(PenStyle style) {
        if (style_ == style) {
            return;
        }
        style_ = style;
        dash_pattern_.clear();
    }

    std::vector<double> dash_pattern() const {
        switch (style_) {
            case PenStyle::DASH_LINE: return {4, 2};
            case PenStyle::DOT_LINE: return {1, 2};
            case PenStyle::DASH_DOT_LINE: return {4, 2, 1, 2};
            case PenStyle::DASH_DOT_DOT_LINE: return {4, 2, 1, 2, 1, 2};
            case PenStyle::CUSTOM_DASH_LINE: return dash_pattern_;
            default: return {};
        }
    }

    void set_dash_pattern(const std::vector<double> &pattern) {
        if (pattern.empty()) {
            return;
        }
        dash_pattern_ = pattern;
        style_ = PenStyle::CUSTOM_DASH_LINE;
        if ((dash_pattern_.size() % 2) == 1) {
            dash_pattern_.push_back(1);
        }
    }

    double width_f() const { return width_; }

    void set_width_f(double width) { width_ = width; }

    Color color() const { return color_; }

    void set_color(const Color &color) { color_ = color; }

    PenCapStyle cap_style() const { return cap_style_; }

    void set_cap_style(PenCapStyle pcs) { cap_style_ = pcs; }

    PenJoinStyle join_style() const { return join_style_; }

    void set_join_style(PenJoinStyle pcs) { join_style_ = pcs; }

    double miter_limit() const { return miter_limit_; }

    void set_miter_limit(double limit) { miter_limit_ = limit; }


private:

    Color color_ = Color::black;
    double width_ = 1.0;
    PenStyle style_ = PenStyle::SOLID_LINE;
    PenCapStyle cap_style_ = PenCapStyle::SQUARE_CAP;
    PenJoinStyle join_style_ = PenJoinStyle::BEVEL_JOIN;
    double miter_limit_ = 2.0;
    std::vector<double> dash_pattern_;
};
} // namespace muse::draw

#endif // MUSE_DRAW_PEN_H
