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
#pragma once

#include "ifontface.h"

namespace muse::draw {
struct FData;
struct GlyphMetrics;
struct SymbolMetrics;

class FontFaceFT : public IFontFace {
public:

    FontFaceFT();
    ~FontFaceFT();

    bool load(const FaceKey &key, const FontData &font_data, bool is_symbol_mode) override;

    const FaceKey &key() const override;
    bool is_symbol_mode() const override;

    f26dot6_t leading() const override;
    f26dot6_t ascent() const override;
    f26dot6_t descent() const override;
    f26dot6_t x_height() const override;
    f26dot6_t cap_height() const override;

    f26dot6_t underline_pos() const override;
    f26dot6_t line_width() const override;

    std::vector<ShapedGlyph> glyphs(const char32_t *text, int text_length) const override;
    glyph_idx_t glyph_index(char32_t ucs4) const override;
    glyph_idx_t glyph_index(const std::string &glyph_name) const override;
    char32_t find_char_code(glyph_idx_t idx) const override;

    FBBox glyph_bbox(glyph_idx_t idx) const override;
    f26dot6_t glyph_advance(glyph_idx_t idx) const override;

    const GlyphImage &glyph_image(glyph_idx_t idx) const override;

private:

    GlyphMetrics *glyph_metrics(glyph_idx_t idx) const;
    SymbolMetrics *symbol_metrics(glyph_idx_t idx) const;

    FaceKey key_;
    bool is_symbol_mode_ = false;
    FData *data_ = nullptr;
    mutable std::unordered_map<glyph_idx_t, GlyphImage> cache_;
};
} // namespace muse::draw
