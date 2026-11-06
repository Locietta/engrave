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
#ifndef MUSE_DRAW_FONTMETRICS_H
#define MUSE_DRAW_FONTMETRICS_H

#include "global/types/string.h"

#include "types/font.h"
#include "types/geometry.h"

#include "ifontprovider.h"

namespace muse::draw {
class FontMetrics {
public:
    FontMetrics(const Font &font);

    // FontMetrics is a value type constructed throughout layout without an owner
    // to carry a provider, so EngineContext binds its font provider here for its
    // lifetime; null outside it.
    static void set_provider(IFontProvider *provider);

    double line_spacing() const;
    double x_height() const;
    double height() const;
    double cap_height() const;
    double ascent() const;
    double descent() const;

    double underline_pos() const;
    double line_width() const;
    double strike_out_pos() const;

    double width(const String &string) const;
    double width(const Char &ch) const;

    double horizontal_advance(const String &string) const;
    double horizontal_advance(char32_t ucs4) const;

    RectF bounding_rect(const String &string) const;
    RectF bounding_rect(char32_t ucs4) const;

    RectF tight_bounding_rect(const String &string) const;
    RectF tight_bounding_rect(const Char &ch) const;

    bool in_font(char32_t ucs4) const;

    static double width(const Font &f, const String &string);
    static RectF bounding_rect(const Font &f, const String &string);
    static RectF tight_bounding_rect(const Font &f, const String &string);
    static double ascent(const Font &f);
    static double cap_height(const Font &f);

private:
    static IFontProvider *font_provider();

    Font font_;
};
} // namespace muse::draw

#endif // MUSE_DRAW_FONTMETRICS_H
