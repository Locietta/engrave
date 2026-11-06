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

void FontMetrics::set_provider(IFontProvider *provider) { s_provider = provider; }

IFontProvider *FontMetrics::font_provider() { return s_provider; }

FontMetrics::FontMetrics(const Font &font) : font_(font) {}

double FontMetrics::line_spacing() const { return font_provider()->line_spacing(font_); }

double FontMetrics::x_height() const { return font_provider()->x_height(font_); }

double FontMetrics::height() const { return font_provider()->height(font_); }

double FontMetrics::cap_height() const { return font_provider()->cap_height(font_); }

double FontMetrics::ascent() const { return font_provider()->ascent(font_); }

double FontMetrics::descent() const { return font_provider()->descent(font_); }

double FontMetrics::underline_pos() const { return font_provider()->underline_pos(font_); }

double FontMetrics::line_width() const { return font_provider()->line_width(font_); }

double FontMetrics::strike_out_pos() const { return font_provider()->strike_out_pos(font_); }

double FontMetrics::width(const String &string) const { return horizontal_advance(string); }

double FontMetrics::width(const Char &ch) const { return horizontal_advance(ch); }

double FontMetrics::horizontal_advance(const String &string) const { return font_provider()->horizontal_advance(font_, string); }

double FontMetrics::horizontal_advance(char32_t ucs4) const { return font_provider()->horizontal_advance(font_, ucs4); }

RectF FontMetrics::bounding_rect(const String &string) const { return font_provider()->bounding_rect(font_, string); }

RectF FontMetrics::bounding_rect(char32_t ucs4) const { return font_provider()->bounding_rect(font_, ucs4); }

RectF FontMetrics::tight_bounding_rect(const String &string) const { return font_provider()->tight_bounding_rect(font_, string); }

RectF FontMetrics::tight_bounding_rect(const Char &ch) const { return font_provider()->tight_bounding_rect(font_, ch); }

bool FontMetrics::in_font(char32_t ucs4) const { return font_provider()->in_font(font_, ucs4); }

// Static

double FontMetrics::width(const Font &f, const String &string) { return FontMetrics(f).width(string); }

RectF FontMetrics::bounding_rect(const Font &f, const String &string) { return FontMetrics(f).bounding_rect(string); }

RectF FontMetrics::tight_bounding_rect(const Font &f, const String &string) { return FontMetrics(f).tight_bounding_rect(string); }

double FontMetrics::ascent(const Font &f) { return FontMetrics(f).ascent(); }

double FontMetrics::cap_height(const Font &f) { return FontMetrics(f).cap_height(); }
