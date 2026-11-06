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
const std::unordered_map<mnx::Orientation, ArticulationAnchor> articulation_anchor_table = {
    {mnx::Orientation::Auto, ArticulationAnchor::AUTO},
    {mnx::Orientation::Below, ArticulationAnchor::BOTTOM},
    {mnx::Orientation::Above, ArticulationAnchor::TOP},
};
} // namespace

ArticulationAnchor to_muse_score_articulation_anchor(mnx::Orientation orient) {
    return muse::value(::mu::iex::mnxio::articulation_anchor_table, orient, ArticulationAnchor::AUTO);
}

mnx::Orientation to_mnx_orientation(ArticulationAnchor anchor) {
    return muse::key(::mu::iex::mnxio::articulation_anchor_table, anchor, mnx::Orientation::Auto);
}

mnx::Orientation to_mnx_orientation(PlacementV placement) {
    switch (placement) {
        case PlacementV::ABOVE: return mnx::Orientation::Above;
        case PlacementV::BELOW: return mnx::Orientation::Below;
    }
    ASSERT_X("invalid placement value");
    return mnx::Orientation::Auto;
}

namespace {
const std::unordered_map<mnx::BarlineType, BarLineType> bar_line_type_table = {
    {mnx::BarlineType::Regular, BarLineType::NORMAL},
    {mnx::BarlineType::Dashed, BarLineType::DASHED},
    {mnx::BarlineType::Dotted, BarLineType::DOTTED},
    {mnx::BarlineType::Double, BarLineType::DOUBLE},
    {mnx::BarlineType::Final, BarLineType::END},
    {mnx::BarlineType::Heavy, BarLineType::HEAVY},
    {mnx::BarlineType::HeavyHeavy, BarLineType::DOUBLE_HEAVY},
    {mnx::BarlineType::HeavyLight, BarLineType::REVERSE_END},
};
} // namespace

BarLineType to_muse_score_bar_line_type(mnx::BarlineType blt) {
    return muse::value(::mu::iex::mnxio::bar_line_type_table, blt, BarLineType::NORMAL);
}

mnx::BarlineType to_mnx_bar_line_type(BarLineType blt) {
    return muse::key(::mu::iex::mnxio::bar_line_type_table, blt, mnx::BarlineType::Regular);
}

