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
#include "engravingfont.h"

#include "../style/styledef.h"
#include "dom/mscore.h"
#include "draw/internal/fontprovider.h"
#include "draw/painter.h"
#include "engraving/dom/mscore.h"
#include "engraving/style/styletypes.h"
#include "engravingfontsprovider.h"
#include "global/flatmap.h"
#include "global/hashmap.h"
#include "io/file.h"
#include "log.h"
#include "serialization/json_fields.h"
#include "smufl.h"
#include "types/symnames.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace muse::draw;
using namespace mu::engraving;

namespace json = muse::json;

struct EngravingFont::Metadata {
    json::SortedJson anchors;
    json::SortedJson alternates;
    json::SortedJson defaults;

    struct glaze {
        using T = Metadata;
        static constexpr auto value =
            glz::object("glyphsWithAnchors", &T::anchors, "glyphsWithAlternates", &T::alternates, "engravingDefaults", &T::defaults);
    };
};

// =============================================
// ScoreFont
// =============================================

EngravingFont::EngravingFont(FontProvider &font_provider, const EngravingFontsProvider &engraving_fonts, const std::string &name,
                             const std::string &family, const PathT &file_path, const PathT &metadata_path)
    : EngravingFont(font_provider, engraving_fonts, name, family, std::make_shared<File>(file_path),
                    std::make_shared<File>(metadata_path)) {}

EngravingFont::EngravingFont(FontProvider &font_provider, const EngravingFontsProvider &engraving_fonts, const std::string &name,
                             const std::string &family, std::shared_ptr<IODevice> font, std::shared_ptr<IODevice> metadata)
    : font_provider_(&font_provider), engraving_fonts_(&engraving_fonts), symbols_(static_cast<size_t>(SymId::LAST_SYM) + 1), name_(name),
      family_(family), font_device_(std::move(font)), metadata_device_(std::move(metadata)) {}

EngravingFont::EngravingFont(const EngravingFont &other) : font_provider_(other.font_provider_), engraving_fonts_(other.engraving_fonts_) {
    loaded_ = false;
    symbols_ = other.symbols_;
    name_ = other.name_;
    family_ = other.family_;
    font_device_ = other.font_device_;
    metadata_device_ = other.metadata_device_;
}

// =============================================
// Properties
// =============================================

const std::string &EngravingFont::name() const { return name_; }

const std::string &EngravingFont::family() const { return family_; }

muse::HashMap<Sid, PropertyValue> EngravingFont::engraving_defaults() const { return engraving_defaults_; }

double EngravingFont::text_enclosure_thickness() { return text_enclosure_thickness_; }

// =============================================
// Load
// =============================================

void EngravingFont::ensure_load() {
    if (loaded_) {
        return;
    }

    const String family = String::from_std_string(family_);
    if (!font_device_ || -1 == font_provider()->add_symbol_font(family, font_device_)) {
        LOGE() << "fatal error: cannot load font: " << name_;
        return;
    }

    font_.set_weight(Font::NORMAL);
    font_.set_italic(false);
    font_.set_family(String::from_std_string(family_), Font::Type::MUSIC_SYMBOL);
    font_.set_no_font_merging(true);
    font_.set_hinting(Font::Hinting::PREFER_VERTICAL_HINTING);

    font_.set_point_size_f(StyleDef::default_smufl_point_size());

    for (size_t id = 0; id < symbols_.size(); ++id) {
        Smufl::Code code = Smufl::code(static_cast<SymId>(id));
        if (!code.is_valid()) {
            continue;
        }
        Sym &sym = symbols_[id];
        compute_metrics(sym, code);
    }

    if (!metadata_device_ || !metadata_device_->open(IODevice::READ_ONLY)) {
        LOGE() << "Failed to open glyph metadata of " << name_;
        return;
    }

    metadata_device_->seek(0);
    const ByteArray metadata = metadata_device_->read_all();
    metadata_device_->close();

    const auto source = json::text(metadata);
    Metadata parsed;
    if (const auto error = glz::read<json::IgnoreUnknown{}>(parsed, source)) {
        LOGE() << "Invalid glyph metadata for " << name_ << ": " << glz::format_error(error, source);
        return;
    }

    load_glyphs_with_anchors(parsed);
    load_composed_glyphs();
    load_stylistic_alternates(parsed);
    load_engraving_defaults(parsed);

    loaded_ = true;
}

