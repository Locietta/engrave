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
#include "fontsengine.h"

#include "fontfacedu.h"
#include "fontfaceft.h"
#include "ifontface.h"
#include "log.h"

using namespace muse;
using namespace muse::draw;

static bool is_ftx([[maybe_unused]] const ByteArray &data) { return false; }

static const double default_pixel_size = 100.0;
static const double loaded_pixel_size_constant = 200.0;
static const double font_metrics_dpi = 1200.0;
static constexpr double ppi = 72.0;

static int font_metrics_pixel_size(const Font &f) {
    if (f.pixel_size() > 0) {
        return f.pixel_size();
    }

    double pixel_size = f.point_size_f() * ::font_metrics_dpi / ::ppi;
    return static_cast<int>(std::round(pixel_size));
}

static FaceKey face_key_for_metrics_font(const Font &f) { return FaceKey(data_key_for_font(f), f.type(), font_metrics_pixel_size(f)); }

static inline RectF from_fb_box(const FBBox &bb, double scale) {
    return RectF(from_f26d6(bb.left()) * scale, from_f26d6(bb.top()) * scale, from_f26d6(bb.width()) * scale,
                 from_f26d6(bb.height()) * scale);
}

static inline RectF scale_rect(const RectF &r, double scale) {
    return RectF(r.x() * scale, r.y() * scale, r.width() * scale, r.height() * scale);
}

static const IFontFace *find_subtitution_font(char32_t ch, const std::vector<IFontFace *> &subtitution_faces) {
    const IFontFace *founded = nullptr;
    for (const IFontFace *sub_face : subtitution_faces) {
        if (sub_face->glyph_index(ch) != 0) {
            founded = sub_face;
        }
    }
    return founded;
}

static bool fits_qt_glyph_cache_metrics(const FBBox &bbox, f26dot6_t advance) {
    return (bbox.width() >> 6) <= 0xFF && (bbox.height() >> 6) <= 0xFF && advance <= 0x7FFF;
}

static bool is_zero_advance_char(char32_t ch) { return ch == U'\n' || ch == U'\r'; }

static bool is_zero_advance_text(const char32_t *text, int length) {
    for (int i = 0; i < length; ++i) {
        if (!is_zero_advance_char(text[i])) {
            return false;
        }
    }
    return true;
}

bool FontsEngine::RequireFace::is_symbol_mode() const { return face ? face->is_symbol_mode() : false; }

double FontsEngine::RequireFace::pixel_scale() const { return pixel_scale_for(face); }

double FontsEngine::RequireFace::pixel_scale_for(const IFontFace *loaded_face) const {
    if (!loaded_face) {
        return 0.0;
    }

    return static_cast<double>(require_key.pixel_size) / static_cast<double>(loaded_face->key().pixel_size);
}

FontsEngine::~FontsEngine() { clear_loaded_faces(); }

void FontsEngine::init() { render_cache_.init(); }

void FontsEngine::set_render_cache_dir_path(const io::PathT &path, const std::string &revision) {
    render_cache_.set_cache_dir_path(path, revision);
}

void FontsEngine::clear_loaded_faces() {
    for (RequireFace *f : required_faces_) {
        delete f;
    }
    required_faces_.clear();

    for (IFontFace *f : loaded_faces_) {
        delete f;
    }
    loaded_faces_.clear();
}

double FontsEngine::line_spacing(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    return from_f26d6(rf->face->leading() + rf->face->ascent() + rf->face->descent()) * rf->pixel_scale();
}

double FontsEngine::x_height(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    return from_f26d6(rf->face->x_height()) * rf->pixel_scale();
}

double FontsEngine::height(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    return from_f26d6(rf->face->ascent() + rf->face->descent()) * rf->pixel_scale();
}

double FontsEngine::cap_height(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    return from_f26d6(rf->face->cap_height()) * rf->pixel_scale();
}

double FontsEngine::ascent(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    return from_f26d6(rf->face->ascent()) * rf->pixel_scale();
}

double FontsEngine::descent(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    return from_f26d6(rf->face->descent()) * rf->pixel_scale();
}

double FontsEngine::underline_pos(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }
    return from_f26d6(rf->face->underline_pos()) * rf->pixel_scale();
}

double FontsEngine::line_width(const Font &f) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 1.0; }
    return from_f26d6(rf->face->line_width()) * rf->pixel_scale();
}

