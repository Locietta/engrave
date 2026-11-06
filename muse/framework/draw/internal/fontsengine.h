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
#include <vector>

#include "fontrendercache.h"
#include "ifontsdatabase.h"
#include "ifontsengine.h"

namespace muse::draw {
struct IFontFace;

struct FontsEngine : IFontsEngine {
private:
    IFontsDatabase &database_;
    mutable uint64_t database_revision_ = 0;

public:
    explicit FontsEngine(IFontsDatabase &database) : database_(database) {}

    ~FontsEngine();

    void init();
    void set_render_cache_dir_path(const io::PathT &path, const std::string &revision = std::string()) override;
    void clear_loaded_faces() override;

    double line_spacing(const Font &f) const override;
    double x_height(const Font &f) const override;
    double height(const Font &f) const override;
    double cap_height(const Font &f) const override;
    double ascent(const Font &f) const override;
    double descent(const Font &f) const override;

    double underline_pos(const Font &f) const override;
    double line_width(const Font &f) const override;
    double strike_out_pos(const Font &f) const override;

    bool in_font(const Font &f, char32_t ucs4) const override;

    double horizontal_advance(const Font &f, const char32_t &ch) const override;
    double horizontal_advance(const Font &f, const std::u32string &text) const override;

    RectF bounding_rect(const Font &f, const char32_t &ch) const override;
    RectF bounding_rect(const Font &f, const std::u32string &text) const override;
    RectF tight_bounding_rect(const Font &f, const std::u32string &text) const override;

    // For draw
    std::vector<GlyphImage> render(const Font &f, const std::u32string &text) const override;

    // For dev
    using FontFaceFactory = std::function<IFontFace *(const FontDataKey &data_key, Font::Type type)>;
    void set_font_face_factory(const FontFaceFactory &f);

private:

    struct TextBlock {
        const char32_t *text = nullptr;
        int lenght = 0;
    };

    struct FontFaceTextBlock {
        TextBlock text;
        const IFontFace *face = nullptr;
    };

    struct RequireFace {
        IFontFace *face = nullptr; // real loaded face
        std::vector<IFontFace *> subtitution_faces;
        FaceKey require_key;         // require face
        FontDataKey actual_data_key; // resolved face

        bool is_symbol_mode() const;
        double pixel_scale() const;
        double pixel_scale_for(const IFontFace *loaded_face) const;
    };

    IFontFace *create_font_face(const FontData &font_data, Font::Type type) const;
    RequireFace *font_face(const Font &f, bool is_symbol_mode = false) const;
    IFontFace *font_face_by_actual_data_key(const FontDataKey &actual_data_key, Font::Type type, int loaded_pixel_size,
                                            bool is_symbol_mode) const;
    IFontFace *sdf_font_face_for(const IFontFace *layout_face) const;

    std::vector<FontFaceTextBlock> split_text_by_font_faces(const RequireFace *rf, const TextBlock &text) const;

    FontFaceFactory font_face_factory_;

    mutable std::vector<IFontFace *> loaded_faces_;
    mutable std::vector<RequireFace *> required_faces_;

    mutable FontRenderCache render_cache_;
};
} // namespace muse::draw
