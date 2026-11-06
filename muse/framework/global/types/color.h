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

#pragma once

#include <string>

#include "string.h"
#include "number.h"

#include "rgba.h"

namespace muse {
class Color {
public:
    Color();
    Color(const Color &other);

    constexpr Color(int r, int g, int b, int a = ::muse::Color::default_alpha)
        : rgba_(rgba(r, g, b, a)), is_valid_(is_rgba_valid(r, g, b, a)) {}

    Color(const char *color);

    ~Color() = default;

    Color &operator=(const Color &other);

    bool operator<(const Color &other) const;

    std::string to_string() const;
    static Color from_string(const std::string &str);
    static Color from_string(const char *str);
    static Color from_string(const String &str);

    void set_named_color(const std::string &color);
    void set_named_color(const char *color);
    void set_red(int value);
    void set_green(int value);
    void set_blue(int value);
    void set_alpha(int value);

    void apply_tint(double tint);

    Color inverted() const;

    bool operator==(const Color &other) const;
    bool operator!=(const Color &other) const;

    bool is_valid() const;

    int red() const;
    int green() const;
    int blue() const;
    int alpha() const;

    static constexpr int default_alpha = 255;

    static const Color transparent;
    static const Color black;
    static const Color white;
    static const Color red_color;
    static const Color green_color;
    static const Color blue_color;

private:

    void set_rgba(int r, int g, int b, int a);
    void set_rgba(Rgba rgb);

    Rgba rgba_ = rgba(0, 0, 0, ::muse::Color::default_alpha);
    bool is_valid_ = false;
};

inline const Color Color::transparent = {255, 255, 255, 0};
inline const Color Color::black = {0, 0, 0, 255};
inline const Color Color::white = {255, 255, 255, 255};
inline const Color Color::red_color = {255, 0, 0, 255};
inline const Color Color::green_color = {0, 255, 0, 255};
inline const Color Color::blue_color{0, 0, 255, 255};

inline Color blend_colors(const Color &c1, const Color &c2) {
    float alpha1 = c1.alpha() / 255.f;
    float alpha2 = c2.alpha() / 255.f;

    float alpha_out = alpha2 + alpha1 * (1 - alpha2);

    if (is_zero(alpha_out)) {
        return Color(0, 0, 0, 0);
    }
    int r = (c2.red() * alpha2 + c1.red() * alpha1 * (1 - alpha2)) / alpha_out;
    int g = (c2.green() * alpha2 + c1.green() * alpha1 * (1 - alpha2)) / alpha_out;
    int b = (c2.blue() * alpha2 + c1.blue() * alpha1 * (1 - alpha2)) / alpha_out;

    return Color(r, g, b, alpha_out * 255);
}

inline Color blend_colors(const Color &c1, const Color &c2, float alpha) {
    float alpha1 = 1.0;
    float alpha2 = alpha;

    int r = (c2.red() * alpha2 + c1.red() * alpha1 * (1 - alpha2));
    int g = (c2.green() * alpha2 + c1.green() * alpha1 * (1 - alpha2));
    int b = (c2.blue() * alpha2 + c1.blue() * alpha1 * (1 - alpha2));

    return Color(r, g, b, 255);
}

} // namespace muse
