// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "fontfacedu.h"

namespace muse::draw {
struct FontFace {
    FontFace(const FontFace &) = delete;
    FontFace &operator=(const FontFace &) = delete;

    FontFace() = default;

    bool load(const FaceKey &key, const FontData &font_data, bool is_symbol_mode) { return face_.load(key, font_data, is_symbol_mode); }

    const FaceKey &key() const { return face_.key(); }

    bool is_symbol_mode() const { return face_.is_symbol_mode(); }

    f26dot6_t leading() const { return face_.leading(); }

    f26dot6_t ascent() const { return face_.ascent(); }

    f26dot6_t descent() const { return face_.descent(); }

    f26dot6_t x_height() const { return face_.x_height(); }

    f26dot6_t cap_height() const { return face_.cap_height(); }

    f26dot6_t underline_pos() const { return face_.underline_pos(); }

    f26dot6_t line_width() const { return face_.line_width(); }

    std::vector<ShapedGlyph> glyphs(const char32_t *text, int text_length) const { return face_.glyphs(text, text_length); }

    glyph_idx_t glyph_index(char32_t ucs4) const { return face_.glyph_index(ucs4); }

    glyph_idx_t glyph_index(const std::string &glyph_name) const { return face_.glyph_index(glyph_name); }

    char32_t find_char_code(glyph_idx_t idx) const { return face_.find_char_code(idx); }

    FBBox glyph_bbox(glyph_idx_t idx) const { return face_.glyph_bbox(idx); }

    f26dot6_t glyph_advance(glyph_idx_t idx) const { return face_.glyph_advance(idx); }

    const GlyphImage &glyph_image(glyph_idx_t idx) const { return face_.glyph_image(idx); }

private:
    FontFaceDU face_;
};
} // namespace muse::draw