void EngravingFont::load_composed_glyphs() {
    static const struct ComposedGlyph {
        const SymId id;
        const SymIdList sub_symbol_ids;
    } composed_glyphs[] = {
        {SymId::ORNAMENT_PRALL_MORDENT,
         {SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_MIDDLE_VERTICAL_STROKE,
          SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END}},
        {SymId::ORNAMENT_UP_PRALL,
         {SymId::ORNAMENT_BOTTOM_LEFT_CONCAVE_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
          SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END}},
        {SymId::ORNAMENT_UP_MORDENT,
         {SymId::ORNAMENT_BOTTOM_LEFT_CONCAVE_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
          SymId::ORNAMENT_MIDDLE_VERTICAL_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END}},
        {SymId::ORNAMENT_PRALL_DOWN,
         {
             SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
             SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
             SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
             SymId::ORNAMENT_BOTTOM_RIGHT_CONCAVE_STROKE,
         }},
        {SymId::ORNAMENT_DOWN_MORDENT,
         {SymId::ORNAMENT_LEFT_VERTICAL_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
          SymId::ORNAMENT_MIDDLE_VERTICAL_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END}},
        {SymId::ORNAMENT_PRALL_UP,
         {
             SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
             SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
             SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
             SymId::ORNAMENT_TOP_RIGHT_CONVEX_STROKE,
         }},
        {SymId::ORNAMENT_LINE_PRALL,
         {SymId::ORNAMENT_LEFT_VERTICAL_STROKE, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END, SymId::ORNAMENT_ZIG_ZAG_LINE_NO_RIGHT_END,
          SymId::ORNAMENT_ZIG_ZAG_LINE_WITH_RIGHT_END}}};

    for (const ComposedGlyph &c : composed_glyphs) {
        Sym &sym = this->sym(c.id);
        if (!sym.is_valid()) {
            sym.sub_symbol_ids = c.sub_symbol_ids;
            sym.bbox = bbox(c.sub_symbol_ids, 1.0);
        }
    }
}

