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
#include "mnxtypesconv.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>

#include "engraving/dom/clef.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteval.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/utils.h"
#include "engraving/editing/transpose.h"
#include "engraving/types/typesconv.h"
#include "framework/global/containers.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
namespace {
const std::unordered_map<model::Orientation, ArticulationAnchor> articulation_anchor_table = {
    {model::Orientation::AUTO, ArticulationAnchor::AUTO},
    {model::Orientation::BELOW, ArticulationAnchor::BOTTOM},
    {model::Orientation::ABOVE, ArticulationAnchor::TOP},
};
} // namespace

ArticulationAnchor to_muse_score_articulation_anchor(model::Orientation orient) {
    return muse::value(::mu::iex::mnxio::articulation_anchor_table, orient, ArticulationAnchor::AUTO);
}

model::Orientation to_mnx_orientation(ArticulationAnchor anchor) {
    return muse::key(::mu::iex::mnxio::articulation_anchor_table, anchor, model::Orientation::AUTO);
}

model::Orientation to_mnx_orientation(PlacementV placement) {
    switch (placement) {
        case PlacementV::ABOVE: return model::Orientation::ABOVE;
        case PlacementV::BELOW: return model::Orientation::BELOW;
    }
    ASSERT_X("invalid placement value");
    return model::Orientation::AUTO;
}

namespace {
const std::unordered_map<model::BarlineType, BarLineType> bar_line_type_table = {
    {model::BarlineType::REGULAR, BarLineType::NORMAL},
    {model::BarlineType::DASHED, BarLineType::DASHED},
    {model::BarlineType::DOTTED, BarLineType::DOTTED},
    {model::BarlineType::DOUBLE, BarLineType::DOUBLE},
    {model::BarlineType::FINAL, BarLineType::END},
    {model::BarlineType::HEAVY, BarLineType::HEAVY},
    {model::BarlineType::HEAVY_HEAVY, BarLineType::DOUBLE_HEAVY},
    {model::BarlineType::HEAVY_LIGHT, BarLineType::REVERSE_END},
};
} // namespace

BarLineType to_muse_score_bar_line_type(model::BarlineType blt) {
    return muse::value(::mu::iex::mnxio::bar_line_type_table, blt, BarLineType::NORMAL);
}

model::BarlineType to_mnx_bar_line_type(BarLineType blt) {
    return muse::key(::mu::iex::mnxio::bar_line_type_table, blt, model::BarlineType::REGULAR);
}

std::optional<model::TimeSignatureDisplay> to_mnx_time_signature_display(TimeSigType type) {
    switch (type) {
        case TimeSigType::NORMAL: return std::nullopt;
        case TimeSigType::FOUR_FOUR: return model::TimeSignatureDisplay::COMMON;
        case TimeSigType::ALLA_BREVE: return model::TimeSignatureDisplay::CUT;
        case TimeSigType::CUT_BACH:
        case TimeSigType::CUT_TRIPLE:
            // MNX has no encoding for these glyphs. Exporting them as Cut would silently
            // substitute timeSigCutCommon for timeSigCut2/timeSigCut3 on the way back in,
            // so omit the display and keep the numeric time signature instead.
            LOGW() << "MNX has no time signature glyph for " << int(type) << "; exporting numbers only.";
            return std::nullopt;
    }
    return std::nullopt;
}

TimeSigType to_muse_score_time_sig_type(std::optional<model::TimeSignatureDisplay> display) {
    if (!display) {
        return TimeSigType::NORMAL;
    }
    switch (display.value()) {
        case model::TimeSignatureDisplay::COMMON: return TimeSigType::FOUR_FOUR;
        case model::TimeSignatureDisplay::CUT: return TimeSigType::ALLA_BREVE;
    }
    return TimeSigType::NORMAL;
}

std::optional<model::TimeSignatureUnit> to_mnx_time_signature_unit(int denominator) {
    static const std::unordered_map<int, std::optional<model::TimeSignatureUnit>> units = {
        {1, model::TimeSignatureUnit::WHOLE},  {2, model::TimeSignatureUnit::HALF},     {4, model::TimeSignatureUnit::QUARTER},
        {8, model::TimeSignatureUnit::EIGHTH}, {16, model::TimeSignatureUnit::N16TH},   {32, model::TimeSignatureUnit::N32ND},
        {64, model::TimeSignatureUnit::N64TH}, {128, model::TimeSignatureUnit::N128TH},
    };
    return muse::value(units, denominator, std::nullopt);
}

