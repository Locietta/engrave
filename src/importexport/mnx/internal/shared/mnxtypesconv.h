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

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <mnxdom/mnxdom.h>

#include "engraving/dom/chord.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/types/types.h"

namespace mu::engraving {
struct Interval;
struct NoteVal;
struct TDuration;
} // namespace mu::engraving

namespace mu::iex::mnxio {
/// How a MuseScore DynamicType is expressed in MNX.
///
/// MNX spells an accent out as accentPrefix + value + accentSuffix + residualValue, so a
/// single MuseScore dynamic type may need up to four MNX properties to describe it.
struct MnxDynamicMapping {
    std::optional<mnx::DynamicValue> value;                      ///< the dynamic, or the initial attack of an accent
    std::optional<mnx::DynamicValue> residual_value;             ///< the value remaining after an accent's attack (accents only)
    bool is_accent = false;                                      ///< whether MNX models this as a `DynamicAccent`
    mnx::DynamicPrefix accent_prefix = mnx::DynamicPrefix::None; ///< meaningful only when #isAccent
    mnx::DynamicSuffix accent_suffix = mnx::DynamicSuffix::None; ///< meaningful only when #isAccent
};

// MNX values -> MuseScore values
extern engraving::ArticulationAnchor to_muse_score_articulation_anchor(mnx::Orientation orient);
extern engraving::BarLineType to_muse_score_bar_line_type(mnx::BarlineType blt);
extern engraving::BeamMode to_muse_score_beam_mode(int lowest_beam_start);
extern engraving::BracketType to_muse_score_bracket_type(mnx::LayoutSymbol lys);
extern engraving::SymId to_muse_score_breath_mark_sym(std::optional<mnx::BreathMarkSymbol> br_sym);
extern engraving::ClefType to_muse_score_clef_type(const mnx::part::Clef &mnx_clef);
extern std::vector<std::string> to_muse_score_dynamic_glyph_names(mnx::DynamicValue value);
extern std::string to_muse_score_dynamic_glyph_name(mnx::DynamicPrefix prefix);
extern std::string to_muse_score_dynamic_glyph_name(mnx::DynamicSuffix suffix);
extern engraving::DurationType to_muse_score_duration_type(mnx::NoteValueBase nvb);
extern engraving::TDuration to_muse_score_duration(mnx::NoteValue nv);
extern engraving::SymId to_muse_score_fermata_sym_id(const mnx::FermataSymbol fermata_symbol);
extern engraving::Fraction to_muse_score_fraction(const mnx::FractionValue &fraction);
extern engraving::JumpType to_muse_score_jump_type(mnx::JumpType jt);
extern engraving::Key to_muse_score_key(int fifths);
extern engraving::LyricsSyllabic to_muse_score_lyrics_syllabic(mnx::LyricLineType llt);
extern engraving::NoteVal to_muse_score_note_val(const mnx::sequence::Pitch::Required &pitch, engraving::Key key, int octave_shift);
extern engraving::OttavaType to_muse_score_ottava_type(mnx::OttavaAmount ottava_amount);
extern engraving::PreferSharpFlat to_muse_score_prefer_sharp_flat(int key_fifths_flip_at);
extern engraving::PlacementV to_muse_score_placement_v(const mnx::Orientation orient, const engraving::EngravingItem *item);
extern engraving::Fraction to_muse_score_r_tick(const mnx::RhythmicPosition &position);
extern engraving::SlurStyleType to_muse_score_slur_style_type(mnx::LineType line_type);
extern engraving::TimeSigType to_muse_score_time_sig_type(std::optional<mnx::TimeSignatureDisplay> display);
extern engraving::TremoloType to_muse_score_tremolo_type(int number_of_beams);
extern engraving::TupletBracketType to_muse_score_tuplet_bracket_type(mnx::AutoYesNo bracket_option);
extern engraving::TupletNumberType to_muse_score_tuplet_number_type(mnx::TupletDisplaySetting number_style);

// MuseScore values -> MNX values
extern mnx::BarlineType to_mnx_bar_line_type(engraving::BarLineType blt);
extern std::optional<mnx::BreathMarkSymbol> to_mnx_breath_mark_sym(engraving::SymId sym);
extern std::optional<mnx::part::Clef::Required> to_mnx_clef(engraving::ClefType clef_type);
extern MnxDynamicMapping to_mnx_dynamic_type(engraving::DynamicType type);
/// Recovers an MNX dynamic from the letters that spell it, for dynamics MuseScore renders but
/// has no DynamicType for. Takes arbitrary text rather than pre-validated letters, and returns
/// std::nullopt unless the whole of it is a dynamic spelling MNX can describe.
extern std::optional<MnxDynamicMapping> to_mnx_dynamic_from_letters(const std::string &dynamic_letters);
/// #toMnxDynamicFromLetters for text set in a music font. Decomposes each glyph into the letters
/// it spells, so ligatures such as dynamicFFF are read as "fff". Returns std::nullopt if any
/// glyph is not one MuseScore spells a dynamic with.
extern std::optional<MnxDynamicMapping> to_mnx_dynamic_from_sym_ids(const std::vector<engraving::SymId> &sym_ids);
/// The inverse of #toMnxDynamicType. Returns std::nullopt when MuseScore has no type that
/// means exactly what the mapping describes, in which case the dynamic is unclassifiable
/// and belongs in DynamicType::OTHER.
extern std::optional<engraving::DynamicType> to_muse_score_dynamic_type(const MnxDynamicMapping &mapping);
extern mnx::FermataDuration to_mnx_fermata_duration(engraving::FermataType fermata_type);
extern mnx::FermataSymbol to_mnx_fermata_symbol(engraving::SymId sym);
extern mnx::FractionValue to_mnx_fraction_value(const engraving::Fraction &fraction);
extern std::optional<mnx::JumpType> to_mnx_jump_type(engraving::JumpType jt);
extern int to_mnx_key_fifths_flip_value(engraving::PreferSharpFlat prefer, const engraving::Interval &key_transposition);
extern mnx::LayoutSymbol to_mnx_layout_symbol(engraving::BracketType bracket_type);
extern mnx::LyricLineType to_mnx_lyric_line_type(engraving::LyricsSyllabic ls);
extern mnx::Orientation to_mnx_orientation(engraving::ArticulationAnchor anchor);
extern mnx::Orientation to_mnx_orientation(engraving::PlacementV placement);
extern mnx::LineType to_mnx_slur_line_type(engraving::SlurStyleType sst);
extern std::optional<mnx::NoteValue::Required> to_mnx_note_value(const engraving::TDuration &duration);
extern std::optional<mnx::OttavaAmount> to_mnx_ottava_amount(engraving::OttavaType ottava_type);
extern std::optional<mnx::sequence::Pitch::Required> to_mnx_pitch(const engraving::Note *note);
extern std::optional<mnx::TimeSignatureDisplay> to_mnx_time_signature_display(engraving::TimeSigType type);
extern std::optional<mnx::TimeSignatureUnit> to_mnx_time_signature_unit(int denominator);
extern std::optional<int> to_mnx_tremolo_marks(engraving::TremoloType tt);
extern mnx::AutoYesNo to_mnx_tuplet_bracket_type(engraving::TupletBracketType bracket_option);
extern mnx::TupletDisplaySetting to_mnx_tuplet_number_type(engraving::TupletNumberType number_style);

// MuseScore -> MuseScore
extern engraving::NoteType dura_type_to_grace_note_type(engraving::DurationType type, bool use_left);

// utilities
extern std::string make_mnx_voice_id_from_track(int mnx_part_staff_num, engraving::track_idx_t cur_track_idx);
} // namespace mu::iex::mnxio