double FontsEngine::strike_out_pos(const Font &f) const { return ascent(f) / 3.0; }

bool FontsEngine::in_font(const Font &f, char32_t ucs4) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return false; }

    return rf->face->glyph_index(ucs4) != 0;
}

double FontsEngine::horizontal_advance(const Font &f, const char32_t &ch) const {
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    glyph_idx_t glyph_idx = rf->face->glyph_index(ch);
    return from_f26d6(rf->face->glyph_advance(glyph_idx)) * rf->pixel_scale();
}

double FontsEngine::horizontal_advance(const Font &f, const std::u32string &text) const {
    if (text.empty()) {
        return 0.0;
    }

    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return 0.0; }

    double advance = 0.0;

    TextBlock text_block;
    text_block.text = &text[0];
    text_block.lenght = static_cast<int>(text.size());

    std::vector<FontFaceTextBlock> font_face_blocks = split_text_by_font_faces(rf, text_block);
    for (const FontFaceTextBlock &ff_block : font_face_blocks) {
        const IFontFace *font_face = ff_block.face;
        if (!font_face) {
            if (is_zero_advance_text(ff_block.text.text, ff_block.text.lenght)) {
                continue;
            }

            font_face = rf->face;
        }

        const double pixel_scale = rf->pixel_scale_for(font_face);
        std::vector<ShapedGlyph> glyphs = font_face->glyphs(ff_block.text.text, ff_block.text.lenght);
        for (const ShapedGlyph &g : glyphs) {
            advance += from_f26d6(g.x_advance) * pixel_scale;
        }
    }

    return advance;
}

RectF FontsEngine::bounding_rect(const Font &f, const char32_t &ch) const {
    RequireFace *rf = font_face(f, f.type() == Font::Type::MUSIC_SYMBOL);
    IF_ASSERT_FAILED(rf && rf->face) { return RectF(); }

    glyph_idx_t glyph_idx = rf->face->glyph_index(ch);
    return from_fb_box(rf->face->glyph_bbox(glyph_idx), rf->pixel_scale());
}

RectF FontsEngine::bounding_rect(const Font &f, const std::u32string &text) const {
    if (text.empty()) {
        return RectF();
    }

    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return RectF(); }

    FBBox rect; // f26dot6_t units
    rect.set_y(-rf->face->ascent());
    rect.set_height(rf->face->ascent() + rf->face->descent());

    f26dot6_t x_offset = 0;
    f26dot6_t xmax = 0;
    f26dot6_t ymax = 0;
    bool has_glyph = false;

    TextBlock text_block;
    text_block.text = &text[0];
    text_block.lenght = static_cast<int>(text.size());

    std::vector<FontFaceTextBlock> font_face_blocks = split_text_by_font_faces(rf, text_block);
    for (const FontFaceTextBlock &ff_block : font_face_blocks) {
        const IFontFace *font_face = ff_block.face;
        if (!font_face) {
            if (is_zero_advance_text(ff_block.text.text, ff_block.text.lenght)) {
                continue;
            }

            font_face = rf->face;
        }

        std::vector<ShapedGlyph> glyphs = font_face->glyphs(ff_block.text.text, ff_block.text.lenght);

        for (const ShapedGlyph &g : glyphs) {
            FBBox bbox = font_face->glyph_bbox(g.idx);
            f26dot6_t x = x_offset + bbox.x();
            f26dot6_t y = bbox.y();

            bool use_cached_glyph_metrics = fits_qt_glyph_cache_metrics(bbox, font_face->glyph_advance(g.idx));
            f26dot6_t glyph_right = use_cached_glyph_metrics ? ((x + 63) & -64) + bbox.width() : x + bbox.width();
            f26dot6_t glyph_bottom = use_cached_glyph_metrics ? ((y + 63) & -64) + bbox.height() : y + bbox.height();

            if (!has_glyph) {
                rect.set_x(x);
                xmax = glyph_right;
                ymax = glyph_bottom;
                has_glyph = true;
            } else {
                rect.set_x(std::min(rect.x(), x));
                xmax = std::max(xmax, glyph_right);
                ymax = std::max(ymax, glyph_bottom);
            }

            rect.set_y(std::min(rect.y(), y));
            x_offset += g.x_advance;
        }
    }

    if (!has_glyph) {
        rect.set_x(0);
        rect.set_width(0);
        return from_fb_box(rect, rf->pixel_scale());
    }

    rect.set_width(xmax - rect.x());
    rect.set_height(std::max(rect.height(), ymax - rect.y()));

    return from_fb_box(rect, rf->pixel_scale());
}