std::optional<model::Clef> to_mnx_clef(ClefType clef_type) {
    using ClefSign = model::ClefSign;
    using OttavaAmountOrZero = model::OttavaAmountOrZero;

    ClefSign sign{};
    OttavaAmountOrZero octave = OttavaAmountOrZero::NONE;

    switch (clef_type) {
        case ClefType::G:
        case ClefType::G_1: sign = ClefSign::G; break;
        case ClefType::G8_VB:
        case ClefType::G8_VB_O:
        case ClefType::G8_VB_P:
        case ClefType::G8_VB_C:
            sign = ClefSign::G;
            octave = OttavaAmountOrZero::OCTAVE_DOWN;
            break;
        case ClefType::G8_VA:
            sign = ClefSign::G;
            octave = OttavaAmountOrZero::OCTAVE_UP;
            break;
        case ClefType::G15_MB:
            sign = ClefSign::G;
            octave = OttavaAmountOrZero::TWO_OCTAVES_DOWN;
            break;
        case ClefType::G15_MA:
            sign = ClefSign::G;
            octave = OttavaAmountOrZero::TWO_OCTAVES_UP;
            break;
        case ClefType::F:
        case ClefType::F_B:
        case ClefType::F_C:
        case ClefType::F_F18C:
        case ClefType::F_19C: sign = ClefSign::F; break;
        case ClefType::F8_VB:
            sign = ClefSign::F;
            octave = OttavaAmountOrZero::OCTAVE_DOWN;
            break;
        case ClefType::F_8VA:
            sign = ClefSign::F;
            octave = OttavaAmountOrZero::OCTAVE_UP;
            break;
        case ClefType::F15_MB:
            sign = ClefSign::F;
            octave = OttavaAmountOrZero::TWO_OCTAVES_DOWN;
            break;
        case ClefType::F_15MA:
            sign = ClefSign::F;
            octave = OttavaAmountOrZero::TWO_OCTAVES_UP;
            break;
        case ClefType::C1:
        case ClefType::C2:
        case ClefType::C3:
        case ClefType::C4:
        case ClefType::C5:
        case ClefType::C_19C:
        case ClefType::C1_F18C:
        case ClefType::C3_F18C:
        case ClefType::C4_F18C:
        case ClefType::C1_F20C:
        case ClefType::C3_F20C:
        case ClefType::C4_F20C: sign = ClefSign::C; break;
        case ClefType::C4_8VB:
            sign = ClefSign::C;
            octave = OttavaAmountOrZero::OCTAVE_DOWN;
            break;
        case ClefType::INVALID:
        case ClefType::PERC:
        case ClefType::PERC2:
        case ClefType::TAB:
        case ClefType::TAB4:
        case ClefType::TAB_SERIF:
        case ClefType::TAB4_SERIF:
        case ClefType::MAX: return std::nullopt;
    }

    model::Clef clef;
    clef.sign = sign;
    clef.staff_position = (ClefInfo::line(clef_type) - 3) * 2;
    if (octave != OttavaAmountOrZero::NONE) clef.octave = octave;
    return clef;
}

std::optional<model::Pitch> to_mnx_pitch(const Note *note) {
    IF_ASSERT_FAILED(note) { return std::nullopt; }

    const Staff *staff = note->staff();
    const Fraction tick = note->tick();
    const ClefType clef_type = staff->clef(tick);

    if (clef_type == ClefType::PERC || clef_type == ClefType::PERC2 || staff->is_tab_staff(tick)) {
        /// @todo handle tab when MNX supports it
        /// @todo percussion notes will convert to kitNotes (probably elsewhere)
        return std::nullopt;
    }

    // MNX: export CONCERT pitch.
    // In MuseScore, note->tpc1() represents the concert spelling.
    const int concert_tpc = note->tpc1();
    const int step = tpc2step(concert_tpc);
    const int alter = tpc2alter_by_key(concert_tpc, Key::C);

    // Use concert MIDI pitch as the base for octave computation.
    // (Applying instrument->transpose() here tends to double-count transposition.)
    const int base_pitch = note->pitch();
    int octave = (base_pitch - alter) / ::mu::engraving::pitch_delta_octave - 1;

    // Correct for ottava lines: ppitch - pitch in semitones.
    // Convert to octave units, clamp to [-3, +3] (8va/15ma/22ma).
    const int pitch_delta = note->ppitch() - note->pitch();
    if ((pitch_delta % ::mu::engraving::pitch_delta_octave) == 0) {
        int octave_shift = pitch_delta / ::mu::engraving::pitch_delta_octave;
        octave_shift = std::clamp(octave_shift, -3, 3);
        octave += octave_shift;
    } else {
        LOGW() << "Ignored non-octave playback displacement when computing MNX pitch: "
               << "tick=" << tick.ticks() << ", pitch=" << note->pitch() << ", ppitch=" << note->ppitch() << ", delta=" << pitch_delta
               << " (not a multiple of " << ::mu::engraving::pitch_delta_octave << " semitones).";
    }

    model::Pitch pitch;
    pitch.step = model::step_at(step);
    pitch.octave = octave;
    if (alter != 0) pitch.alter = alter;
    return pitch;
}

BeamMode to_muse_score_beam_mode(int lowest_beam_start) {
    static const std::unordered_map<int, BeamMode> beam_mode_table = {
        {0, BeamMode::MID},
        {1, BeamMode::BEGIN},
        {2, BeamMode::BEGIN16},
        {3, BeamMode::BEGIN32},
        // MNX can go arbitrarily higher but MuseScore currently cannot
    };
    return muse::value(beam_mode_table, lowest_beam_start, BeamMode::MID);
}

namespace {
const std::unordered_map<model::StaffSymbol, BracketType> layout_symbol_table = {
    {model::StaffSymbol::NO_SYMBOL, BracketType::NO_BRACKET},
    {model::StaffSymbol::BRACE, BracketType::BRACE},
    {model::StaffSymbol::BRACKET, BracketType::NORMAL},
};
} // namespace

