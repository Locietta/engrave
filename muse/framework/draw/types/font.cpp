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

bool Font::g_disableFontMerging = false;

Font::Font(const FontFamily &family, Type type) : family_(family), type_(type) {}

bool Font::operator==(const Font &other) const {
    //! NOTE At the moment, the type is entered for information,
    //! its correct installation is not guaranteed,
    //! so we do not take it when comparing it yet
    // && type_ == other.type_

    return family_ == other.family_ && RealIsEqual(pointSizeF_, other.pointSizeF_) && weight_ == other.weight_ && style_ == other.style_ &&
           noFontMerging_ == other.noFontMerging_ && hinting_ == other.hinting_;
}

void Font::setFamily(const FontFamily &family, Type type) {
    family_ = family;
    type_ = type;
}

Font::FontFamily Font::family() const { return family_; }

Font::Type Font::type() const { return type_; }

double Font::pointSizeF() const { return pointSizeF_; }

void Font::setPointSizeF(double s) {
    pointSizeF_ = s;
    pixelSize_ = -1;
}

int Font::pixelSize() const { return pixelSize_; }

void Font::setPixelSize(int s) {
    pixelSize_ = s;
    pointSizeF_ = -1.0;
}

Font::Weight Font::weight() const { return weight_; }

void Font::setWeight(Weight w) { weight_ = w; }

bool Font::bold() const { return style_.testFlag(Style::Bold); }

void Font::setBold(bool arg) { style_.setFlag(Style::Bold, arg); }

bool Font::italic() const { return style_.testFlag(Style::Italic); }

void Font::setItalic(bool arg) { style_.setFlag(Style::Italic, arg); }

bool Font::underline() const { return style_.testFlag(Style::Underline); }

void Font::setUnderline(bool arg) { style_.setFlag(Style::Underline, arg); }

bool Font::strike() const { return style_.testFlag(Style::Strike); }

void Font::setStrike(bool arg) { style_.setFlag(Style::Strike, arg); }

void Font::setNoFontMerging(bool arg) { noFontMerging_ = arg; }

bool Font::noFontMerging() const {
    if (g_disableFontMerging) {
        return true;
    }
    return noFontMerging_;
}

Font::Hinting Font::hinting() const { return hinting_; }

void Font::setHinting(Hinting hinting) { hinting_ = hinting; }