static const struct GlyphWithAlternates {
    const std::string key;
    const std::string alternate_key;
    const SymId alternate_sym_id;
} glyphs_with_alternates[] = {
    {std::string("4stringTabClef"), std::string("4stringTabClefSerif"), SymId::FOUR_STRING_TAB_CLEF_SERIF},
    {std::string("6stringTabClef"), std::string("6stringTabClefSerif"), SymId::SIX_STRING_TAB_CLEF_SERIF},
    {std::string("cClef"), std::string("cClefFrench"), SymId::C_CLEF_FRENCH},
    {std::string("cClef"), std::string("cClefFrench20C"), SymId::C_CLEF_FRENCH20_C},
    {std::string("fClef"), std::string("fClefFrench"), SymId::F_CLEF_FRENCH},
    {std::string("fClef"), std::string("fClef19thCentury"), SymId::F_CLEF19TH_CENTURY},
    {std::string("noteheadBlack"), std::string("noteheadBlackOversized"), SymId::NOTEHEAD_BLACK},
    {std::string("noteheadHalf"), std::string("noteheadHalfOversized"), SymId::NOTEHEAD_HALF},
    {std::string("noteheadWhole"), std::string("noteheadWholeOversized"), SymId::NOTEHEAD_WHOLE},
    {std::string("noteheadDoubleWhole"), std::string("noteheadDoubleWholeOversized"), SymId::NOTEHEAD_DOUBLE_WHOLE},
    {std::string("noteheadDoubleWholeSquare"), std::string("noteheadDoubleWholeSquareOversized"), SymId::NOTEHEAD_DOUBLE_WHOLE_SQUARE},
    {std::string("noteheadDoubleWhole"), std::string("noteheadDoubleWholeAlt"), SymId::NOTEHEAD_DOUBLE_WHOLE_ALT},
    {std::string("brace"), std::string("braceSmall"), SymId::BRACE_SMALL},
    {std::string("brace"), std::string("braceLarge"), SymId::BRACE_LARGE},
    {std::string("brace"), std::string("braceLarger"), SymId::BRACE_LARGER},
    {std::string("flag1024thDown"), std::string("flag1024thDownStraight"), SymId::FLAG1024TH_DOWN_STRAIGHT},
    {std::string("flag1024thUp"), std::string("flag1024thUpStraight"), SymId::FLAG1024TH_UP_STRAIGHT},
    {std::string("flag128thDown"), std::string("flag128thDownStraight"), SymId::FLAG128TH_DOWN_STRAIGHT},
    {std::string("flag128thUp"), std::string("flag128thUpStraight"), SymId::FLAG128TH_UP_STRAIGHT},
    {std::string("flag16thDown"), std::string("flag16thDownStraight"), SymId::FLAG16TH_DOWN_STRAIGHT},
    {std::string("flag16thUp"), std::string("flag16thUpStraight"), SymId::FLAG16TH_UP_STRAIGHT},
    {std::string("flag256thDown"), std::string("flag256thDownStraight"), SymId::FLAG256TH_DOWN_STRAIGHT},
    {std::string("flag256thUp"), std::string("flag256thUpStraight"), SymId::FLAG256TH_UP_STRAIGHT},
    {std::string("flag32ndDown"), std::string("flag32ndDownStraight"), SymId::FLAG32ND_DOWN_STRAIGHT},
    {std::string("flag32ndUp"), std::string("flag32ndUpStraight"), SymId::FLAG32ND_UP_STRAIGHT},
    {std::string("flag512thDown"), std::string("flag512thDownStraight"), SymId::FLAG512TH_DOWN_STRAIGHT},
    {std::string("flag512thUp"), std::string("flag512thUpStraight"), SymId::FLAG512TH_UP_STRAIGHT},
    {std::string("flag64thDown"), std::string("flag64thDownStraight"), SymId::FLAG64TH_DOWN_STRAIGHT},
    {std::string("flag64thUp"), std::string("flag64thUpStraight"), SymId::FLAG64TH_UP_STRAIGHT},
    {std::string("flag8thDown"), std::string("flag8thDownStraight"), SymId::FLAG8TH_DOWN_STRAIGHT},
    {std::string("flag8thUp"), std::string("flag8thUpStraight"), SymId::FLAG8TH_UP_STRAIGHT},
    // Time sig variants
    {std::string("timeSig0"), std::string("timeSig0Large"), SymId::TIME_SIG0_LARGE},
    {std::string("timeSig0"), std::string("timeSig0Small"), SymId::TIME_SIG0_SMALL},
    {std::string("timeSig0"), std::string("timeSig0Narrow"), SymId::TIME_SIG0_NARROW},
    {std::string("timeSig1"), std::string("timeSig1Large"), SymId::TIME_SIG1_LARGE},
    {std::string("timeSig1"), std::string("timeSig1Small"), SymId::TIME_SIG1_SMALL},
    {std::string("timeSig1"), std::string("timeSig1Narrow"), SymId::TIME_SIG1_NARROW},
    {std::string("timeSig2"), std::string("timeSig2Large"), SymId::TIME_SIG2_LARGE},
    {std::string("timeSig2"), std::string("timeSig2Small"), SymId::TIME_SIG2_SMALL},
    {std::string("timeSig2"), std::string("timeSig2Narrow"), SymId::TIME_SIG2_NARROW},
    {std::string("timeSig3"), std::string("timeSig3Large"), SymId::TIME_SIG3_LARGE},
    {std::string("timeSig3"), std::string("timeSig3Small"), SymId::TIME_SIG3_SMALL},
    {std::string("timeSig3"), std::string("timeSig3Narrow"), SymId::TIME_SIG3_NARROW},
    {std::string("timeSig4"), std::string("timeSig4Large"), SymId::TIME_SIG4_LARGE},
    {std::string("timeSig4"), std::string("timeSig4Small"), SymId::TIME_SIG4_SMALL},
    {std::string("timeSig4"), std::string("timeSig4Narrow"), SymId::TIME_SIG4_NARROW},
    {std::string("timeSig5"), std::string("timeSig5Large"), SymId::TIME_SIG5_LARGE},
    {std::string("timeSig5"), std::string("timeSig5Small"), SymId::TIME_SIG5_SMALL},
    {std::string("timeSig5"), std::string("timeSig5Narrow"), SymId::TIME_SIG5_NARROW},
    {std::string("timeSig6"), std::string("timeSig6Large"), SymId::TIME_SIG6_LARGE},
    {std::string("timeSig6"), std::string("timeSig6Small"), SymId::TIME_SIG6_SMALL},
    {std::string("timeSig6"), std::string("timeSig6Narrow"), SymId::TIME_SIG6_NARROW},
    {std::string("timeSig7"), std::string("timeSig7Large"), SymId::TIME_SIG7_LARGE},
    {std::string("timeSig7"), std::string("timeSig7Small"), SymId::TIME_SIG7_SMALL},
    {std::string("timeSig7"), std::string("timeSig7Narrow"), SymId::TIME_SIG7_NARROW},
    {std::string("timeSig8"), std::string("timeSig8Large"), SymId::TIME_SIG8_LARGE},
    {std::string("timeSig8"), std::string("timeSig8Small"), SymId::TIME_SIG8_SMALL},
    {std::string("timeSig8"), std::string("timeSig8Narrow"), SymId::TIME_SIG8_NARROW},
    {std::string("timeSig9"), std::string("timeSig9Large"), SymId::TIME_SIG9_LARGE},
    {std::string("timeSig9"), std::string("timeSig9Small"), SymId::TIME_SIG9_SMALL},
    {std::string("timeSig9"), std::string("timeSig9Narrow"), SymId::TIME_SIG9_NARROW},
    {std::string("timeSigBracketLeftSmall"), std::string("timeSigBracketLeftSmallLarge"), SymId::TIME_SIG_BRACKET_LEFT_SMALL_LARGE},
    {std::string("timeSigBracketLeftSmall"), std::string("timeSigBracketLeftSmallNarrow"), SymId::TIME_SIG_BRACKET_LEFT_SMALL_NARROW},
    {std::string("timeSigBracketRightSmall"), std::string("timeSigBracketRightSmallLarge"), SymId::TIME_SIG_BRACKET_RIGHT_SMALL_LARGE},
    {std::string("timeSigBracketRightSmall"), std::string("timeSigBracketRightSmallNarrow"), SymId::TIME_SIG_BRACKET_RIGHT_SMALL_NARROW},
    {std::string("timeSigCommon"), std::string("timeSigCommonLarge"), SymId::TIME_SIG_COMMON_LARGE},
    {std::string("timeSigCommon"), std::string("timeSigCommonNarrow"), SymId::TIME_SIG_COMMON_NARROW},
    {std::string("timeSigCutCommon"), std::string("timeSigCutCommonLarge"), SymId::TIME_SIG_CUT_COMMON_LARGE},
    {std::string("timeSigCutCommon"), std::string("timeSigCutCommonNarrow"), SymId::TIME_SIG_CUT_COMMON_NARROW},
    {std::string("timeSigPlus"), std::string("timeSigPlusLarge"), SymId::TIME_SIG_PLUS_LARGE},
    {std::string("timeSigPlus"), std::string("timeSigPlusNarrow"), SymId::TIME_SIG_PLUS_NARROW},
    {std::string("timeSigPlusSmall"), std::string("timeSigPlusSmallLarge"), SymId::TIME_SIG_PLUS_SMALL_LARGE},
    {std::string("timeSigPlusSmall"), std::string("timeSigPlusSmallNarrow"), SymId::TIME_SIG_PLUS_SMALL_NARROW},
    {std::string("timeSigFractionalSlash"), std::string("timeSigFractionalSlashLarge"), SymId::TIME_SIG_FRACTIONAL_SLASH_LARGE},
    {std::string("timeSigFractionalSlash"), std::string("timeSigFractionalSlashNarrow"), SymId::TIME_SIG_FRACTIONAL_SLASH_NARROW},
    {std::string("timeSigEquals"), std::string("timeSigEqualsLarge"), SymId::TIME_SIG_EQUALS_LARGE},
    {std::string("timeSigEquals"), std::string("timeSigEqualsNarrow"), SymId::TIME_SIG_EQUALS_NARROW},
    {std::string("timeSigMinus"), std::string("timeSigMinusLarge"), SymId::TIME_SIG_MINUS_LARGE},
    {std::string("timeSigMinus"), std::string("timeSigMinusNarrow"), SymId::TIME_SIG_MINUS_NARROW},
    {std::string("timeSigMultiply"), std::string("timeSigMultiplyLarge"), SymId::TIME_SIG_MULTIPLY_LARGE},
    {std::string("timeSigMultiply"), std::string("timeSigMultiplyNarrow"), SymId::TIME_SIG_MULTIPLY_NARROW},
    {std::string("timeSigParensLeftSmall"), std::string("timeSigParensLeftSmallLarge"), SymId::TIME_SIG_PARENS_LEFT_SMALL_LARGE},
    {std::string("timeSigParensLeftSmall"), std::string("timeSigParensLeftSmallNarrow"), SymId::TIME_SIG_PARENS_LEFT_SMALL_NARROW},
    {std::string("timeSigParensRightSmall"), std::string("timeSigParensRightSmallLarge"), SymId::TIME_SIG_PARENS_RIGHT_SMALL_LARGE},
    {std::string("timeSigParensRightSmall"), std::string("timeSigParensRightSmallNarrow"), SymId::TIME_SIG_PARENS_RIGHT_SMALL_NARROW},
    {std::string("timeSigParensLeft"), std::string("timeSigParensLeftLarge"), SymId::TIME_SIG_PARENS_LEFT_LARGE},
    {std::string("timeSigParensLeft"), std::string("timeSigParensLeftNarrow"), SymId::TIME_SIG_PARENS_LEFT_NARROW},
    {std::string("timeSigParensRight"), std::string("timeSigParensRightLarge"), SymId::TIME_SIG_PARENS_RIGHT_LARGE},
    {std::string("timeSigParensRight"), std::string("timeSigParensRightNarrow"), SymId::TIME_SIG_PARENS_RIGHT_NARROW},
    {std::string("timeSigCommaEquals"), std::string("timeSigCommaEqualsLarge"), SymId::TIME_SIG_COMMA_EQUALS_LARGE},
    {std::string("timeSigCommaEquals"), std::string("timeSigCommaEqualsNarrow"), SymId::TIME_SIG_COMMA_EQUALS_NARROW},
    {std::string("timeSigFractionQuarter"), std::string("timeSigFractionQuarterLarge"), SymId::TIME_SIG_FRACTION_QUARTER_LARGE},
    {std::string("timeSigFractionQuarter"), std::string("timeSigFractionQuarterNarrow"), SymId::TIME_SIG_FRACTION_QUARTER_NARROW},
    {std::string("timeSigFractionHalf"), std::string("timeSigFractionHalfLarge"), SymId::TIME_SIG_FRACTION_HALF_LARGE},
    {std::string("timeSigFractionHalf"), std::string("timeSigFractionHalfNarrow"), SymId::TIME_SIG_FRACTION_HALF_NARROW},
    {std::string("timeSigFractionThreeQuarters"), std::string("timeSigFractionThreeQuartersLarge"),
     SymId::TIME_SIG_FRACTION_THREE_QUARTERS_LARGE},
    {std::string("timeSigFractionThreeQuarters"), std::string("timeSigFractionThreeQuartersNarrow"),
     SymId::TIME_SIG_FRACTION_THREE_QUARTERS_NARROW},
    {std::string("timeSigFractionOneThird"), std::string("timeSigFractionOneThirdLarge"), SymId::TIME_SIG_FRACTION_ONE_THIRD_LARGE},
    {std::string("timeSigFractionOneThird"), std::string("timeSigFractionOneThirdNarrow"), SymId::TIME_SIG_FRACTION_ONE_THIRD_NARROW},
    {std::string("timeSigFractionTwoThirds"), std::string("timeSigFractionTwoThirdsLarge"), SymId::TIME_SIG_FRACTION_TWO_THIRDS_LARGE},
    {std::string("timeSigFractionTwoThirds"), std::string("timeSigFractionTwoThirdsNarrow"), SymId::TIME_SIG_FRACTION_TWO_THIRDS_NARROW},
    {std::string("timeSigX"), std::string("timeSigXLarge"), SymId::TIME_SIG_X_LARGE},
    {std::string("timeSigX"), std::string("timeSigXNarrow"), SymId::TIME_SIG_X_NARROW},
    {std::string("timeSigOpenPenderecki"), std::string("timeSigOpenPendereckiLarge"), SymId::TIME_SIG_OPEN_PENDERECKI_LARGE},
    {std::string("timeSigOpenPenderecki"), std::string("timeSigOpenPendereckiNarrow"), SymId::TIME_SIG_OPEN_PENDERECKI_NARROW},
    {std::string("timeSigCut2"), std::string("timeSigCut2Large"), SymId::TIME_SIG_CUT2_LARGE},
    {std::string("timeSigCut2"), std::string("timeSigCut2Narrow"), SymId::TIME_SIG_CUT2_NARROW},
    {std::string("timeSigCut3"), std::string("timeSigCut3Large"), SymId::TIME_SIG_CUT3_LARGE},
    {std::string("timeSigCut3"), std::string("timeSigCut3Narrow"), SymId::TIME_SIG_CUT3_NARROW},
};