BracketType to_muse_score_bracket_type(model::StaffSymbol lys) {
    return muse::value(::mu::iex::mnxio::layout_symbol_table, lys, BracketType::NO_BRACKET);
}

model::StaffSymbol to_mnx_layout_symbol(BracketType bracket_type) {
    return muse::key(::mu::iex::mnxio::layout_symbol_table, bracket_type, model::StaffSymbol::NO_SYMBOL);
}

namespace {
// MNX names breath marks with free strings; these are the ones MuseScore has glyphs for.
const std::unordered_map<std::string, SymId> breath_mark_table = {
    {"comma", SymId::BREATH_MARK_COMMA},
    {"tick", SymId::BREATH_MARK_TICK},
    {"upbow", SymId::BREATH_MARK_UPBOW},
    {"salzedo", SymId::BREATH_MARK_SALZEDO},
};
} // namespace

SymId to_muse_score_breath_mark_sym(const std::optional<std::string> &br_sym) {
    return muse::value(::mu::iex::mnxio::breath_mark_table, br_sym.value_or("comma"), SymId::BREATH_MARK_COMMA);
}

std::optional<std::string> to_mnx_breath_mark_sym(SymId sym) {
    return muse::key(::mu::iex::mnxio::breath_mark_table, sym, std::optional<std::string>{});
}

MnxDynamicMapping to_mnx_dynamic_type(DynamicType type) {
    using Prefix = model::DynamicPrefix;
    using Suffix = model::DynamicSuffix;
    using Value = model::DynamicValue;

    MnxDynamicMapping result;

    // Accents spell themselves out as accentPrefix + value + accentSuffix + residualValue,
    // so `value` is the initial attack and `residualValue` is what remains after it.
    auto accent = [&](Prefix prefix, Value value, Suffix suffix, std::optional<Value> residual = std::nullopt) {
        result.is_accent = true;
        result.accent_prefix = prefix;
        result.accent_suffix = suffix;
        result.value = value;
        result.residual_value = residual;
    };

    switch (type) {
        case DynamicType::PPPPPP: result.value = Value::PPPPPP; break;
        case DynamicType::PPPPP: result.value = Value::PPPPP; break;
        case DynamicType::PPPP: result.value = Value::PPPP; break;
        case DynamicType::PPP: result.value = Value::PPP; break;
        case DynamicType::PP: result.value = Value::PP; break;
        case DynamicType::P: result.value = Value::P; break;
        case DynamicType::MP: result.value = Value::MP; break;
        case DynamicType::MF: result.value = Value::MF; break;
        case DynamicType::F: result.value = Value::F; break;
        case DynamicType::FF: result.value = Value::FF; break;
        case DynamicType::FFF: result.value = Value::FFF; break;
        case DynamicType::FFFF: result.value = Value::FFFF; break;
        case DynamicType::FFFFF: result.value = Value::FFFFF; break;
        case DynamicType::FFFFFF: result.value = Value::FFFFFF; break;
        case DynamicType::N: result.value = Value::N; break;
        // FP and PF carry no accent letters, but residualValue is only available on an accent,
        // so that is the only MNX dynamic type able to express them.
        case DynamicType::FP: accent(Prefix::NONE, Value::F, Suffix::NONE, Value::P); break;
        case DynamicType::PF: accent(Prefix::NONE, Value::P, Suffix::NONE, Value::F); break;
        case DynamicType::SF: accent(Prefix::S, Value::F, Suffix::NONE); break;
        case DynamicType::SFZ: accent(Prefix::S, Value::F, Suffix::Z); break;
        case DynamicType::FZ: accent(Prefix::NONE, Value::F, Suffix::Z); break;
        case DynamicType::RF: accent(Prefix::R, Value::F, Suffix::NONE); break;
        case DynamicType::RFZ: accent(Prefix::R, Value::F, Suffix::Z); break;
        case DynamicType::SFF: accent(Prefix::S, Value::FF, Suffix::NONE); break;
        case DynamicType::SFFZ: accent(Prefix::S, Value::FF, Suffix::Z); break;
        case DynamicType::SFFF: accent(Prefix::S, Value::FFF, Suffix::NONE); break;
        case DynamicType::SFFFZ: accent(Prefix::S, Value::FFF, Suffix::Z); break;
        case DynamicType::SFP: accent(Prefix::S, Value::F, Suffix::NONE, Value::P); break;
        case DynamicType::SFPP: accent(Prefix::S, Value::F, Suffix::NONE, Value::PP); break;
        // These have no MNX equivalent. The caller falls back to exporting the glyphs.
        case DynamicType::OTHER:
        case DynamicType::M:
        case DynamicType::R:
        case DynamicType::S:
        case DynamicType::Z:
        case DynamicType::LAST: break;
    }

    return result;
}

