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

#include <functional>
#include <string>
#include <vector>

#include "draw/types/font.h"
#include "draw/types/fontstypes.h"
#include "draw/types/geometry.h"
#include "engrave/src/runtime/fonts_database.h"
#include "fontrendercache.h"
#include "global/io/path.h"

namespace muse::draw {
struct FontFace;

struct FontsEngine {
private:
    music::RuntimeFontsDatabase &database_;
    mutable uint64_t database_revision_ = 0;

public:
    explicit FontsEngine(music::RuntimeFontsDatabase &database) : database_(database) {}

    ~FontsEngine();

    void init();
    void set_render_cache_dir_path(const io::PathT &path, const std::string &revision = std::string());
    void clear_loaded_faces();

    double line_spacing(const Font &f) const;
    double x_height(const Font &f) const;
    double height(const Font &f) const;
    double cap_height(const Font &f) const;
    double ascent(const Font &f) const;
    double descent(const Font &f) const;

    double underline_pos(const Font &f) const;
    double line_width(const Font &f) const;
    double strike_out_pos(const Font &f) const;

    bool in_font(const Font &f, char32_t ucs4) const;

    double horizontal_advance(const Font &f, const char32_t &ch) const;
    double horizontal_advance(const Font &f, const std::u32string &text) const;

    RectF bounding_rect(const Font &f, const char32_t &ch) const;
    RectF bounding_rect(const Font &f, const std::u32string &text) const;
    RectF tight_bounding_rect(const Font &f, const std::u32string &text) const;

    // For draw
    std::vector<GlyphImage> render(const Font &f, const std::u32string &text) const;


private:

    struct TextBlock {
        const char32_t *text = nullptr;
        int lenght = 0;
    };

    struct FontFaceTextBlock {
        TextBlock text;
        const FontFace *face = nullptr;
    };

    struct RequireFace {
        FontFace *face = nullptr; // real loaded face
        std::vector<FontFace *> subtitution_faces;
        FaceKey require_key;         // require face
        FontDataKey actual_data_key; // resolved face

        bool is_symbol_mode() const;
        double pixel_scale() const;
        double pixel_scale_for(const FontFace *loaded_face) const;
    };

    RequireFace *font_face(const Font &f, bool is_symbol_mode = false) const;
    FontFace *font_face_by_actual_data_key(const FontDataKey &actual_data_key, Font::Type type, int loaded_pixel_size,
                                           bool is_symbol_mode) const;
    FontFace *sdf_font_face_for(const FontFace *layout_face) const;

    std::vector<FontFaceTextBlock> split_text_by_font_faces(const RequireFace *rf, const TextBlock &text) const;

    mutable std::vector<FontFace *> loaded_faces_;
    mutable std::vector<RequireFace *> required_faces_;

    mutable FontRenderCache render_cache_;
};
} // namespace muse::draw