RectF FontsEngine::tight_bounding_rect(const Font &f, const std::u32string &text) const {
    if (text.empty()) {
        return RectF();
    }

    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return RectF(); }

    FBBox rect; // f26dot6_t units
    f26dot6_t x_offset = 0;
    f26dot6_t xmax = 0;
    f26dot6_t ymax = 0;
    bool has_glyph = false;

    TextBlock text_block;
    text_block.text = &text[0];
    text_block.lenght = static_cast<int>(text.size());

    std::vector<FontFaceTextBlock> font_face_blocks = split_text_by_font_faces(rf, text_block);
    for (const FontFaceTextBlock &ff_block : font_face_blocks) {
        const IFontFace *font_face = ff_block.face;
        if (!font_face) {
            if (is_zero_advance_text(ff_block.text.text, ff_block.text.lenght)) {
                continue;
            }

            font_face = rf->face;
        }

        std::vector<ShapedGlyph> glyphs = font_face->glyphs(ff_block.text.text, ff_block.text.lenght);

        for (const ShapedGlyph &g : glyphs) {
            FBBox bbox = font_face->glyph_bbox(g.idx);
            f26dot6_t x = x_offset + bbox.x();
            f26dot6_t y = bbox.y();

            f26dot6_t glyph_right = ((x + 63) & -64) + bbox.width();
            f26dot6_t glyph_bottom = ((y + 63) & -64) + bbox.height();

            if (!has_glyph) {
                rect.set_x(x);
                rect.set_y(y);
                xmax = glyph_right;
                ymax = glyph_bottom;
                has_glyph = true;
            } else {
                rect.set_x(std::min(rect.x(), x));
                rect.set_y(std::min(rect.y(), y));
                xmax = std::max(xmax, glyph_right);
                ymax = std::max(ymax, glyph_bottom);
            }

            x_offset += g.x_advance;
        }
    }

    if (!has_glyph) {
        return RectF();
    }

    rect.set_width(xmax - rect.x());
    rect.set_height(ymax - rect.y());

    return from_fb_box(rect, rf->pixel_scale());
}

std::vector<GlyphImage> FontsEngine::render(const Font &f, const std::u32string &text) const {
    //! NOTE for rendering, all fonts, including symbols fonts, are processed as text
    RequireFace *rf = font_face(f);
    IF_ASSERT_FAILED(rf && rf->face) { return std::vector<GlyphImage>(); }

    std::vector<GlyphImage> images;

    if (text.empty()) {
        return images;
    }

    return images;
}

void FontsEngine::set_font_face_factory(const FontFaceFactory &f) { font_face_factory_ = f; }

IFontFace *FontsEngine::create_font_face(const FontData &font_data, Font::Type type) const {
    const FontDataKey &data_key = font_data.key;
    if (font_face_factory_) {
        return font_face_factory_(data_key, type);
    }

    IFontFace *origin = nullptr;
    if (is_ftx(font_data.data)) {
        LOGE() << "XT font face backend is disabled: " << data_key.family().id();
        return nullptr;
    } else {
        origin = new FontFaceFT();
    }

    return new FontFaceDU(origin);
}

IFontFace *FontsEngine::font_face_by_actual_data_key(const FontDataKey &actual_data_key, Font::Type type, int input_loaded_pixel_size,
                                                     bool is_symbol_mode) const {
    for (IFontFace *face : loaded_faces_) {
        if (face->key().data_key == actual_data_key && face->key().type == type && face->key().pixel_size == input_loaded_pixel_size &&
            face->is_symbol_mode() == is_symbol_mode) {
            return face;
        }
    }

    FontData font_data = database_.font_data(actual_data_key, type);
    if (!font_data.valid()) {
        LOGE() << "font data is empty: " << actual_data_key.family().id();
        return nullptr;
    }

    FaceKey loaded_key;
    loaded_key.data_key = font_data.key;
    loaded_key.type = type;
    loaded_key.pixel_size = input_loaded_pixel_size;

    IFontFace *face = create_font_face(font_data, type);
    IF_ASSERT_FAILED(face) { return nullptr; }

    if (!face->load(loaded_key, font_data, is_symbol_mode)) {
        LOGE() << "failed load font face: " << font_data.key.family().id();
        delete face;
        return nullptr;
    }

    loaded_faces_.push_back(face);
    return face;
}