namespace {
/// Maps each SMuFL dynamic glyph to the letters it spells, derived from MuseScore's own
/// DynamicType table so that ligatures such as dynamicFFF decompose to "fff" without a
/// second table to maintain. Several types share a glyph -- SymId::dynamicSforzando belongs
/// to both S ("s") and SF ("sf") -- and the glyph on its own renders the shortest of them,
/// so the shortest spelling wins. This is also why TConv::dynamicType(SymId) cannot be used
/// here: it returns whichever type the table lists first.
const std::unordered_map<SymId, std::string> &dynamic_glyph_letters() {
    static const std::unordered_map<SymId, std::string> letters = []() {
        std::unordered_map<SymId, std::string> result;
        for (int index = 0; index < int(DynamicType::LAST); index++) {
            const DynamicType type = DynamicType(index);
            const SymId sym_id = TConv::sym_id(type);
            if (sym_id == SymId::NO_SYM) {
                continue;
            }
            const std::string spelling = TConv::to_xml(type).ascii();
            const auto existing = result.find(sym_id);
            if (existing == result.end() || spelling.size() < existing->second.size()) {
                result[sym_id] = spelling;
            }
        }
        return result;
    }();
    return letters;
}

constexpr std::array all_dynamic_values{
    model::DynamicValue::PPP,   model::DynamicValue::PP,     model::DynamicValue::P,      model::DynamicValue::MP,
    model::DynamicValue::MF,    model::DynamicValue::F,      model::DynamicValue::FF,     model::DynamicValue::FFF,
    model::DynamicValue::N,     model::DynamicValue::PPPP,   model::DynamicValue::PPPPP,  model::DynamicValue::FFFF,
    model::DynamicValue::FFFFF, model::DynamicValue::PPPPPP, model::DynamicValue::FFFFFF,
};

/// Consumes the longest dynamic value spelled at @p pos, e.g. "ffff" rather than "f".
std::optional<model::DynamicValue> take_dynamic_value(const std::string &letters, size_t &pos) {
    std::optional<model::DynamicValue> result;
    size_t best_end = pos;
    for (const auto value : all_dynamic_values) {
        const std::string_view spelling = glz::get_enum_name(value);
        const size_t end = pos + spelling.size();
        if (end > best_end && letters.compare(pos, spelling.size(), spelling) == 0) {
            best_end = end;
            result = value;
        }
    }
    pos = best_end;
    return result;
}
} // namespace

std::optional<MnxDynamicMapping> to_mnx_dynamic_from_letters(const std::string &dynamic_letters) {
    // MNX spells a dynamic as accentPrefix + value + accentSuffix + residualValue. Reading the
    // affixes a character at a time is only unambiguous because no DynamicValue spells itself
    // with s, r, or z -- every one of them is a run of p, m, f, or n -- so a peek here can never
    // consume a letter that belongs to the value that follows.
    MnxDynamicMapping mapping;
    size_t pos = 0;
    if (pos < dynamic_letters.size() && (dynamic_letters[pos] == 's' || dynamic_letters[pos] == 'r')) {
        mapping.accent_prefix = dynamic_letters[pos] == 's' ? model::DynamicPrefix::S : model::DynamicPrefix::R;
        pos++;
    }
    mapping.value = take_dynamic_value(dynamic_letters, pos);
    if (!mapping.value) {
        return std::nullopt; // no sounding level, so MNX has nothing to anchor the dynamic to
    }
    if (pos < dynamic_letters.size() && dynamic_letters[pos] == 'z') {
        mapping.accent_suffix = model::DynamicSuffix::Z;
        pos++;
    }
    if (pos < dynamic_letters.size()) {
        mapping.residual_value = take_dynamic_value(dynamic_letters, pos);
        if (!mapping.residual_value) {
            return std::nullopt;
        }
    }
    if (pos != dynamic_letters.size()) {
        return std::nullopt; // trailing letters we cannot account for
    }

    mapping.is_accent = mapping.accent_prefix != model::DynamicPrefix::NONE || mapping.accent_suffix != model::DynamicSuffix::NONE ||
                        mapping.residual_value.has_value();
    return mapping;
}

std::optional<MnxDynamicMapping> to_mnx_dynamic_from_sym_ids(const std::vector<engraving::SymId> &sym_ids) {
    if (sym_ids.empty()) {
        return std::nullopt;
    }

    std::string letters;
    for (const SymId sym_id : sym_ids) {
        const auto it = dynamic_glyph_letters().find(sym_id);
        if (it == dynamic_glyph_letters().end()) {
            return std::nullopt; // not a dynamic spelling
        }
        letters += it->second;
    }

    return to_mnx_dynamic_from_letters(letters);
}

std::optional<DynamicType> to_muse_score_dynamic_type(const MnxDynamicMapping &mapping) {
    if (!mapping.value) {
        return std::nullopt;
    }
    // Search the forward table rather than keeping a second one that could drift from it.
    // Every DynamicType maps to a distinct combination, so the first match is the only one.
    for (int index = 0; index < int(DynamicType::LAST); index++) {
        const DynamicType type = DynamicType(index);
        const MnxDynamicMapping candidate = to_mnx_dynamic_type(type);
        if (!candidate.value || candidate.value != mapping.value || candidate.residual_value != mapping.residual_value ||
            candidate.is_accent != mapping.is_accent) {
            continue;
        }
        // The affixes only distinguish accents; they are meaningless otherwise.
        if (!candidate.is_accent ||
            (candidate.accent_prefix == mapping.accent_prefix && candidate.accent_suffix == mapping.accent_suffix)) {
            return type;
        }
    }
    return std::nullopt;
}

