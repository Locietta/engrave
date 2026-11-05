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
#include "fontmetrics.h"

using namespace muse;
using namespace muse::draw;

namespace {
IFontProvider *s_provider = nullptr;
} // namespace

void FontMetrics::setProvider(IFontProvider *provider) { s_provider = provider; }

IFontProvider *FontMetrics::fontProvider() { return s_provider; }

FontMetrics::FontMetrics(const Font &font) : font_(font) {}

double FontMetrics::lineSpacing() const { return fontProvider()->lineSpacing(font_); }

double FontMetrics::xHeight() const { return fontProvider()->xHeight(font_); }

double FontMetrics::height() const { return fontProvider()->height(font_); }

double FontMetrics::capHeight() const { return fontProvider()->capHeight(font_); }

double FontMetrics::ascent() const { return fontProvider()->ascent(font_); }

double FontMetrics::descent() const { return fontProvider()->descent(font_); }

double FontMetrics::underlinePos() const { return fontProvider()->underlinePos(font_); }

double FontMetrics::lineWidth() const { return fontProvider()->lineWidth(font_); }

double FontMetrics::strikeOutPos() const { return fontProvider()->strikeOutPos(font_); }

double FontMetrics::width(const String &string) const { return horizontalAdvance(string); }

double FontMetrics::width(const Char &ch) const { return horizontalAdvance(ch); }

double FontMetrics::horizontalAdvance(const String &string) const { return fontProvider()->horizontalAdvance(font_, string); }

double FontMetrics::horizontalAdvance(char32_t ucs4) const { return fontProvider()->horizontalAdvance(font_, ucs4); }

RectF FontMetrics::boundingRect(const String &string) const { return fontProvider()->boundingRect(font_, string); }

RectF FontMetrics::boundingRect(char32_t ucs4) const { return fontProvider()->boundingRect(font_, ucs4); }

RectF FontMetrics::tightBoundingRect(const String &string) const { return fontProvider()->tightBoundingRect(font_, string); }

RectF FontMetrics::tightBoundingRect(const Char &ch) const { return fontProvider()->tightBoundingRect(font_, ch); }

bool FontMetrics::inFont(char32_t ucs4) const { return fontProvider()->inFont(font_, ucs4); }

// Static

double FontMetrics::width(const Font &f, const String &string) { return FontMetrics(f).width(string); }

RectF FontMetrics::boundingRect(const Font &f, const String &string) { return FontMetrics(f).boundingRect(string); }

RectF FontMetrics::tightBoundingRect(const Font &f, const String &string) { return FontMetrics(f).tightBoundingRect(string); }

double FontMetrics::ascent(const Font &f) { return FontMetrics(f).ascent(); }

double FontMetrics::capHeight(const Font &f) { return FontMetrics(f).capHeight(); }
