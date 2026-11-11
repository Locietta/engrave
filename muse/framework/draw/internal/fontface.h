// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <utility>
#include <variant>

#include "fontfacedu.h"
#include "fontfaceft.h"

namespace muse::draw {
struct FontFace {
    template <typename T, typename... Args>
    explicit FontFace(std::in_place_type_t<T> type, Args &&...args) : face_(type, std::forward<Args>(args)...) {}

    bool load(const FaceKey &key, const FontData &font_data, bool is_symbol_mode) {
        return std::visit([&](auto &face) -> bool { return face.load(key, font_data, is_symbol_mode); }, face_);
    }

    const FaceKey &key() const {
        return std::visit([&](auto &face) -> const FaceKey & { return face.key(); }, face_);
    }

    bool is_symbol_mode() const {
        return std::visit([&](auto &face) -> bool { return face.is_symbol_mode(); }, face_);
    }

    f26dot6_t leading() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.leading(); }, face_);
    }

    f26dot6_t ascent() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.ascent(); }, face_);
    }

    f26dot6_t descent() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.descent(); }, face_);
    }

    f26dot6_t x_height() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.x_height(); }, face_);
    }

    f26dot6_t cap_height() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.cap_height(); }, face_);
    }

    f26dot6_t underline_pos() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.underline_pos(); }, face_);
    }

    f26dot6_t line_width() const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.line_width(); }, face_);
    }

    std::vector<ShapedGlyph> glyphs(const char32_t *text, int text_length) const {
        return std::visit([&](auto &face) -> std::vector<ShapedGlyph> { return face.glyphs(text, text_length); }, face_);
    }

    glyph_idx_t glyph_index(char32_t ucs4) const {
        return std::visit([&](auto &face) -> glyph_idx_t { return face.glyph_index(ucs4); }, face_);
    }

    glyph_idx_t glyph_index(const std::string &glyph_name) const {
        return std::visit([&](auto &face) -> glyph_idx_t { return face.glyph_index(glyph_name); }, face_);
    }

    char32_t find_char_code(glyph_idx_t idx) const {
        return std::visit([&](auto &face) -> char32_t { return face.find_char_code(idx); }, face_);
    }

    FBBox glyph_bbox(glyph_idx_t idx) const {
        return std::visit([&](auto &face) -> FBBox { return face.glyph_bbox(idx); }, face_);
    }

    f26dot6_t glyph_advance(glyph_idx_t idx) const {
        return std::visit([&](auto &face) -> f26dot6_t { return face.glyph_advance(idx); }, face_);
    }

    const GlyphImage &glyph_image(glyph_idx_t idx) const {
        return std::visit([&](auto &face) -> const GlyphImage & { return face.glyph_image(idx); }, face_);
    }
private:
    std::variant<FontFaceFT, FontFaceDU> face_;
};
} // namespace muse::draw