std::vector<std::string> to_muse_score_dynamic_glyph_names(model::DynamicValue value) {
    // Every MNX dynamic value is a run of p/m/f/n, so the SMuFL glyphs follow from the
    // characters of its JSON representation.
    static const std::unordered_map<char, std::string> glyph_names = {
        {'p', "dynamicPiano"},
        {'m', "dynamicMezzo"},
        {'f', "dynamicForte"},
        {'n', "dynamicNiente"},
    };

    std::vector<std::string> result;
    for (const char ch : glz::get_enum_name(value)) {
        const std::string glyph_name = muse::value(glyph_names, ch, std::string{});
        IF_ASSERT_FAILED(!glyph_name.empty()) { continue; }
        result.push_back(glyph_name);
    }
    return result;
}

std::string to_muse_score_dynamic_glyph_name(model::DynamicPrefix prefix) {
    switch (prefix) {
        case model::DynamicPrefix::NONE: return {};
        case model::DynamicPrefix::R: return "dynamicRinforzando";
        case model::DynamicPrefix::S: return "dynamicSforzando";
    }
    return {};
}

std::string to_muse_score_dynamic_glyph_name(model::DynamicSuffix suffix) {
    switch (suffix) {
        case model::DynamicSuffix::NONE: return {};
        case model::DynamicSuffix::Z: return "dynamicZ";
    }
    return {};
}

namespace {
const std::unordered_map<model::NoteValueBase, DurationType> dura_type_table = {
    {model::NoteValueBase::N4096TH, DurationType::V_INVALID},
    {model::NoteValueBase::N2048TH, DurationType::V_INVALID},
    {model::NoteValueBase::N1024TH, DurationType::V_1024TH},
    {model::NoteValueBase::N512TH, DurationType::V_512TH},
    {model::NoteValueBase::N256TH, DurationType::V_256TH},
    {model::NoteValueBase::N128TH, DurationType::V_128TH},
    {model::NoteValueBase::N64TH, DurationType::V_64TH},
    {model::NoteValueBase::N32ND, DurationType::V_32ND},
    {model::NoteValueBase::N16TH, DurationType::V_16TH},
    {model::NoteValueBase::EIGHTH, DurationType::V_EIGHTH},
    {model::NoteValueBase::QUARTER, DurationType::V_QUARTER},
    {model::NoteValueBase::HALF, DurationType::V_HALF},
    {model::NoteValueBase::WHOLE, DurationType::V_WHOLE},
    {model::NoteValueBase::BREVE, DurationType::V_BREVE},
    {model::NoteValueBase::LONGA, DurationType::V_LONG},
    {model::NoteValueBase::MAXIMA, DurationType::V_INVALID},
    {model::NoteValueBase::DUPLEX_MAXIMA, DurationType::V_INVALID},
};
} // namespace

DurationType to_muse_score_duration_type(model::NoteValueBase nvb) {
    return muse::value(::mu::iex::mnxio::dura_type_table, nvb, DurationType::V_INVALID);
}

TDuration to_muse_score_duration(const model::NoteValue &nv) {
    if (nv.dots_or_default() > ::mu::engraving::max_dots) {
        return TDuration(); // invalid
    }
    return TDuration(DurationTypeWithDots(to_muse_score_duration_type(nv.base), nv.dots_or_default()));
}

std::optional<model::NoteValue> to_mnx_note_value(const TDuration &duration) {
    const auto base = muse::key(::mu::iex::mnxio::dura_type_table, duration.type(), std::optional<model::NoteValueBase>{});
    if (!base) {
        return std::nullopt;
    }

    model::NoteValue value;
    value.base = *base;
    if (duration.dots() > 0) value.dots = duration.dots();
    return value;
}

namespace {
const std::unordered_map<model::FermataSymbol, SymId> sym_id_to_fermata_sym{
    {model::FermataSymbol::NORMAL, SymId::FERMATA_ABOVE},
    {model::FermataSymbol::ANGLED, SymId::FERMATA_SHORT_ABOVE},
    {model::FermataSymbol::SQUARE, SymId::FERMATA_LONG_ABOVE},
    {model::FermataSymbol::DOUBLE_ANGLED, SymId::FERMATA_VERY_SHORT_ABOVE},
    {model::FermataSymbol::DOUBLE_SQUARE, SymId::FERMATA_VERY_LONG_ABOVE},
    {model::FermataSymbol::DOUBLE_DOT, SymId::FERMATA_LONG_HENZE_ABOVE},
    {model::FermataSymbol::HALF_CURVE, SymId::FERMATA_SHORT_HENZE_ABOVE},
    {model::FermataSymbol::CURLEW, SymId::CURLEW_SIGN},
};
} // namespace

SymId to_muse_score_fermata_sym_id(const model::FermataSymbol fermata_symbol) {
    return muse::value(::mu::iex::mnxio::sym_id_to_fermata_sym, fermata_symbol, SymId::FERMATA_ABOVE);
}

namespace {
const std::unordered_map<FermataType, model::FermataDuration> fermata_dura_table{
    {FermataType::NORMAL, model::FermataDuration::NORMAL},         {FermataType::LONG, model::FermataDuration::LONG},
    {FermataType::LONG_HENZE, model::FermataDuration::LONG},       {FermataType::SHORT, model::FermataDuration::SHORT},
    {FermataType::SHORT_HENZE, model::FermataDuration::SHORT},     {FermataType::VERY_LONG, model::FermataDuration::VERY_LONG},
    {FermataType::VERY_SHORT, model::FermataDuration::VERY_SHORT},
};
} // namespace