void EngravingFont::load_stylistic_alternates(const Metadata &metadata) {
    for (const GlyphWithAlternates &glyph : ::glyphs_with_alternates) {
        const auto *glyph_entry = json::find(metadata.alternates, glyph.key);
        if (!glyph_entry) continue;

        const auto &alternates = json::array(json::member(*glyph_entry, "alternates"));
        for (const auto &alternate : alternates) {
            if (json::get_or_empty<std::string>(json::member(alternate, "name")) != glyph.alternate_key) continue;

            bool ok;
            Smufl::Code code;
            const auto &main = json::get_or_empty<std::string>(json::member(alternate, "codepoint"));
            const auto smufl_code = String::from_std_string(main).mid(2).to_u_int(&ok, 16);
            if (ok) code.smufl_code = smufl_code;

            const auto &other = json::get_or_empty<std::string>(json::member(alternate, "alternateCodepoint"));
            const auto music_sym_block_code = String::from_std_string(other).mid(2).to_u_int(&ok, 16);
            if (ok) code.music_sym_block_code = music_sym_block_code;

            if (code.smufl_code || code.music_sym_block_code) compute_metrics(sym(glyph.alternate_sym_id), code);
            break;
        }
    }
}

void EngravingFont::load_glyphs_with_anchors(const Metadata &metadata) {
    for (const auto &[sym_name, anchors] : json::object(metadata.anchors)) {
        SymId sym_id = SymNames::sym_id_by_name(sym_name);

        if (sym_id == SymId::NO_SYM) {
            for (auto &alternate : ::glyphs_with_alternates) {
                if (alternate.alternate_key == sym_name) {
                    sym_id = alternate.alternate_sym_id;
                    break;
                }
            }
        }

        if (sym_id == SymId::NO_SYM) {
            //! NOTE currently, Bravura contains a bunch of entries in glyphsWithAnchors
            //! for glyph names that will not be found - flag32ndUpStraight, etc.
            continue;
        }

        Sym &sym = this->sym(sym_id);
        static const muse::HashMap<std::string, SmuflAnchorId> smufl_anchor_id_names{
            {"stemDownNW", SmuflAnchorId::STEM_DOWN_NW},      {"stemUpSE", SmuflAnchorId::STEM_UP_SE},
            {"stemDownSW", SmuflAnchorId::STEM_DOWN_SW},      {"stemUpNW", SmuflAnchorId::STEM_UP_NW},
            {"cutOutNE", SmuflAnchorId::CUT_OUT_NE},          {"cutOutNW", SmuflAnchorId::CUT_OUT_NW},
            {"cutOutSE", SmuflAnchorId::CUT_OUT_SE},          {"cutOutSW", SmuflAnchorId::CUT_OUT_SW},
            {"opticalCenter", SmuflAnchorId::OPTICAL_CENTER},
        };

        for (const auto &[anchor_id, coords] : json::object(anchors)) {
            const auto search = smufl_anchor_id_names.find(anchor_id);
            if (search == smufl_anchor_id_names.cend()) {
                continue;
            }

            const auto &arr = json::array(coords);
            if (arr.size() < 2) continue;
            const double x = json::get_or_empty<double>(arr[0]);
            const double y = json::get_or_empty<double>(arr[1]);
            const double default_spatium = StyleDef::style_values[static_cast<size_t>(Sid::SPATIUM)].default_value.to_double();
            sym.smufl_anchors[search->second] = PointF(x, -y) * default_spatium;
        }
    }
}

