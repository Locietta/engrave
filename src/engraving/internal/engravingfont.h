/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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
#pragma once

#include <memory>
#include <unordered_map>

#include "draw/ifontprovider.h"
#include "draw/types/geometry.h"
#include "iengravingfont.h"
#include "iengravingfontsprovider.h"
#include "infrastructure/shape.h"
#include "infrastructure/smufl.h"
#include "io/iodevice.h"
#include "io/path.h"
#include "style/styledef.h"
#include "types/symid.h"

namespace muse {
struct JsonObject;
} // namespace muse

namespace muse::draw {
struct Painter;
} // namespace muse::draw

namespace mu::engraving {
struct Shape;

struct EngravingFont : IEngravingFont {
    EngravingFont(muse::draw::IFontProvider &font_provider, const IEngravingFontsProvider &engraving_fonts, const std::string &name,
                  const std::string &family, const muse::io::PathT &file_path, const muse::io::PathT &metadata_path);
    EngravingFont(muse::draw::IFontProvider &font_provider, const IEngravingFontsProvider &engraving_fonts, const std::string &name,
                  const std::string &family, std::shared_ptr<muse::io::IODevice> font, std::shared_ptr<muse::io::IODevice> metadata);
    EngravingFont(const EngravingFont &other);

    const std::string &name() const override;
    const std::string &family() const override;

    std::unordered_map<Sid, PropertyValue> engraving_defaults() const override;
    double text_enclosure_thickness();

    char32_t sym_code(SymId id) const override;
    SymId from_code(char32_t code) const override;
    String to_string(SymId id) const override;

    bool is_valid(SymId id) const override;

    RectF bbox(SymId id, double mag) const override;
    RectF bbox(SymId id, const SizeF &) const override;
    RectF bbox(const SymIdList &s, double mag) const override;
    RectF bbox(const SymIdList &s, const SizeF &mag) const override;
    Shape shape(const SymIdList &s, double mag) const override;
    Shape shape(const SymIdList &s, const SizeF &mag) const override;
    Shape shape_with_cutouts(SymId id, double mag) override;
    Shape shape_with_cutouts(SymId id, const SizeF &mag) override;

    double width(SymId id, double mag) const override;
    double width(const SymIdList &, double mag) const override;
    double height(SymId id, double mag) const override;
    double advance(SymId id, double mag) const override;

    PointF smufl_anchor(SymId sym_id, SmuflAnchorId anchor_id, double mag) const override;

    // Draw
    void draw(SymId id, muse::draw::Painter *p, double mag, const PointF &pos, const double angle = 0) const override;
    void draw(SymId id, muse::draw::Painter *p, const SizeF &mag, const PointF &pos, const double angle = 0) const override;

    void draw(const SymIdList &ids, muse::draw::Painter *p, double mag, const PointF &pos, const double angle = 0) const override;
    void draw(const SymIdList &ids, muse::draw::Painter *p, const SizeF &mag, const PointF &pos, const double angle = 0) const override;

    void ensure_load();

private:

    friend struct SymbolFonts;

    struct Sym {
        char32_t code;
        RectF bbox;
        Shape shape_with_cutouts;
        double advance = 0.0;

        std::map<SmuflAnchorId, PointF> smufl_anchors;
        SymIdList sub_symbol_ids;

        bool is_valid() const { return code != 0 && bbox.is_valid(); }

        bool is_compound() const { return !sub_symbol_ids.empty(); }
    };

    void load_glyphs_with_anchors(const muse::JsonObject &glyphs_with_anchors);
    void load_composed_glyphs();
    void load_stylistic_alternates(const muse::JsonObject &glyphs_with_alternates_object);
    void load_engraving_defaults(const muse::JsonObject &engraving_defaults_object);
    void compute_metrics(Sym &sym, const Smufl::Code &code);

    void construct_shape_with_cutouts(Shape &shape, SymId id);

    Sym &sym(SymId id);
    const Sym &sym(SymId id) const;

    bool use_fallback_font(SymId id) const;

    muse::draw::IFontProvider *font_provider() const { return font_provider_; }

    const IEngravingFontsProvider *engraving_fonts() const { return engraving_fonts_; }

    muse::draw::IFontProvider *font_provider_ = nullptr;
    const IEngravingFontsProvider *engraving_fonts_ = nullptr;
    bool loaded_ = false;
    std::vector<Sym> symbols_;
    mutable muse::draw::Font font_;

    std::string name_;
    std::string family_;
    std::shared_ptr<muse::io::IODevice> font_device_;
    std::shared_ptr<muse::io::IODevice> metadata_device_;

    std::unordered_map<Sid, PropertyValue> engraving_defaults_;
    double text_enclosure_thickness_ = 0;
};
} // namespace mu::engraving