model::FermataDuration to_mnx_fermata_duration(FermataType fermata_type) {
    return muse::value(::mu::iex::mnxio::fermata_dura_table, fermata_type, model::FermataDuration::AUTO);
}

namespace {
const std::unordered_map<SymId, model::FermataSymbol> fermata_sym_to_sym_id{
    {SymId::FERMATA_ABOVE, model::FermataSymbol::NORMAL},
    {SymId::FERMATA_BELOW, model::FermataSymbol::NORMAL},
    {SymId::FERMATA_SHORT_ABOVE, model::FermataSymbol::ANGLED},
    {SymId::FERMATA_SHORT_BELOW, model::FermataSymbol::ANGLED},
    {
        SymId::FERMATA_LONG_ABOVE,
        model::FermataSymbol::SQUARE,
    },
    {
        SymId::FERMATA_LONG_BELOW,
        model::FermataSymbol::SQUARE,
    },
    {SymId::FERMATA_VERY_SHORT_ABOVE, model::FermataSymbol::DOUBLE_ANGLED},
    {SymId::FERMATA_VERY_SHORT_BELOW, model::FermataSymbol::DOUBLE_ANGLED},
    {SymId::FERMATA_VERY_LONG_ABOVE, model::FermataSymbol::DOUBLE_SQUARE},
    {SymId::FERMATA_VERY_LONG_BELOW, model::FermataSymbol::DOUBLE_SQUARE},
    {SymId::FERMATA_LONG_HENZE_ABOVE, model::FermataSymbol::DOUBLE_DOT},
    {SymId::FERMATA_LONG_HENZE_BELOW, model::FermataSymbol::DOUBLE_DOT},
    {SymId::FERMATA_SHORT_HENZE_ABOVE, model::FermataSymbol::HALF_CURVE},
    {SymId::FERMATA_SHORT_HENZE_BELOW, model::FermataSymbol::HALF_CURVE},
    {SymId::CURLEW_SIGN, model::FermataSymbol::CURLEW},
};
} // namespace

model::FermataSymbol to_mnx_fermata_symbol(SymId sym) {
    return muse::value(::mu::iex::mnxio::fermata_sym_to_sym_id, sym, model::FermataSymbol::NORMAL);
}

namespace {
/// @todo Grow this table as MNX grows it.
const std::unordered_map<model::JumpType, JumpType> jump_table = {
    {model::JumpType::DSALFINE, JumpType::DS_AL_FINE},
    {model::JumpType::SEGNO, JumpType::DSS},
};
} // namespace

JumpType to_muse_score_jump_type(model::JumpType jt) { return muse::value(::mu::iex::mnxio::jump_table, jt, JumpType::USER); }

std::optional<model::JumpType> to_mnx_jump_type(JumpType jt) {
    return muse::key(::mu::iex::mnxio::jump_table, jt, std::optional<model::JumpType>{});
}

namespace {
const std::unordered_map<model::EventLyricLineType, LyricsSyllabic> line_type_table = {
    {model::EventLyricLineType::WHOLE, LyricsSyllabic::SINGLE},
    {model::EventLyricLineType::START, LyricsSyllabic::BEGIN},
    {model::EventLyricLineType::MIDDLE, LyricsSyllabic::MIDDLE},
    {model::EventLyricLineType::END, LyricsSyllabic::END},
};
} // namespace

LyricsSyllabic to_muse_score_lyrics_syllabic(model::EventLyricLineType llt) {
    return muse::value(::mu::iex::mnxio::line_type_table, llt, LyricsSyllabic::SINGLE);
}

model::EventLyricLineType to_mnx_lyric_line_type(LyricsSyllabic ls) {
    return muse::key(::mu::iex::mnxio::line_type_table, ls, model::EventLyricLineType::WHOLE);
}

namespace {
const std::unordered_map<model::OttavaAmount, OttavaType> ottava_type_table = {
    {model::OttavaAmount::OCTAVE_DOWN, OttavaType::OTTAVA_8VB},         {model::OttavaAmount::OCTAVE_UP, OttavaType::OTTAVA_8VA},
    {model::OttavaAmount::TWO_OCTAVES_DOWN, OttavaType::OTTAVA_15MB},   {model::OttavaAmount::TWO_OCTAVES_UP, OttavaType::OTTAVA_15MA},
    {model::OttavaAmount::THREE_OCTAVES_DOWN, OttavaType::OTTAVA_22MB}, {model::OttavaAmount::THREE_OCTAVES_UP, OttavaType::OTTAVA_22MA},
};
} // namespace

OttavaType to_muse_score_ottava_type(model::OttavaAmount ottava_amount) {
    return muse::value(::mu::iex::mnxio::ottava_type_table, ottava_amount, OttavaType::OTTAVA_8VA);
}

std::optional<model::OttavaAmount> to_mnx_ottava_amount(OttavaType ottava_type) {
    return muse::key(::mu::iex::mnxio::ottava_type_table, ottava_type, std::optional<model::OttavaAmount>{});
}