void EngravingFont::load_engraving_defaults(const Metadata &metadata) {
    if (!metadata.defaults.is_object()) return;
    const auto &defaults = json::object(metadata.defaults);

    struct EngravingDefault {
        std::vector<Sid> sids;

        // If a childKey is not specified in `the font metadata`,
        // it will receive the value for the key of `this` EngravingDefault.
        // This is done for compatibility with fonts made for older SMuFL versions:
        // in newer versions, some settings have been split into two.
        std::vector<std::string> child_keys = {};

        EngravingDefault(const std::vector<Sid> &sids) : sids(sids), child_keys() {}

        EngravingDefault(const std::vector<Sid> &sids, const std::vector<std::string> &child_keys) : sids(sids), child_keys(child_keys) {}
    };

    // https://w3c.github.io/smufl/latest/specification/engravingdefaults.html
    static const muse::HashMap<std::string, EngravingDefault> engraving_defaults_mapping = {
        // "textFontFamily" not supported
        {"staffLineThickness", {{Sid::STAFF_LINE_WIDTH}}},
        {"stemThickness", {{Sid::STEM_WIDTH}}},
        {"beamThickness", {{Sid::BEAM_WIDTH}}},
        // "beamSpacing" handled separately
        {"legerLineThickness", {{Sid::LEDGER_LINE_WIDTH}}},
        {"legerLineExtension", {{Sid::LEDGER_LINE_LENGTH}}},
        {"slurEndpointThickness", {{Sid::SLUR_END_WIDTH}}},
        {"slurMidpointThickness", {{Sid::SLUR_MID_WIDTH}}},
        {"tieEndpointThickness", {{Sid::TIE_END_WIDTH}}},
        {"tieMidpointThickness", {{Sid::TIE_MID_WIDTH}}},
        {"thinBarlineThickness", {{Sid::BAR_WIDTH, Sid::DOUBLE_BAR_WIDTH}}},
        {"thickBarlineThickness", {{Sid::END_BAR_WIDTH}}},
        {"dashedBarlineThickness", {{Sid::DASH_BAR_WIDTH}}},
        {"dashedBarlineDashLength", {{Sid::DASH_BAR_DASH}}},
        {"dashedBarlineGapLength", {{Sid::DASH_BAR_GAP}}},
        {"barlineSeparation", {{Sid::DOUBLE_BAR_DISTANCE}, {"thinThickBarlineSeparation"}}},
        {"thinThickBarlineSeparation", {{Sid::END_BAR_DISTANCE}}},
        {"repeatBarlineDotSeparation", {{Sid::REPEAT_BARLINE_DOT_SEPARATION}}},
        {"bracketThickness", {{Sid::BRACKET_WIDTH}}},
        // "subBracketThickness" not supported
        {"hairpinThickness", {{Sid::HAIRPIN_LINE_WIDTH}}},
        {"octaveLineThickness", {{Sid::OTTAVA_LINE_WIDTH}}},
        {"pedalLineThickness", {{Sid::PEDAL_LINE_WIDTH}}},
        {"repeatEndingLineThickness", {{Sid::VOLTA_LINE_WIDTH}}},
        // "arrowShaftThickness" not supported
        {"lyricLineThickness", {{Sid::LYRICS_LINE_THICKNESS}}},
        // "textEnclosureThickness" handled separately
        {"tupletBracketThickness", {{Sid::TUPLET_BRACKET_WIDTH}}},
        {"hBarThickness", {{Sid::MM_REST_H_BAR_THICKNESS}}}};

    const auto apply_engraving_default = [&](this const auto &self, const std::string &key, const PropertyValue &value) -> void {
        auto search = engraving_defaults_mapping.find(key);
        if (search != engraving_defaults_mapping.cend()) {
            for (Sid sid : search->second.sids) {
                engraving_defaults_.insert({sid, value});
            }

            for (const std::string &child_key : search->second.child_keys) {
                if (!defaults.contains(child_key)) {
                    self(child_key, value);
                }
            }
        }
    };

    for (const auto &[key, field] : defaults) {
        const double number = json::get_or_empty<double>(field);
        if (key == "textEnclosureThickness") {
            text_enclosure_thickness_ = number;
            continue;
        }

        if (key == "beamSpacing") {
            bool value = number > 0.75;
            engraving_defaults_.insert({Sid::USE_WIDE_BEAMS, value});
            continue;
        }

        apply_engraving_default(key, number);
    }

    engraving_defaults_.insert({Sid::MUSICAL_TEXT_FONT, String(u"%1 Text").arg(String::from_std_string(family_))});
}

