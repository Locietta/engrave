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
#include "fontfaceft.h"

#include <cmath>
#include <unordered_map>
#include <utility>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BBOX_H
#include FT_TRUETYPE_TABLES_H
#include <hb-ft.h>

#include "fontfaceutils.h"
#include "global/types/bytearray.h"
#include "log.h"

using namespace muse::draw;

static const hb_tag_t kern_tag = HB_TAG('k', 'e', 'r', 'n'); // kerning operations
static const hb_tag_t liga_tag = HB_TAG('l', 'i', 'g', 'a'); // standard ligature substitution
static const hb_tag_t clig_tag = HB_TAG('c', 'l', 'i', 'g'); // contextual ligature substitution
static const hb_tag_t dlig_tag = HB_TAG('d', 'l', 'i', 'g'); // contextual ligature substitution
static const hb_tag_t hlig_tag = HB_TAG('h', 'l', 'i', 'g'); // contextual ligature substitution

static const std::vector<hb_feature_t> hb_features = {{::kern_tag, 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
                                                      {::liga_tag, 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
                                                      {::clig_tag, 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
                                                      {::dlig_tag, 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END},
                                                      {::hlig_tag, 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END}};

static FT_Library ftlib = nullptr;

static bool init_ft() {
    int error = 0;
    if (!ftlib) {
        error = FT_Init_FreeType(&ftlib);
        if (!ftlib || error) {
            LOGE() << "init freetype library failed";
        }
    }
    return error == 0;
}

#define FLOOR(x) ((x) & -64)
#define CEIL(x) (((x) + 63) & -64)
#define TRUNC(x) ((x) >> 6)
#define ROUND(x) (((x) + 32) & -64)

struct muse::draw::GlyphMetrics {
    FBBox bbox;
    FT_Fixed linear_advance = 0;
};

struct muse::draw::SymbolMetrics {
    glyph_idx_t idx = 0;
    FT_BBox bbox;
    FT_Fixed linear_advance = 0;
};

struct HeightMetrics {
    f26dot6_t ascent = 0;
    f26dot6_t descent = 0;
    f26dot6_t leading = 0;
};

struct muse::draw::FData {
    ByteArray font_data;
    FT_Face face = nullptr;
    hb_font_t *hb_font = nullptr;
    std::unordered_map<glyph_idx_t, GlyphMetrics> glyphs_metrics;
    std::unordered_map<glyph_idx_t, SymbolMetrics> symbol_metrics;
    FT_Size_Metrics metrics;
    HeightMetrics height_metrics;
};

static f26dot6_t table_value_to_f26_dot6(FT_Face face, long value, int pixel_size) {
    if (!face || face->units_per_EM == 0) {
        return 0;
    }

    return static_cast<f26dot6_t>(value * pixel_size * 64.0 / face->units_per_EM);
}

static bool process_hhea_table(FT_Face face, int pixel_size, HeightMetrics &metrics) {
    TT_HoriHeader *hhea = (TT_HoriHeader *) FT_Get_Sfnt_Table(face, ft_sfnt_hhea);
    if (!hhea) {
        return false;
    }

    if (hhea->Ascender == 0 && hhea->Descender == 0) {
        return false;
    }

    metrics.ascent = table_value_to_f26_dot6(face, hhea->Ascender, pixel_size);
    metrics.descent = -table_value_to_f26_dot6(face, hhea->Descender, pixel_size);
    metrics.leading = table_value_to_f26_dot6(face, hhea->Line_Gap, pixel_size);
    return true;
}

static bool process_o_s2_table(FT_Face face, int pixel_size, bool prefer_typo_line_metrics, HeightMetrics &metrics) {
    TT_OS2 *os2 = (TT_OS2 *) FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    if (!os2) {
        return false;
    }

    enum { USE_TYPO_METRICS = 0x80 };

    if (prefer_typo_line_metrics || (os2->fsSelection & USE_TYPO_METRICS)) {
        if (os2->sTypoAscender == 0 && os2->sTypoDescender == 0) {
            return false;
        }

        metrics.ascent = table_value_to_f26_dot6(face, os2->sTypoAscender, pixel_size);
        metrics.descent = -table_value_to_f26_dot6(face, os2->sTypoDescender, pixel_size);
        metrics.leading = table_value_to_f26_dot6(face, os2->sTypoLineGap, pixel_size);
    } else {
        if (os2->usWinAscent == 0 && os2->usWinDescent == 0) {
            return false;
        }

        metrics.ascent = table_value_to_f26_dot6(face, os2->usWinAscent, pixel_size);
        metrics.descent = table_value_to_f26_dot6(face, os2->usWinDescent, pixel_size);
        metrics.leading = 0;
    }

    return true;
}

static HeightMetrics calculate_height_metrics(FData *data, int pixel_size) {
    const FT_Size_Metrics &metrics = data->metrics;
    const double scale = static_cast<double>(pixel_size) / metrics.y_ppem;

    HeightMetrics result;
    result.ascent = static_cast<f26dot6_t>(metrics.ascender * scale);
    result.descent = static_cast<f26dot6_t>(-metrics.descender * scale);
    result.leading = static_cast<f26dot6_t>((metrics.height - metrics.ascender + metrics.descender) * scale);

    process_hhea_table(data->face, pixel_size, result);
    process_o_s2_table(data->face, pixel_size, true, result);
    return result;
}

static void initialize_height_metrics(FData *data, int pixel_size) { data->height_metrics = calculate_height_metrics(data, pixel_size); }

FontFaceFT::FontFaceFT() { data_ = new FData(); }

FontFaceFT::~FontFaceFT() {
    if (data_->hb_font) {
        hb_font_destroy(data_->hb_font);
    }
    FT_Done_Face(data_->face);
    delete data_;
}

bool FontFaceFT::load(const FaceKey &key, FontData font_data, bool is_symbol_mode) {
    if (!init_ft()) {
        return false;
    }

    key_ = key;
    is_symbol_mode_ = is_symbol_mode;

    if (!font_data.valid()) {
        LOGE() << "empty font data: " << key_.data_key.family().id();
        return false;
    }

    data_->font_data = std::move(font_data.data);

    int rval = new_matching_memory_face(ftlib, (FT_Byte *) data_->font_data.const_data(), (FT_Long) data_->font_data.size(), key.data_key,
                                        &data_->face);
    if (rval) {
        LOGE() << "freetype: cannot create face: " << key_.data_key.family().id() << ", rval: " << rval;
        return false;
    }

    if (is_symbol_mode_) {
        FT_Set_Pixel_Sizes(data_->face, 0, int(key_.pixel_size + .5));
    } else {
        FT_Set_Char_Size(data_->face, to_f26d6(key_.pixel_size), to_f26d6(key_.pixel_size), 0, 0);

        FT_Matrix matrix;
        matrix.xx = 0x10000;
        matrix.yy = 0x10000;
        matrix.xy = 0;
        matrix.yx = 0;
        FT_Set_Transform(data_->face, &matrix, nullptr);

        data_->hb_font = hb_ft_font_create(data_->face, nullptr);
    }

    data_->metrics = data_->face->size->metrics;
    initialize_height_metrics(data_, key_.pixel_size);

    return true;
}

const FaceKey &FontFaceFT::key() const { return key_; }

bool FontFaceFT::is_symbol_mode() const { return is_symbol_mode_; }

std::vector<ShapedGlyph> FontFaceFT::glyphs(const char32_t *text, int text_length) const {
    if (text_length < 1) {
        return std::vector<ShapedGlyph>();
    }

    std::vector<ShapedGlyph> result;
    if (is_symbol_mode_) {
        for (int i = 0; i < text_length; ++i) {
            glyph_idx_t idx = glyph_index(text[i]);
            SymbolMetrics *sm = symbol_metrics(idx);
            IF_ASSERT_FAILED(sm) { return std::vector<ShapedGlyph>(); }

            ShapedGlyph p;
            p.idx = sm->idx;
            p.x_advance = sm->linear_advance;
            p.cluster = static_cast<uint32_t>(i);

            result.push_back(std::move(p));
        }
    } else {
        hb_buffer_t *hb_buffer = hb_buffer_create();
        hb_segment_properties_t props = HB_SEGMENT_PROPERTIES_DEFAULT;

        hb_buffer_add_utf32(hb_buffer, (uint32_t *) text, text_length, 0, -1);
        hb_buffer_set_direction(hb_buffer, props.direction);
        hb_buffer_set_script(hb_buffer, props.script);

        hb_buffer_set_segment_properties(hb_buffer, &props);
        hb_buffer_guess_segment_properties(hb_buffer);

        hb_shape(data_->hb_font, hb_buffer, &::hb_features[0], static_cast<unsigned int>(::hb_features.size()));
        unsigned int len = hb_buffer_get_length(hb_buffer);
        result.reserve(len);

        hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buffer, nullptr);
        hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buffer, nullptr);

        for (unsigned int i = 0; i < len; i++) {
            f26dot6_t x_advance_value = static_cast<f26dot6_t>(pos[i].x_advance);
            hb_position_t hb_advance = hb_font_get_glyph_h_advance(data_->hb_font, info[i].codepoint);
            if (GlyphMetrics *metrics = glyph_metrics(info[i].codepoint)) {
                x_advance_value += metrics->linear_advance - hb_advance;
            }

            result.push_back({info[i].codepoint, x_advance_value, info[i].cluster});
        }

        hb_buffer_destroy(hb_buffer);
    }

    return result;
}

glyph_idx_t FontFaceFT::glyph_index(char32_t ucs4) const {
    if (ucs4 == 0) {
        return 0;
    }

    FT_UInt index = FT_Get_Char_Index(data_->face, ucs4);
    return static_cast<glyph_idx_t>(index);
}

glyph_idx_t FontFaceFT::glyph_index(const std::string &glyph_name) const {
    FT_UInt index = FT_Get_Name_Index(data_->face, glyph_name.c_str());
    return static_cast<glyph_idx_t>(index);
}

char32_t FontFaceFT::find_char_code(glyph_idx_t idx) const {
    auto find_c = [this](glyph_idx_t idx) {
        FT_UInt gindex = 0;
        FT_ULong charcode = FT_Get_First_Char(data_->face, &gindex);
        if (gindex == idx) {
            return charcode;
        }

        while (gindex != 0) {
            charcode = FT_Get_Next_Char(data_->face, charcode, &gindex);
            if (gindex == idx) {
                return charcode;
            }
        }

        return FT_ULong(0);
    };

    char32_t c = find_c(idx);

    // check
    assert(glyph_index(c) == idx);

    return c;
}

FBBox FontFaceFT::glyph_bbox(glyph_idx_t idx) const {
    if (is_symbol_mode()) {
        FT_UInt index = static_cast<FT_UInt>(idx);
        if (index == 0) {
            return FBBox();
        }

        if (FT_Load_Glyph(data_->face, index, FT_LOAD_NO_BITMAP) != 0) {
            return FBBox();
        }

        FT_GlyphSlot slot = data_->face->glyph;
        if (slot->format != FT_GLYPH_FORMAT_OUTLINE) {
            return FBBox();
        }

        FT_BBox outline_box;
        FT_Outline_Get_BBox(&slot->outline, &outline_box);

        FBBox bbox;
        bbox.set_coords(outline_box.xMin, -outline_box.yMax, outline_box.xMax, -outline_box.yMin);
        return bbox;
    } else {
        GlyphMetrics *gm = glyph_metrics(idx);
        if (!gm) {
            return FBBox();
        }
        return gm->bbox;
    }
}

f26dot6_t FontFaceFT::glyph_advance(glyph_idx_t idx) const {
    if (is_symbol_mode()) {
        SymbolMetrics *sm = symbol_metrics(idx);
        IF_ASSERT_FAILED(sm) { return 0.0; }
        return sm->linear_advance;
    } else {
        GlyphMetrics *gm = glyph_metrics(idx);
        if (!gm) {
            return 0.0;
        }
        return gm->linear_advance;
    }
}

const GlyphImage &FontFaceFT::glyph_image([[maybe_unused]] glyph_idx_t idx) const {
    static const GlyphImage null;

    return null;
}

f26dot6_t FontFaceFT::leading() const { return data_->height_metrics.leading; }

f26dot6_t FontFaceFT::ascent() const { return data_->height_metrics.ascent; }

f26dot6_t FontFaceFT::descent() const { return data_->height_metrics.descent; }

f26dot6_t FontFaceFT::x_height() const {
    TT_OS2 *os2 = (TT_OS2 *) FT_Get_Sfnt_Table(data_->face, ft_sfnt_os2);
    if (os2 && os2->sxHeight) {
        f26dot6_t result = std::round(os2->sxHeight * data_->face->size->metrics.y_ppem * 64.0 / (double) data_->face->units_per_EM);
        return result;
    }

    const glyph_idx_t glyph = glyph_index('x');
    if (glyph == 0) {
        return 0;
    }

    GlyphMetrics *gm = glyph_metrics(glyph);
    IF_ASSERT_FAILED(gm) { return 0; }
    return gm->bbox.height();
}

f26dot6_t FontFaceFT::cap_height() const {
    TT_OS2 *os2 = (TT_OS2 *) FT_Get_Sfnt_Table(data_->face, ft_sfnt_os2);
    if (os2 && os2->sCapHeight) {
        f26dot6_t result = std::round(os2->sCapHeight * data_->face->size->metrics.y_ppem * 64.0 / (double) data_->face->units_per_EM);
        return result;
    }

    const glyph_idx_t glyph = glyph_index('X');
    if (glyph == 0) {
        return 0;
    }

    GlyphMetrics *gm = glyph_metrics(glyph);
    IF_ASSERT_FAILED(gm) { return 0; }
    return gm->bbox.height();
}

f26dot6_t FontFaceFT::underline_pos() const {
    f26dot6_t thickness = FT_MulFix(data_->face->underline_thickness, data_->face->size->metrics.y_scale);
    f26dot6_t center_pos = -FT_MulFix(data_->face->underline_position, data_->face->size->metrics.y_scale);
    return center_pos - thickness / 2;
}

f26dot6_t FontFaceFT::line_width() const { return FT_MulFix(data_->face->underline_thickness, data_->face->size->metrics.y_scale); }

GlyphMetrics *FontFaceFT::glyph_metrics(glyph_idx_t idx) const {
    if (data_->glyphs_metrics.find(idx) != data_->glyphs_metrics.end()) {
        return &data_->glyphs_metrics.at(idx);
    }

    FT_UInt index = static_cast<FT_UInt>(idx);
    if (index == 0) {
        return nullptr;
    }

    if (FT_Load_Glyph(data_->face, index, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0) {
        return nullptr;
    }

    FT_GlyphSlot slot = data_->face->glyph;

    GlyphMetrics &gm = data_->glyphs_metrics[idx];

    auto floor_f26_dot6 = [](FT_Pos value) { return value & -64; };

    auto ceil_f26_dot6 = [](FT_Pos value) { return (value + 63) & -64; };

    FT_Pos left = floor_f26_dot6(slot->metrics.horiBearingX);
    FT_Pos right = ceil_f26_dot6(slot->metrics.horiBearingX + slot->metrics.width);
    FT_Pos top = ceil_f26_dot6(slot->metrics.horiBearingY);
    FT_Pos bottom = floor_f26_dot6(slot->metrics.horiBearingY - slot->metrics.height);

    gm.bbox.set_left(left);
    gm.bbox.set_top(-top);
    gm.bbox.set_width(right - left);
    gm.bbox.set_height(top - bottom);

    gm.linear_advance = slot->linearHoriAdvance >> 10;

    return &gm;
}

SymbolMetrics *FontFaceFT::symbol_metrics(glyph_idx_t idx) const {
    if (data_->symbol_metrics.find(idx) != data_->symbol_metrics.end()) {
        return &data_->symbol_metrics.at(idx);
    }

    FT_UInt index = static_cast<FT_UInt>(idx);
    if (index == 0) {
        return nullptr;
    }

    if (FT_Load_Glyph(data_->face, index, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0) {
        return nullptr;
    }

    SymbolMetrics &sm = data_->symbol_metrics[idx];

    sm.idx = static_cast<glyph_idx_t>(index);

    FT_GlyphSlot slot = data_->face->glyph;
    sm.bbox.xMin = slot->metrics.horiBearingX;
    sm.bbox.xMax = slot->metrics.horiBearingX + slot->metrics.width;
    sm.bbox.yMin = slot->metrics.horiBearingY - slot->metrics.height;
    sm.bbox.yMax = slot->metrics.horiBearingY;

    //! NOTE Moved form MSS FontEngineFT::advance
    //! double advance = linearHoriAdvance * dpi_f / 655360.0;
    //! -> f26dot6_t advance = linearHoriAdvance * dpi_f * 64.0 / 655360.0;
    //! -> dpi_f = 5.0 constant
    //! -> f26dot6_t advance = linearHoriAdvance * 320.0 / 655360.0;
    //! -> f26dot6_t advance = linearHoriAdvance / 2048;
    sm.linear_advance = data_->face->glyph->linearHoriAdvance / 2048;

    return &sm;
}