std::optional<mnx::TimeSignatureDisplay> to_mnx_time_signature_display(TimeSigType type) {
    switch (type) {
        case TimeSigType::NORMAL: return std::nullopt;
        case TimeSigType::FOUR_FOUR: return mnx::TimeSignatureDisplay::Common;
        case TimeSigType::ALLA_BREVE: return mnx::TimeSignatureDisplay::Cut;
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

TimeSigType to_muse_score_time_sig_type(std::optional<mnx::TimeSignatureDisplay> display) {
    if (!display) {
        return TimeSigType::NORMAL;
    }
    switch (display.value()) {
        case mnx::TimeSignatureDisplay::Common: return TimeSigType::FOUR_FOUR;
        case mnx::TimeSignatureDisplay::Cut: return TimeSigType::ALLA_BREVE;
    }
    return TimeSigType::NORMAL;
}

std::optional<mnx::TimeSignatureUnit> to_mnx_time_signature_unit(int denominator) {
    static const std::unordered_map<int, std::optional<mnx::TimeSignatureUnit>> units = {
        {1, mnx::TimeSignatureUnit::Whole},      {2, mnx::TimeSignatureUnit::Half},         {4, mnx::TimeSignatureUnit::Quarter},
        {8, mnx::TimeSignatureUnit::Eighth},     {16, mnx::TimeSignatureUnit::Value16th},   {32, mnx::TimeSignatureUnit::Value32nd},
        {64, mnx::TimeSignatureUnit::Value64th}, {128, mnx::TimeSignatureUnit::Value128th},
    };
    return muse::value(units, denominator, std::nullopt);
}

std::optional<mnx::part::Clef::Required> to_mnx_clef(ClefType clef_type) {
    using Required = mnx::part::Clef::Required;
    using ClefSign = mnx::ClefSign;
    using OttavaAmountOrZero = mnx::OttavaAmountOrZero;

    ClefSign sign{};
    OttavaAmountOrZero octave = OttavaAmountOrZero::NoTransposition;

    switch (clef_type) {
        case ClefType::G:
        case ClefType::G_1: sign = ClefSign::GClef; break;
        case ClefType::G8_VB:
        case ClefType::G8_VB_O:
        case ClefType::G8_VB_P:
        case ClefType::G8_VB_C:
            sign = ClefSign::GClef;
            octave = OttavaAmountOrZero::OctaveDown;
            break;
        case ClefType::G8_VA:
            sign = ClefSign::GClef;
            octave = OttavaAmountOrZero::OctaveUp;
            break;
        case ClefType::G15_MB:
            sign = ClefSign::GClef;
            octave = OttavaAmountOrZero::TwoOctavesDown;
            break;
        case ClefType::G15_MA:
            sign = ClefSign::GClef;
            octave = OttavaAmountOrZero::TwoOctavesUp;
            break;
        case ClefType::F:
        case ClefType::F_B:
        case ClefType::F_C:
        case ClefType::F_F18C:
        case ClefType::F_19C: sign = ClefSign::FClef; break;
        case ClefType::F8_VB:
            sign = ClefSign::FClef;
            octave = OttavaAmountOrZero::OctaveDown;
            break;
        case ClefType::F_8VA:
            sign = ClefSign::FClef;
            octave = OttavaAmountOrZero::OctaveUp;
            break;
        case ClefType::F15_MB:
            sign = ClefSign::FClef;
            octave = OttavaAmountOrZero::TwoOctavesDown;
            break;
        case ClefType::F_15MA:
            sign = ClefSign::FClef;
            octave = OttavaAmountOrZero::TwoOctavesUp;
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
        case ClefType::C4_F20C: sign = ClefSign::CClef; break;
        case ClefType::C4_8VB:
            sign = ClefSign::CClef;
            octave = OttavaAmountOrZero::OctaveDown;
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

    const int staff_position = (ClefInfo::line(clef_type) - 3) * 2;
    return Required{sign, staff_position, octave};
}

std::optional<mnx::sequence::Pitch::Required> to_mnx_pitch(const Note *note) {
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

    return mnx::sequence::Pitch::make(static_cast<mnx::NoteStep>(step), octave, alter);
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
const std::unordered_map<mnx::LayoutSymbol, BracketType> layout_symbol_table = {
    {mnx::LayoutSymbol::NoSymbol, BracketType::NO_BRACKET},
    {mnx::LayoutSymbol::Brace, BracketType::BRACE},
    {mnx::LayoutSymbol::Bracket, BracketType::NORMAL},
};
} // namespace

BracketType to_muse_score_bracket_type(mnx::LayoutSymbol lys) {
    return muse::value(::mu::iex::mnxio::layout_symbol_table, lys, BracketType::NO_BRACKET);
}

mnx::LayoutSymbol to_mnx_layout_symbol(BracketType bracket_type) {
    return muse::key(::mu::iex::mnxio::layout_symbol_table, bracket_type, mnx::LayoutSymbol::NoSymbol);
}

namespace {
const std::unordered_map<mnx::BreathMarkSymbol, SymId> breath_mark_table = {
    {mnx::BreathMarkSymbol::Comma, SymId::BREATH_MARK_COMMA},
    {mnx::BreathMarkSymbol::Tick, SymId::BREATH_MARK_TICK},
    {mnx::BreathMarkSymbol::Upbow, SymId::BREATH_MARK_UPBOW},
    {mnx::BreathMarkSymbol::Salzedo, SymId::BREATH_MARK_SALZEDO},
};
} // namespace

SymId to_muse_score_breath_mark_sym(std::optional<mnx::BreathMarkSymbol> br_sym) {
    using BreathMark = mnx::BreathMarkSymbol;
    return muse::value(::mu::iex::mnxio::breath_mark_table, br_sym.value_or(BreathMark::Comma), SymId::BREATH_MARK_COMMA);
}

std::optional<mnx::BreathMarkSymbol> to_mnx_breath_mark_sym(SymId sym) {
    return muse::key(::mu::iex::mnxio::breath_mark_table, sym, std::optional<mnx::BreathMarkSymbol>{});
}

MnxDynamicMapping to_mnx_dynamic_type(DynamicType type) {
    using Prefix = mnx::DynamicPrefix;
    using Suffix = mnx::DynamicSuffix;
    using Value = mnx::DynamicValue;

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
        case DynamicType::PPPPPP: result.value = Value::pppppp; break;
        case DynamicType::PPPPP: result.value = Value::ppppp; break;
        case DynamicType::PPPP: result.value = Value::pppp; break;
        case DynamicType::PPP: result.value = Value::ppp; break;
        case DynamicType::PP: result.value = Value::pp; break;
        case DynamicType::P: result.value = Value::p; break;
        case DynamicType::MP: result.value = Value::mp; break;
        case DynamicType::MF: result.value = Value::mf; break;
        case DynamicType::F: result.value = Value::f; break;
        case DynamicType::FF: result.value = Value::ff; break;
        case DynamicType::FFF: result.value = Value::fff; break;
        case DynamicType::FFFF: result.value = Value::ffff; break;
        case DynamicType::FFFFF: result.value = Value::fffff; break;
        case DynamicType::FFFFFF: result.value = Value::ffffff; break;
        case DynamicType::N: result.value = Value::n; break;
        // FP and PF carry no accent letters, but residualValue is only available on an accent,
        // so that is the only MNX dynamic type able to express them.
        case DynamicType::FP: accent(Prefix::None, Value::f, Suffix::None, Value::p); break;
        case DynamicType::PF: accent(Prefix::None, Value::p, Suffix::None, Value::f); break;
        case DynamicType::SF: accent(Prefix::s, Value::f, Suffix::None); break;
        case DynamicType::SFZ: accent(Prefix::s, Value::f, Suffix::z); break;
        case DynamicType::FZ: accent(Prefix::None, Value::f, Suffix::z); break;
        case DynamicType::RF: accent(Prefix::r, Value::f, Suffix::None); break;
        case DynamicType::RFZ: accent(Prefix::r, Value::f, Suffix::z); break;
        case DynamicType::SFF: accent(Prefix::s, Value::ff, Suffix::None); break;
        case DynamicType::SFFZ: accent(Prefix::s, Value::ff, Suffix::z); break;
        case DynamicType::SFFF: accent(Prefix::s, Value::fff, Suffix::None); break;
        case DynamicType::SFFFZ: accent(Prefix::s, Value::fff, Suffix::z); break;
        case DynamicType::SFP: accent(Prefix::s, Value::f, Suffix::None, Value::p); break;
        case DynamicType::SFPP: accent(Prefix::s, Value::f, Suffix::None, Value::pp); break;
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

/// Consumes the longest dynamic value spelled at @p pos, e.g. "ffff" rather than "f".
std::optional<mnx::DynamicValue> take_dynamic_value(const std::string &letters, size_t &pos) {
    std::optional<mnx::DynamicValue> result;
    size_t best_end = pos;
    for (const auto &[value, spelling] : mnx::EnumStringMapping<mnx::DynamicValue>::enumToString()) {
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
        mapping.accent_prefix = dynamic_letters[pos] == 's' ? mnx::DynamicPrefix::s : mnx::DynamicPrefix::r;
        pos++;
    }
    mapping.value = take_dynamic_value(dynamic_letters, pos);
    if (!mapping.value) {
        return std::nullopt; // no sounding level, so MNX has nothing to anchor the dynamic to
    }
    if (pos < dynamic_letters.size() && dynamic_letters[pos] == 'z') {
        mapping.accent_suffix = mnx::DynamicSuffix::z;
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

    mapping.is_accent = mapping.accent_prefix != mnx::DynamicPrefix::None || mapping.accent_suffix != mnx::DynamicSuffix::None ||
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

std::vector<std::string> to_muse_score_dynamic_glyph_names(mnx::DynamicValue value) {
    // Every MNX dynamic value is a run of p/m/f/n, so the SMuFL glyphs follow from the
    // characters of its JSON representation.
    static const std::unordered_map<char, std::string> glyph_names = {
        {'p', "dynamicPiano"},
        {'m', "dynamicMezzo"},
        {'f', "dynamicForte"},
        {'n', "dynamicNiente"},
    };

    const auto &value_names = mnx::EnumStringMapping<mnx::DynamicValue>::enumToString();
    std::vector<std::string> result;
    for (const char ch : muse::value(value_names, value, std::string{})) {
        const std::string glyph_name = muse::value(glyph_names, ch, std::string{});
        IF_ASSERT_FAILED(!glyph_name.empty()) { continue; }
        result.push_back(glyph_name);
    }
    return result;
}

std::string to_muse_score_dynamic_glyph_name(mnx::DynamicPrefix prefix) {
    switch (prefix) {
        case mnx::DynamicPrefix::None: return {};
        case mnx::DynamicPrefix::r: return "dynamicRinforzando";
        case mnx::DynamicPrefix::s: return "dynamicSforzando";
    }
    return {};
}

std::string to_muse_score_dynamic_glyph_name(mnx::DynamicSuffix suffix) {
    switch (suffix) {
        case mnx::DynamicSuffix::None: return {};
        case mnx::DynamicSuffix::z: return "dynamicZ";
    }
    return {};
}

namespace {
const std::unordered_map<mnx::NoteValueBase, DurationType> dura_type_table = {
    {mnx::NoteValueBase::Note4096th, DurationType::V_INVALID},   {mnx::NoteValueBase::Note2048th, DurationType::V_INVALID},
    {mnx::NoteValueBase::Note1024th, DurationType::V_1024TH},    {mnx::NoteValueBase::Note512th, DurationType::V_512TH},
    {mnx::NoteValueBase::Note256th, DurationType::V_256TH},      {mnx::NoteValueBase::Note128th, DurationType::V_128TH},
    {mnx::NoteValueBase::Note64th, DurationType::V_64TH},        {mnx::NoteValueBase::Note32nd, DurationType::V_32ND},
    {mnx::NoteValueBase::Note16th, DurationType::V_16TH},        {mnx::NoteValueBase::Eighth, DurationType::V_EIGHTH},
    {mnx::NoteValueBase::Quarter, DurationType::V_QUARTER},      {mnx::NoteValueBase::Half, DurationType::V_HALF},
    {mnx::NoteValueBase::Whole, DurationType::V_WHOLE},          {mnx::NoteValueBase::Breve, DurationType::V_BREVE},
    {mnx::NoteValueBase::Longa, DurationType::V_LONG},           {mnx::NoteValueBase::Maxima, DurationType::V_INVALID},
    {mnx::NoteValueBase::DuplexMaxima, DurationType::V_INVALID},
};
} // namespace

DurationType to_muse_score_duration_type(mnx::NoteValueBase nvb) {
    return muse::value(::mu::iex::mnxio::dura_type_table, nvb, DurationType::V_INVALID);
}

TDuration to_muse_score_duration(mnx::NoteValue nv) {
    if (nv.dots() > ::mu::engraving::max_dots) {
        return TDuration(); // invalid
    }
    return TDuration(DurationTypeWithDots(to_muse_score_duration_type(nv.base()), nv.dots()));
}

std::optional<mnx::NoteValue::Required> to_mnx_note_value(const TDuration &duration) {
    const auto base = muse::key(::mu::iex::mnxio::dura_type_table, duration.type(), std::optional<mnx::NoteValueBase>{});
    if (!base) {
        return std::nullopt;
    }

    return mnx::NoteValue::make(*base, static_cast<unsigned>(duration.dots()));
}

namespace {
const std::unordered_map<mnx::FermataSymbol, SymId> sym_id_to_fermata_sym{
    {mnx::FermataSymbol::Normal, SymId::FERMATA_ABOVE},
    {mnx::FermataSymbol::Angled, SymId::FERMATA_SHORT_ABOVE},
    {mnx::FermataSymbol::Square, SymId::FERMATA_LONG_ABOVE},
    {mnx::FermataSymbol::DoubleAngled, SymId::FERMATA_VERY_SHORT_ABOVE},
    {mnx::FermataSymbol::DoubleSquare, SymId::FERMATA_VERY_LONG_ABOVE},
    {mnx::FermataSymbol::DoubleDot, SymId::FERMATA_LONG_HENZE_ABOVE},
    {mnx::FermataSymbol::HalfCurve, SymId::FERMATA_SHORT_HENZE_ABOVE},
    {mnx::FermataSymbol::Curlew, SymId::CURLEW_SIGN},
};
} // namespace

SymId to_muse_score_fermata_sym_id(const mnx::FermataSymbol fermata_symbol) {
    return muse::value(::mu::iex::mnxio::sym_id_to_fermata_sym, fermata_symbol, SymId::FERMATA_ABOVE);
}

namespace {
const std::unordered_map<FermataType, mnx::FermataDuration> fermata_dura_table{
    {FermataType::NORMAL, mnx::FermataDuration::Normal},        {FermataType::LONG, mnx::FermataDuration::Long},
    {FermataType::LONG_HENZE, mnx::FermataDuration::Long},      {FermataType::SHORT, mnx::FermataDuration::Short},
    {FermataType::SHORT_HENZE, mnx::FermataDuration::Short},    {FermataType::VERY_LONG, mnx::FermataDuration::VeryLong},
    {FermataType::VERY_SHORT, mnx::FermataDuration::VeryShort},
};
} // namespace

mnx::FermataDuration to_mnx_fermata_duration(FermataType fermata_type) {
    return muse::value(::mu::iex::mnxio::fermata_dura_table, fermata_type, mnx::FermataDuration::Auto);
}

namespace {
const std::unordered_map<SymId, mnx::FermataSymbol> fermata_sym_to_sym_id{
    {SymId::FERMATA_ABOVE, mnx::FermataSymbol::Normal},
    {SymId::FERMATA_BELOW, mnx::FermataSymbol::Normal},
    {SymId::FERMATA_SHORT_ABOVE, mnx::FermataSymbol::Angled},
    {SymId::FERMATA_SHORT_BELOW, mnx::FermataSymbol::Angled},
    {
        SymId::FERMATA_LONG_ABOVE,
        mnx::FermataSymbol::Square,
    },
    {
        SymId::FERMATA_LONG_BELOW,
        mnx::FermataSymbol::Square,
    },
    {SymId::FERMATA_VERY_SHORT_ABOVE, mnx::FermataSymbol::DoubleAngled},
    {SymId::FERMATA_VERY_SHORT_BELOW, mnx::FermataSymbol::DoubleAngled},
    {SymId::FERMATA_VERY_LONG_ABOVE, mnx::FermataSymbol::DoubleSquare},
    {SymId::FERMATA_VERY_LONG_BELOW, mnx::FermataSymbol::DoubleSquare},
    {SymId::FERMATA_LONG_HENZE_ABOVE, mnx::FermataSymbol::DoubleDot},
    {SymId::FERMATA_LONG_HENZE_BELOW, mnx::FermataSymbol::DoubleDot},
    {SymId::FERMATA_SHORT_HENZE_ABOVE, mnx::FermataSymbol::HalfCurve},
    {SymId::FERMATA_SHORT_HENZE_BELOW, mnx::FermataSymbol::HalfCurve},
    {SymId::CURLEW_SIGN, mnx::FermataSymbol::Curlew},
};
} // namespace

mnx::FermataSymbol to_mnx_fermata_symbol(SymId sym) {
    return muse::value(::mu::iex::mnxio::fermata_sym_to_sym_id, sym, mnx::FermataSymbol::Normal);
}

namespace {
/// @todo Grow this table as MNX grows it.
const std::unordered_map<mnx::JumpType, JumpType> jump_table = {
    {mnx::JumpType::DsAlFine, JumpType::DS_AL_FINE},
    {mnx::JumpType::Segno, JumpType::DSS},
};
} // namespace

JumpType to_muse_score_jump_type(mnx::JumpType jt) { return muse::value(::mu::iex::mnxio::jump_table, jt, JumpType::USER); }

std::optional<mnx::JumpType> to_mnx_jump_type(JumpType jt) {
    return muse::key(::mu::iex::mnxio::jump_table, jt, std::optional<mnx::JumpType>{});
}

namespace {
const std::unordered_map<mnx::LyricLineType, LyricsSyllabic> line_type_table = {
    {mnx::LyricLineType::Whole, LyricsSyllabic::SINGLE},
    {mnx::LyricLineType::Start, LyricsSyllabic::BEGIN},
    {mnx::LyricLineType::Middle, LyricsSyllabic::MIDDLE},
    {mnx::LyricLineType::End, LyricsSyllabic::END},
};
} // namespace

LyricsSyllabic to_muse_score_lyrics_syllabic(mnx::LyricLineType llt) {
    return muse::value(::mu::iex::mnxio::line_type_table, llt, LyricsSyllabic::SINGLE);
}

mnx::LyricLineType to_mnx_lyric_line_type(LyricsSyllabic ls) {
    return muse::key(::mu::iex::mnxio::line_type_table, ls, mnx::LyricLineType::Whole);
}

namespace {
const std::unordered_map<mnx::OttavaAmount, OttavaType> ottava_type_table = {
    {mnx::OttavaAmount::OctaveDown, OttavaType::OTTAVA_8VB},        {mnx::OttavaAmount::OctaveUp, OttavaType::OTTAVA_8VA},
    {mnx::OttavaAmount::TwoOctavesDown, OttavaType::OTTAVA_15MB},   {mnx::OttavaAmount::TwoOctavesUp, OttavaType::OTTAVA_15MA},
    {mnx::OttavaAmount::ThreeOctavesDown, OttavaType::OTTAVA_22MB}, {mnx::OttavaAmount::ThreeOctavesUp, OttavaType::OTTAVA_22MA},
};
} // namespace

OttavaType to_muse_score_ottava_type(mnx::OttavaAmount ottava_amount) {
    return muse::value(::mu::iex::mnxio::ottava_type_table, ottava_amount, OttavaType::OTTAVA_8VA);
}

std::optional<mnx::OttavaAmount> to_mnx_ottava_amount(OttavaType ottava_type) {
    return muse::key(::mu::iex::mnxio::ottava_type_table, ottava_type, std::optional<mnx::OttavaAmount>{});
}

Fraction to_muse_score_r_tick(const mnx::RhythmicPosition &position) { return to_muse_score_fraction(position.fraction()); }

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
const std::unordered_map<mnx::LineType, SlurStyleType> slur_style_table = {
    {mnx::LineType::Dashed, SlurStyleType::DASHED},
    {mnx::LineType::Dotted, SlurStyleType::DOTTED},
    {mnx::LineType::Solid, SlurStyleType::SOLID},
};
} // namespace

SlurStyleType to_muse_score_slur_style_type(mnx::LineType line_type) {
    return muse::value(::mu::iex::mnxio::slur_style_table, line_type, SlurStyleType::SOLID);
}

mnx::LineType to_mnx_slur_line_type(SlurStyleType sst) { return muse::key(::mu::iex::mnxio::slur_style_table, sst, mnx::LineType::Solid); }

namespace {
const std::unordered_map<mnx::AutoYesNo, TupletBracketType> tuplet_bracket_type_table = {
    {mnx::AutoYesNo::Auto, TupletBracketType::AUTO_BRACKET},
    {mnx::AutoYesNo::Yes, TupletBracketType::SHOW_BRACKET},
    {mnx::AutoYesNo::No, TupletBracketType::SHOW_NO_BRACKET},
};
} // namespace

TupletBracketType to_muse_score_tuplet_bracket_type(mnx::AutoYesNo bracket_option) {
    return muse::value(::mu::iex::mnxio::tuplet_bracket_type_table, bracket_option, TupletBracketType::AUTO_BRACKET);
}

mnx::AutoYesNo to_mnx_tuplet_bracket_type(TupletBracketType bracket_option) {
    return muse::key(::mu::iex::mnxio::tuplet_bracket_type_table, bracket_option, mnx::AutoYesNo::Auto);
}

namespace {
const std::unordered_map<mnx::TupletDisplaySetting, TupletNumberType> tuplet_number_type_table = {
    {mnx::TupletDisplaySetting::Inner, TupletNumberType::SHOW_NUMBER},
    {mnx::TupletDisplaySetting::NoNumber, TupletNumberType::NO_TEXT},
    {mnx::TupletDisplaySetting::Both, TupletNumberType::SHOW_RELATION},
};
} // namespace

TupletNumberType to_muse_score_tuplet_number_type(mnx::TupletDisplaySetting number_style) {
    return muse::value(::mu::iex::mnxio::tuplet_number_type_table, number_style, TupletNumberType::SHOW_NUMBER);
}

mnx::TupletDisplaySetting to_mnx_tuplet_number_type(TupletNumberType number_style) {
    return muse::key(::mu::iex::mnxio::tuplet_number_type_table, number_style, mnx::TupletDisplaySetting::Inner);
}

NoteVal to_muse_score_note_val(const mnx::sequence::Pitch::Required &pitch, Key key, int octave_shift) {
    int step = static_cast<int>(pitch.step);
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

ClefType to_muse_score_clef_type(const mnx::part::Clef &mnx_clef) {
    using ClefSign = mnx::ClefSign;
    using OttavaAmountOrZero = mnx::OttavaAmountOrZero;

    const auto snap_to_line = [](int staff_position_half_spaces) -> int {
        // clefTable only supports clefs centered on lines (even half-spaces).
        if (staff_position_half_spaces & 1) {
            staff_position_half_spaces += (staff_position_half_spaces > 0 ? -1 : +1); // snap toward 0
        }
        return staff_position_half_spaces;
    };

    const int sp = snap_to_line(mnx_clef.staffPosition());
    const auto sign = mnx_clef.sign();
    const auto octave = mnx_clef.octave();

    switch (sign) {
        case ClefSign::GClef:
            // G clef: standard at sp == -2, also supports G_1 at sp == -4
            if (sp == -4) {
                return ClefType::G_1; // best-effort: ignore octave for this placement
            }

            switch (octave) {
                case OttavaAmountOrZero::NoTransposition: return ClefType::G;
                case OttavaAmountOrZero::TwoOctavesDown: return ClefType::G15_MB;
                case OttavaAmountOrZero::OctaveDown: return ClefType::G8_VB;
                case OttavaAmountOrZero::OctaveUp: return ClefType::G8_VA;
                case OttavaAmountOrZero::TwoOctavesUp: return ClefType::G15_MA;
                default: return ClefType::INVALID;
            }

        case ClefSign::FClef:
            // F clef: supports sp == 0 (F_B line 3), sp == +2 (F line 4), sp == +4 (F_C line 5)
            if (sp == 0) {
                return ClefType::F_B; // best-effort: ignore octave for this placement
            }
            if (sp == +4) {
                return ClefType::F_C; // best-effort: ignore octave for this placement
            }

            switch (octave) {
                case OttavaAmountOrZero::NoTransposition: return ClefType::F;
                case OttavaAmountOrZero::TwoOctavesDown: return ClefType::F15_MB;
                case OttavaAmountOrZero::OctaveDown: return ClefType::F8_VB;
                case OttavaAmountOrZero::OctaveUp: return ClefType::F_8VA;
                case OttavaAmountOrZero::TwoOctavesUp: return ClefType::F_15MA;
                default: return ClefType::INVALID;
            }

        case ClefSign::CClef:
            // Special-case explicit octave variant in your clefTable
            if (sp == +2 && octave == OttavaAmountOrZero::OctaveDown) {
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

Fraction to_muse_score_fraction(const mnx::FractionValue &fraction) { return Fraction(fraction.numerator(), fraction.denominator()); }

mnx::FractionValue to_mnx_fraction_value(const Fraction &fraction) {
    return mnx::FractionValue(fraction.numerator(), fraction.denominator());
}

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

PlacementV to_muse_score_placement_v(const mnx::Orientation orient, const EngravingItem *item) {
    switch (orient) {
        case mnx::Orientation::Above: return PlacementV::ABOVE;
        case mnx::Orientation::Below: return PlacementV::BELOW;
        case mnx::Orientation::Auto: IF_ASSERT_FAILED(item) { break;
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