void EngravingFont::compute_metrics(EngravingFont::Sym &sym, const Smufl::Code &code) {
    if (font_provider()->in_font(font_, code.smufl_code)) {
        sym.code = code.smufl_code;
    } else if (font_provider()->in_font(font_, code.music_sym_block_code)) {
        sym.code = code.music_sym_block_code;
    }

    if (sym.code > 0) {
        sym.bbox = font_provider()->bounding_rect(font_, sym.code);
        sym.advance = font_provider()->horizontal_advance(font_, sym.code);
    }
}

// =============================================
// Symbol properties
// =============================================

EngravingFont::Sym &EngravingFont::sym(SymId id) { return symbols_[static_cast<size_t>(id)]; }

const EngravingFont::Sym &EngravingFont::sym(SymId id) const { return symbols_.at(static_cast<size_t>(id)); }

char32_t EngravingFont::sym_code(SymId id) const {
    const Sym &s = sym(id);
    if (s.is_valid()) {
        return s.code;
    }

    // fallback: search in the common SMuFL table
    return Smufl::smufl_code(id);
}

SymId EngravingFont::from_code(char32_t code) const {
    auto it = std::find_if(symbols_.begin(), symbols_.end(), [code](const Sym &s) { return s.code == code; });
    return static_cast<SymId>(it == symbols_.end() ? 0 : it - symbols_.begin());
}