IFontFace *FontsEngine::sdf_font_face_for(const IFontFace *layout_face) const {
    if (!layout_face) {
        return nullptr;
    }

    return font_face_by_actual_data_key(layout_face->key().data_key, layout_face->key().type,
                                        static_cast<int>(::loaded_pixel_size_constant), layout_face->is_symbol_mode());
}

FontsEngine::RequireFace *FontsEngine::font_face(const Font &f, bool is_symbol_mode) const {
    if (database_revision_ != database_.revision()) {
        const_cast<FontsEngine *>(this)->clear_loaded_faces();
        database_revision_ = database_.revision();
    }

    //! NOTE This font is required
    FaceKey require_key = face_key_for_metrics_font(f);

    //! NOTE If pixelSize is not set, then specify the default
    //! (this is the default pixelSize in Qt)
    if (!(require_key.pixel_size > 0)) {
        require_key.pixel_size = ::default_pixel_size;
    }

    //! NOTE At the moment, in some cases, the type may not be specified,
    //! so set as Text
    if (require_key.type == Font::Type::UNDEFINED || require_key.type == Font::Type::UNKNOWN) {
        require_key.type = Font::Type::TEXT;
    }

    //! NOTE We are looking for the require font we need among the previously loaded ones
    for (RequireFace *face : required_faces_) {
        if (face->require_key == require_key && face->is_symbol_mode() == is_symbol_mode) {
            return face;
        }
    }

    //! Let's find out which real font will be used
    //! (for example, if there is no required one)
    FontDataKey actual_data_key = database_.actual_font(require_key.data_key, require_key.type);

    const int loaded_pixel_size_value = is_ftx(database_.font_data(actual_data_key, require_key.type).data) ?
                                            static_cast<int>(::loaded_pixel_size_constant) :
                                            require_key.pixel_size;
    IFontFace *face = font_face_by_actual_data_key(actual_data_key, require_key.type, loaded_pixel_size_value, is_symbol_mode);
    if (!face) {
        return nullptr;
    }

    //! If we didn't find it, we create a new require font
    RequireFace *new_font = new RequireFace();
    new_font->require_key = require_key;
    new_font->actual_data_key = actual_data_key;
    new_font->face = face;

    auto subtitution_font_data_keys = database_.substitution_fonts(require_key.data_key);
    for (const FontDataKey &data_key : subtitution_font_data_keys) {
        const FontDataKey actual_subtitution_data_key = database_.actual_font(data_key, require_key.type);
        const int subtitution_loaded_pixel_size = is_ftx(database_.font_data(actual_subtitution_data_key, require_key.type).data) ?
                                                      static_cast<int>(::loaded_pixel_size_constant) :
                                                      require_key.pixel_size;
        IFontFace *subtitution_face =
            font_face_by_actual_data_key(actual_subtitution_data_key, require_key.type, subtitution_loaded_pixel_size, is_symbol_mode);
        if (subtitution_face) {
            new_font->subtitution_faces.push_back(subtitution_face);
        }
    }

    required_faces_.push_back(new_font);

    return new_font;
}

std::vector<FontsEngine::FontFaceTextBlock> FontsEngine::split_text_by_font_faces(const RequireFace *rf, const TextBlock &text) const {
    std::vector<FontFaceTextBlock> text_blocks;

    FontFaceTextBlock txt_block;
    txt_block.text.text = &text.text[0];
    const IFontFace *current = nullptr;
    for (int i = 0; i < text.lenght; ++i) {
        const IFontFace *new_face = nullptr;
        if (rf->face->glyph_index(text.text[i]) != 0) {
            new_face = rf->face;
        } else {
            new_face = find_subtitution_font(text.text[i], rf->subtitution_faces);
        }

        if (txt_block.text.lenght > 0 && new_face != current) {
            txt_block.face = current;
            text_blocks.push_back(txt_block);
            txt_block.text.text = &text.text[i];
            txt_block.text.lenght = 0;
        }

        current = new_face;
        ++txt_block.text.lenght;

        if (i == (text.lenght - 1)) {
            txt_block.face = current;
            text_blocks.push_back(txt_block);
            txt_block.text.text = nullptr;
            txt_block.text.lenght = 0;
        }
    }

    return text_blocks;
}