Fraction to_muse_score_r_tick(const model::RhythmicPosition &position) {
    return to_muse_score_fraction(model::Rational(position.fraction));
}

namespace {
const std::unordered_map<int, TremoloType> tremolo_type_table = {
    {1, TremoloType::C8},
    {2, TremoloType::C16},
    {3, TremoloType::C32},
    {4, TremoloType::C64},
};
} // namespace

TremoloType to_muse_score_tremolo_type(int number_of_beams) {
    return muse::value(::mu::iex::mnxio::tremolo_type_table, number_of_beams, TremoloType::INVALID_TREMOLO);
}

std::optional<int> to_mnx_tremolo_marks(TremoloType tt) {
    return muse::key(::mu::iex::mnxio::tremolo_type_table, tt, std::optional<int>{});
}

namespace {
const std::unordered_map<model::LineType, SlurStyleType> slur_style_table = {
    {model::LineType::DASHED, SlurStyleType::DASHED},
    {model::LineType::DOTTED, SlurStyleType::DOTTED},
    {model::LineType::SOLID, SlurStyleType::SOLID},
};
} // namespace

SlurStyleType to_muse_score_slur_style_type(model::LineType line_type) {
    return muse::value(::mu::iex::mnxio::slur_style_table, line_type, SlurStyleType::SOLID);
}

model::LineType to_mnx_slur_line_type(SlurStyleType sst) {
    return muse::key(::mu::iex::mnxio::slur_style_table, sst, model::LineType::SOLID);
}

namespace {
const std::unordered_map<model::YesNoAuto, TupletBracketType> tuplet_bracket_type_table = {
    {model::YesNoAuto::AUTO, TupletBracketType::AUTO_BRACKET},
    {model::YesNoAuto::YES, TupletBracketType::SHOW_BRACKET},
    {model::YesNoAuto::NO, TupletBracketType::SHOW_NO_BRACKET},
};
} // namespace

TupletBracketType to_muse_score_tuplet_bracket_type(model::YesNoAuto bracket_option) {
    return muse::value(::mu::iex::mnxio::tuplet_bracket_type_table, bracket_option, TupletBracketType::AUTO_BRACKET);
}

model::YesNoAuto to_mnx_tuplet_bracket_type(TupletBracketType bracket_option) {
    return muse::key(::mu::iex::mnxio::tuplet_bracket_type_table, bracket_option, model::YesNoAuto::AUTO);
}

namespace {
const std::unordered_map<model::TupletDisplaySetting, TupletNumberType> tuplet_number_type_table = {
    {model::TupletDisplaySetting::INNER, TupletNumberType::SHOW_NUMBER},
    {model::TupletDisplaySetting::NO_NUMBER, TupletNumberType::NO_TEXT},
    {model::TupletDisplaySetting::BOTH, TupletNumberType::SHOW_RELATION},
};
} // namespace

TupletNumberType to_muse_score_tuplet_number_type(model::TupletDisplaySetting number_style) {
    return muse::value(::mu::iex::mnxio::tuplet_number_type_table, number_style, TupletNumberType::SHOW_NUMBER);
}

model::TupletDisplaySetting to_mnx_tuplet_number_type(TupletNumberType number_style) {
    return muse::key(::mu::iex::mnxio::tuplet_number_type_table, number_style, model::TupletDisplaySetting::INNER);
}

NoteVal to_muse_score_note_val(const model::SpelledPitch &pitch, Key key, int octave_shift) {
    int step = model::step_index(pitch.step);
    int alteration = pitch.alter;
    NoteVal nval;
    nval.pitch = 60 /*middle C*/ + (octave_shift + pitch.octave - 4) * ::mu::engraving::pitch_delta_octave + step2pitch(step) + alteration;
    if (alteration < int(AccidentalVal::MIN) || alteration > int(AccidentalVal::MAX) || !pitch_is_valid(nval.pitch)) {
        nval.pitch = clamp_pitch(nval.pitch);
        nval.tpc1 = pitch2tpc(nval.pitch, key, Prefer::NEAREST);
        LOGW() << "Enharmonically transposing pitch with alteration value out of range.";
    } else {
        nval.tpc1 = step2tpc(step, AccidentalVal(alteration));
    }
    nval.tpc2 = nval.tpc1;
    return nval;
}

