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

using namespace muse::draw;

struct DummyGlyph {
    FBBox textBbox;
    f26dot6_t textAdvance = 0;
    FBBox symBbox;
    f26dot6_t symAdvance = 0;
};

static const DummyGlyph &dummyGlyph() {
    static DummyGlyph g;
    if (g.textAdvance == 0) {
        g.textBbox = FBBox(0, -8064, 4160, 9728);
        g.textAdvance = 4160;
        g.symBbox = FBBox(0, -4011, 2079, 4817);
        g.symAdvance = 2080;
    }

    return g;
}

FontFaceDU::FontFaceDU(IFontFace *origin) : origin_(origin) {}

FontFaceDU::~FontFaceDU() { delete origin_; }

bool FontFaceDU::load(const FaceKey &key, const FontData &fontData, bool isSymbolMode) {
    return origin_->load(key, fontData, isSymbolMode);
}

const FaceKey &FontFaceDU::key() const { return origin_->key(); }

bool FontFaceDU::isSymbolMode() const { return origin_->isSymbolMode(); }

f26dot6_t FontFaceDU::leading() const { return origin_->leading(); }

f26dot6_t FontFaceDU::ascent() const { return origin_->ascent(); }

f26dot6_t FontFaceDU::descent() const { return origin_->descent(); }

f26dot6_t FontFaceDU::xHeight() const { return origin_->xHeight(); }

f26dot6_t FontFaceDU::capHeight() const { return origin_->capHeight(); }

f26dot6_t FontFaceDU::underlinePos() const { return origin_->underlinePos(); }

f26dot6_t FontFaceDU::lineWidth() const { return origin_->lineWidth(); }

std::vector<ShapedGlyph> FontFaceDU::glyphs(const char32_t *text, int text_length) const {
    std::vector<ShapedGlyph> glyphs = origin_->glyphs(text, text_length);
    std::vector<ShapedGlyph> ret;
    ret.reserve(glyphs.size());
    for (ShapedGlyph &gp : glyphs) {
        if (gp.idx == 0) {
            gp.x_advance = glyphAdvance(0); // dummy
        }
        ret.push_back(gp);
    }
    return ret;
}

glyph_idx_t FontFaceDU::glyphIndex(char32_t ucs4) const { return origin_->glyphIndex(ucs4); }

glyph_idx_t FontFaceDU::glyphIndex(const std::string &glyphName) const { return origin_->glyphIndex(glyphName); }

char32_t FontFaceDU::findCharCode(glyph_idx_t idx) const { return origin_->findCharCode(idx); }

FBBox FontFaceDU::glyphBbox(glyph_idx_t idx) const {
    if (idx == 0) {
        return origin_->isSymbolMode() ? dummyGlyph().symBbox : dummyGlyph().textBbox;
    }
    return origin_->glyphBbox(idx);
}

f26dot6_t FontFaceDU::glyphAdvance(glyph_idx_t idx) const {
    if (idx == 0) {
        return origin_->isSymbolMode() ? dummyGlyph().symAdvance : dummyGlyph().textAdvance;
    }
    return origin_->glyphAdvance(idx);
}

const GlyphImage &FontFaceDU::glyphImage(glyph_idx_t idx) const {
    if (idx == 0) {
        static const GlyphImage emptyImage;
        return emptyImage;
    }

    return origin_->glyphImage(idx);
}
