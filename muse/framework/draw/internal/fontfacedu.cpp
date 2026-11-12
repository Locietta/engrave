/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "fontfacedu.h"

#include <utility>

using namespace muse::draw;

struct DummyGlyph {
    FBBox text_bbox;
    f26dot6_t text_advance = 0;
    FBBox sym_bbox;
    f26dot6_t sym_advance = 0;
};

static const DummyGlyph &dummy_glyph() {
    static const DummyGlyph g{FBBox(0, -8064, 4160, 9728), 4160, FBBox(0, -4011, 2079, 4817), 2080};

    return g;
}

bool FontFaceDU::load(const FaceKey &key, FontData font_data, bool is_symbol_mode) {
    return origin_.load(key, std::move(font_data), is_symbol_mode);
}

const FaceKey &FontFaceDU::key() const { return origin_.key(); }

bool FontFaceDU::is_symbol_mode() const { return origin_.is_symbol_mode(); }

f26dot6_t FontFaceDU::leading() const { return origin_.leading(); }

f26dot6_t FontFaceDU::ascent() const { return origin_.ascent(); }

f26dot6_t FontFaceDU::descent() const { return origin_.descent(); }

f26dot6_t FontFaceDU::x_height() const { return origin_.x_height(); }

f26dot6_t FontFaceDU::cap_height() const { return origin_.cap_height(); }

f26dot6_t FontFaceDU::underline_pos() const { return origin_.underline_pos(); }

f26dot6_t FontFaceDU::line_width() const { return origin_.line_width(); }

std::vector<ShapedGlyph> FontFaceDU::glyphs(const char32_t *text, int text_length) const {
    std::vector<ShapedGlyph> glyphs = origin_.glyphs(text, text_length);
    std::vector<ShapedGlyph> ret;
    ret.reserve(glyphs.size());
    for (ShapedGlyph &gp : glyphs) {
        if (gp.idx == 0) {
            gp.x_advance = glyph_advance(0); // dummy
        }
        ret.push_back(gp);
    }
    return ret;
}

glyph_idx_t FontFaceDU::glyph_index(char32_t ucs4) const { return origin_.glyph_index(ucs4); }

glyph_idx_t FontFaceDU::glyph_index(const std::string &glyph_name) const { return origin_.glyph_index(glyph_name); }

char32_t FontFaceDU::find_char_code(glyph_idx_t idx) const { return origin_.find_char_code(idx); }

FBBox FontFaceDU::glyph_bbox(glyph_idx_t idx) const {
    if (idx == 0) {
        return origin_.is_symbol_mode() ? dummy_glyph().sym_bbox : dummy_glyph().text_bbox;
    }
    return origin_.glyph_bbox(idx);
}

f26dot6_t FontFaceDU::glyph_advance(glyph_idx_t idx) const {
    if (idx == 0) {
        return origin_.is_symbol_mode() ? dummy_glyph().sym_advance : dummy_glyph().text_advance;
    }
    return origin_.glyph_advance(idx);
}

const GlyphImage &FontFaceDU::glyph_image(glyph_idx_t idx) const {
    if (idx == 0) {
        static const GlyphImage empty_image;
        return empty_image;
    }

    return origin_.glyph_image(idx);
}