String EngravingFont::to_string(SymId id) const { return String::from_ucs4(sym_code(id)); }

bool EngravingFont::is_valid(SymId id) const { return sym(id).is_valid(); }

bool EngravingFont::use_fallback_font(SymId id) const {
    return MScore::use_fallback_font && !sym(id).is_valid() && !engraving_fonts()->is_fallback_font(this);
}

// =============================================
// Symbol bounding box
// =============================================

RectF EngravingFont::bbox(SymId id, double mag) const { return bbox(id, SizeF(mag, mag)); }

RectF EngravingFont::bbox(SymId id, const SizeF &mag) const {
    if (use_fallback_font(id)) {
        return engraving_fonts()->fallback_font()->bbox(id, mag);
    }

    RectF r = sym(id).bbox;
    return r.scale(mag);
}

RectF EngravingFont::bbox(const SymIdList &s, double mag) const { return bbox(s, SizeF(mag, mag)); }

RectF EngravingFont::bbox(const SymIdList &s, const SizeF &mag) const {
    RectF r;
    PointF pos;
    for (SymId id : s) {
        r.unite(bbox(id, mag).translated(pos));
        pos.rx() += advance(id, mag.width());
    }
    return r;
}

Shape EngravingFont::shape(const SymIdList &s, double mag) const { return shape(s, SizeF(mag, mag)); }

Shape EngravingFont::shape(const SymIdList &s, const SizeF &mag) const {
    Shape sh;
    PointF pos(0.0, 0.0);
    for (SymId id : s) {
        sh.add(Shape(bbox(id, mag)).translate(pos));
        pos.rx() += advance(id, mag.width());
    }
    return sh;
}

Shape EngravingFont::shape_with_cutouts(SymId id, double mag) { return shape_with_cutouts(id, SizeF(mag, mag)); }

Shape EngravingFont::shape_with_cutouts(SymId id, const SizeF &mag) {
    Shape &shape = sym(id).shape_with_cutouts;
    if (shape.empty()) {
        construct_shape_with_cutouts(shape, id);
    }

    return shape.scaled(mag);
}