ClefType to_muse_score_clef_type(const model::Clef &mnx_clef) {
    using ClefSign = model::ClefSign;
    using OttavaAmountOrZero = model::OttavaAmountOrZero;

    const auto snap_to_line = [](int staff_position_half_spaces) -> int {
        // clefTable only supports clefs centered on lines (even half-spaces).
        if (staff_position_half_spaces & 1) {
            staff_position_half_spaces += (staff_position_half_spaces > 0 ? -1 : +1); // snap toward 0
        }
        return staff_position_half_spaces;
    };

    const int sp = snap_to_line(mnx_clef.staff_position);
    const auto sign = mnx_clef.sign;
    const auto octave = mnx_clef.octave_or_default();

    switch (sign) {
        case ClefSign::G:
            // G clef: standard at sp == -2, also supports G_1 at sp == -4
            if (sp == -4) {
                return ClefType::G_1; // best-effort: ignore octave for this placement
            }

            switch (octave) {
                case OttavaAmountOrZero::NONE: return ClefType::G;
                case OttavaAmountOrZero::TWO_OCTAVES_DOWN: return ClefType::G15_MB;
                case OttavaAmountOrZero::OCTAVE_DOWN: return ClefType::G8_VB;
                case OttavaAmountOrZero::OCTAVE_UP: return ClefType::G8_VA;
                case OttavaAmountOrZero::TWO_OCTAVES_UP: return ClefType::G15_MA;
                default: return ClefType::INVALID;
            }

        case ClefSign::F:
            // F clef: supports sp == 0 (F_B line 3), sp == +2 (F line 4), sp == +4 (F_C line 5)
            if (sp == 0) {
                return ClefType::F_B; // best-effort: ignore octave for this placement
            }
            if (sp == +4) {
                return ClefType::F_C; // best-effort: ignore octave for this placement
            }

            switch (octave) {
                case OttavaAmountOrZero::NONE: return ClefType::F;
                case OttavaAmountOrZero::TWO_OCTAVES_DOWN: return ClefType::F15_MB;
                case OttavaAmountOrZero::OCTAVE_DOWN: return ClefType::F8_VB;
                case OttavaAmountOrZero::OCTAVE_UP: return ClefType::F_8VA;
                case OttavaAmountOrZero::TWO_OCTAVES_UP: return ClefType::F_15MA;
                default: return ClefType::INVALID;
            }

        case ClefSign::C:
            // Special-case explicit octave variant in your clefTable
            if (sp == +2 && octave == OttavaAmountOrZero::OCTAVE_DOWN) {
                return ClefType::C4_8VB;
            }

            switch (sp) {
                case -4: return ClefType::C1;
                case -2: return ClefType::C2;
                case 0: return ClefType::C3;
                case +2: return ClefType::C4;
                case +4: return ClefType::C5;
                default:
                    // If MNX gives something outside these, pick a sane fallback:
                    return ClefType::C3;
            }

        default: return ClefType::INVALID;
    }
}

Fraction to_muse_score_fraction(const model::Rational &fraction) {
    return Fraction(static_cast<int>(fraction.numerator), static_cast<int>(fraction.denominator));
}

model::Fraction to_mnx_fraction(const Fraction &fraction) { return {fraction.numerator(), fraction.denominator()}; }

Key to_muse_score_key(int fifths) {
    if (fifths < int(Key::MIN) || fifths > int(Key::MAX)) {
        return Key::INVALID;
    }
    return Key(fifths);
}

PreferSharpFlat to_muse_score_prefer_sharp_flat(int key_fifths_flip_at) {
    switch (key_fifths_flip_at) {
        case -5: return PreferSharpFlat::SHARPS;
        case 5: return PreferSharpFlat::FLATS;
        case -6:
        case 6:
        case -7:
        case 7: return PreferSharpFlat::AUTO;
        default: return PreferSharpFlat::NONE;
    }
}

PlacementV to_muse_score_placement_v(const model::Orientation orient, const EngravingItem *item) {
    switch (orient) {
        case model::Orientation::ABOVE: return PlacementV::ABOVE;
        case model::Orientation::BELOW: return PlacementV::BELOW;
        case model::Orientation::AUTO: IF_ASSERT_FAILED(item) { break;
            }
            return item->property_default(Pid::PLACEMENT).value<PlacementV>();
    }
    return PlacementV::ABOVE;
}

int to_mnx_key_fifths_flip_value(PreferSharpFlat prefer, const Interval &key_transposition) {
    switch (prefer) {
        case PreferSharpFlat::FLATS: return 5;
        case PreferSharpFlat::SHARPS: return -5;
        case PreferSharpFlat::AUTO:
        case PreferSharpFlat::NONE:
        default: {
            const int abs_flip_at = (prefer == PreferSharpFlat::AUTO) ? 7 : 8;
            const int key_delta = int(Transpose::transpose_key(Key::C, key_transposition, prefer)) - int(Key::C);
            return key_delta >= 0 ? abs_flip_at : -abs_flip_at;
        }
    }
}

NoteType dura_type_to_grace_note_type(DurationType type, bool use_left) {
    if (int(type) < int(DurationType::V_EIGHTH)) {
        return use_left ? NoteType::GRACE8_AFTER : NoteType::GRACE4;
    }
    if (int(type) >= int(DurationType::V_32ND)) {
        return use_left ? NoteType::GRACE32_AFTER : NoteType::GRACE32;
    }
    if (type == DurationType::V_16TH) {
        return use_left ? NoteType::GRACE16_AFTER : NoteType::GRACE16;
    }
    return use_left ? NoteType::GRACE8_AFTER : NoteType::APPOGGIATURA;
}

// Generates a stable MNX voice identifier string from a MuseScore track index.
// This is a convention used by the MNX import/export layer and is not defined
// by the MNX specification itself.
std::string make_mnx_voice_id_from_track(int mnx_part_staff_num, track_idx_t cur_track_idx) {
    return "s" + std::to_string(mnx_part_staff_num) + "v" + std::to_string(cur_track_idx % ::mu::engraving::voices + 1);
}
} // namespace mu::iex::mnxio
