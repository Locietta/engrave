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
#include "font.h"

#include "global/realfn.h"

using namespace muse;
using namespace muse::draw;

bool Font::g_disable_font_merging = false;

Font::Font(const FontFamily &family, Type type) : family_(family), type_(type) {}

bool Font::operator==(const Font &other) const {
    //! NOTE At the moment, the type is entered for information,
    //! its correct installation is not guaranteed,
    //! so we do not take it when comparing it yet
    // && type_ == other.type_

    return family_ == other.family_ && real_is_equal(point_size_f_, other.point_size_f_) && weight_ == other.weight_ &&
           style_ == other.style_ && no_font_merging_ == other.no_font_merging_ && hinting_ == other.hinting_;
}

void Font::set_family(const FontFamily &family, Type type) {
    family_ = family;
    type_ = type;
}

Font::FontFamily Font::family() const { return family_; }

Font::Type Font::type() const { return type_; }

double Font::point_size_f() const { return point_size_f_; }

void Font::set_point_size_f(double s) {
    point_size_f_ = s;
    pixel_size_ = -1;
}

int Font::pixel_size() const { return pixel_size_; }

void Font::set_pixel_size(int s) {
    pixel_size_ = s;
    point_size_f_ = -1.0;
}

Font::Weight Font::weight() const { return weight_; }

void Font::set_weight(Weight w) { weight_ = w; }

bool Font::bold() const { return style_.test_flag(Style::BOLD); }

void Font::set_bold(bool arg) { style_.set_flag(Style::BOLD, arg); }

bool Font::italic() const { return style_.test_flag(Style::ITALIC); }

void Font::set_italic(bool arg) { style_.set_flag(Style::ITALIC, arg); }

bool Font::underline() const { return style_.test_flag(Style::UNDERLINE); }

void Font::set_underline(bool arg) { style_.set_flag(Style::UNDERLINE, arg); }

bool Font::strike() const { return style_.test_flag(Style::STRIKE); }

void Font::set_strike(bool arg) { style_.set_flag(Style::STRIKE, arg); }

void Font::set_no_font_merging(bool arg) { no_font_merging_ = arg; }

bool Font::no_font_merging() const {
    if (g_disable_font_merging) {
        return true;
    }
    return no_font_merging_;
}

Font::Hinting Font::hinting() const { return hinting_; }

void Font::set_hinting(Hinting hinting) { hinting_ = hinting; }