void EngravingFont::construct_shape_with_cutouts(Shape &shape, SymId id) {
    RectF bounding_box = bbox(id, 1.0);
    double bottom = bounding_box.bottom();
    double top = bounding_box.top();
    double left = bounding_box.left();
    double right = bounding_box.right();

    PointF cut_out_nw = smufl_anchor(id, SmuflAnchorId::CUT_OUT_NW, 1.0);
    PointF cut_out_ne = smufl_anchor(id, SmuflAnchorId::CUT_OUT_NE, 1.0);
    PointF cut_out_sw = smufl_anchor(id, SmuflAnchorId::CUT_OUT_SW, 1.0);
    PointF cut_out_se = smufl_anchor(id, SmuflAnchorId::CUT_OUT_SE, 1.0);

    bool nw_null = cut_out_nw.is_null();
    bool ne_null = cut_out_ne.is_null();
    bool sw_null = cut_out_sw.is_null();
    bool se_null = cut_out_se.is_null();

    if (nw_null && ne_null && sw_null && se_null) {
        shape = Shape(bbox(id, 1.0));
        return;
    }

    if (nw_null) {
        cut_out_nw = PointF(left, top);
    }
    if (ne_null) {
        cut_out_ne = PointF(right, top);
    }
    if (sw_null) {
        cut_out_sw = PointF(left, bottom);
    }
    if (se_null) {
        cut_out_se = PointF(right, bottom);
    }

    double left_inset = std::max(cut_out_nw.x(), cut_out_sw.x());
    double right_inset = std::min(cut_out_ne.x(), cut_out_se.x());
    double top_inset = std::max(cut_out_nw.y(), cut_out_ne.y());
    double bottom_inset = std::min(cut_out_sw.y(), cut_out_se.y());

    std::vector<RectF> rects;
    rects.reserve(6); //at most

    // bottom rect
    rects.emplace_back(RectF(PointF(cut_out_sw.x(), bottom), PointF(cut_out_se.x(), top_inset)).normalized());
    // right rect
    bool right_rect_placed = false;
    if (!se_null) {
        rects.emplace_back(RectF(PointF(right, cut_out_se.y()), PointF(left_inset, cut_out_ne.y())).normalized());
        right_rect_placed = true;
    }
    // top rect
    bool top_rect_placed = false;
    if (!right_rect_placed || !ne_null) {
        rects.emplace_back(RectF(PointF(cut_out_nw.x(), top), PointF(cut_out_ne.x(), bottom_inset)).normalized());
        top_rect_placed = true;
    }
    // left rect
    if (!top_rect_placed || !nw_null) {
        rects.emplace_back(RectF(PointF(left, cut_out_sw.y()), PointF(right_inset, cut_out_nw.y())).normalized());
    }
    // center horizontal rect if needed
    if (left_inset > right_inset && top_inset < bottom_inset) {
        rects.emplace_back(RectF(PointF(left, bottom_inset), PointF(right, top_inset)).normalized());
    }
    // center vertical rect if needed
    if (left_inset < right_inset && top_inset > bottom_inset) {
        rects.emplace_back(RectF(PointF(left_inset, bottom), PointF(right_inset, top)).normalized());
    }

    shape = Shape(rects);
}

// =============================================
// Symbol metrics
// =============================================

double EngravingFont::width(SymId id, double mag) const { return bbox(id, mag).width(); }

double EngravingFont::height(SymId id, double mag) const { return bbox(id, mag).height(); }

double EngravingFont::advance(SymId id, double mag) const {
    if (use_fallback_font(id)) {
        return engraving_fonts()->fallback_font()->advance(id, mag);
    }

    return sym(id).advance * mag;
}

double EngravingFont::width(const SymIdList &s, double mag) const { return bbox(s, mag).width(); }

PointF EngravingFont::smufl_anchor(SymId sym_id, SmuflAnchorId anchor_id, double mag) const {
    if (use_fallback_font(sym_id)) {
        return engraving_fonts()->fallback_font()->smufl_anchor(sym_id, anchor_id, mag);
    }

    const muse::FlatMap<SmuflAnchorId, PointF> &smufl_anchors = sym(sym_id).smufl_anchors;

    auto it = smufl_anchors.find(anchor_id);
    if (it == smufl_anchors.cend()) {
        return PointF();
    }

    return it->second * mag;
}

// =============================================
// Draw
// =============================================

void EngravingFont::draw(SymId id, Painter *painter, const SizeF &mag, const PointF &pos, const double angle) const {
    const Sym &sym = this->sym(id);
    if (sym.is_compound()) { // is this a compound symbol?
        draw(sym.sub_symbol_ids, painter, mag, pos, angle);
        return;
    }

    if (!sym.is_valid()) {
        if (MScore::use_fallback_font && !engraving_fonts()->is_fallback_font(this)) {
            engraving_fonts()->fallback_font()->draw(id, painter, mag, pos, angle);
        } else {
            LOGE() << "invalid sym: " << static_cast<size_t>(id);
        }

        return;
    }

    painter->save();
    font_.set_point_size_f(StyleDef::default_smufl_point_size());
    painter->scale(mag.width(), mag.height());
    painter->set_font(font_);
    if (angle != 0) {
        const double width_value = sym.bbox.width() / 2;
        const double height_value = sym.bbox.height() / 2;
        painter->translate(width_value, -height_value);
        painter->rotate(angle);
        painter->translate(-width_value, height_value);
    }
    painter->draw_symbol(PointF(pos.x() / mag.width(), pos.y() / mag.height()), sym_code(id));
    painter->restore();
}

void EngravingFont::draw(SymId id, Painter *painter, double mag, const PointF &pos, const double angle) const {
    draw(id, painter, SizeF(mag, mag), pos, angle);
}

void EngravingFont::draw(const SymIdList &ids, Painter *painter, double mag, const PointF &start_pos, const double angle) const {
    PointF pos(start_pos);
    for (SymId id : ids) {
        draw(id, painter, mag, pos, angle);
        pos.set_x(pos.x() + advance(id, mag));
    }
}

void EngravingFont::draw(const SymIdList &ids, Painter *painter, const SizeF &mag, const PointF &start_pos, const double angle) const {
    PointF pos(start_pos);
    for (SymId id : ids) {
        draw(id, painter, mag, pos, angle);
        pos.set_x(pos.x() + advance(id, mag.width()));
    }
}
