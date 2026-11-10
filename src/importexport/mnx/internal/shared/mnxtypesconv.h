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

#include "engraving/dom/chord.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/types/types.h"
#include "model/layout.h"
#include "model/mnx_model.h"
#include "model/music.h"
#include "model/rational.h"

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
    std::optional<model::DynamicValue> value;                        ///< the dynamic, or the initial attack of an accent
    std::optional<model::DynamicValue> residual_value;               ///< the value remaining after an accent's attack (accents only)
    bool is_accent = false;                                          ///< whether MNX models this as a `DynamicAccent`
    model::DynamicPrefix accent_prefix = model::DynamicPrefix::NONE; ///< meaningful only when #isAccent
    model::DynamicSuffix accent_suffix = model::DynamicSuffix::NONE; ///< meaningful only when #isAccent
};

// MNX values -> MuseScore values
extern engraving::ArticulationAnchor to_muse_score_articulation_anchor(model::Orientation orient);
extern engraving::BarLineType to_muse_score_bar_line_type(model::BarlineType blt);
extern engraving::BeamMode to_muse_score_beam_mode(int lowest_beam_start);
extern engraving::BracketType to_muse_score_bracket_type(model::StaffSymbol lys);
extern engraving::SymId to_muse_score_breath_mark_sym(const std::optional<std::string> &br_sym);
extern engraving::ClefType to_muse_score_clef_type(const model::Clef &mnx_clef);
extern std::vector<std::string> to_muse_score_dynamic_glyph_names(model::DynamicValue value);
extern std::string to_muse_score_dynamic_glyph_name(model::DynamicPrefix prefix);
extern std::string to_muse_score_dynamic_glyph_name(model::DynamicSuffix suffix);
extern engraving::DurationType to_muse_score_duration_type(model::NoteValueBase nvb);
extern engraving::TDuration to_muse_score_duration(const model::NoteValue &nv);
extern engraving::SymId to_muse_score_fermata_sym_id(const model::FermataSymbol fermata_symbol);
extern engraving::Fraction to_muse_score_fraction(const model::Rational &fraction);
extern engraving::JumpType to_muse_score_jump_type(model::JumpType jt);
extern engraving::Key to_muse_score_key(int fifths);
extern engraving::LyricsSyllabic to_muse_score_lyrics_syllabic(model::EventLyricLineType llt);
extern engraving::NoteVal to_muse_score_note_val(const model::SpelledPitch &pitch, engraving::Key key, int octave_shift);
extern engraving::OttavaType to_muse_score_ottava_type(model::OttavaAmount ottava_amount);
extern engraving::PreferSharpFlat to_muse_score_prefer_sharp_flat(int key_fifths_flip_at);
extern engraving::PlacementV to_muse_score_placement_v(const model::Orientation orient, const engraving::EngravingItem *item);
extern engraving::Fraction to_muse_score_r_tick(const model::RhythmicPosition &position);
extern engraving::SlurStyleType to_muse_score_slur_style_type(model::LineType line_type);
extern engraving::TimeSigType to_muse_score_time_sig_type(std::optional<model::TimeSignatureDisplay> display);
extern engraving::TremoloType to_muse_score_tremolo_type(int number_of_beams);
extern engraving::TupletBracketType to_muse_score_tuplet_bracket_type(model::YesNoAuto bracket_option);
extern engraving::TupletNumberType to_muse_score_tuplet_number_type(model::TupletDisplaySetting number_style);

// MuseScore values -> MNX values
extern model::BarlineType to_mnx_bar_line_type(engraving::BarLineType blt);
extern std::optional<std::string> to_mnx_breath_mark_sym(engraving::SymId sym);
extern std::optional<model::Clef> to_mnx_clef(engraving::ClefType clef_type);
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
extern model::FermataDuration to_mnx_fermata_duration(engraving::FermataType fermata_type);
extern model::FermataSymbol to_mnx_fermata_symbol(engraving::SymId sym);
extern model::Fraction to_mnx_fraction(const engraving::Fraction &fraction);
extern std::optional<model::JumpType> to_mnx_jump_type(engraving::JumpType jt);
extern int to_mnx_key_fifths_flip_value(engraving::PreferSharpFlat prefer, const engraving::Interval &key_transposition);
extern model::StaffSymbol to_mnx_layout_symbol(engraving::BracketType bracket_type);
extern model::EventLyricLineType to_mnx_lyric_line_type(engraving::LyricsSyllabic ls);
extern model::Orientation to_mnx_orientation(engraving::ArticulationAnchor anchor);
extern model::Orientation to_mnx_orientation(engraving::PlacementV placement);
extern model::LineType to_mnx_slur_line_type(engraving::SlurStyleType sst);
extern std::optional<model::NoteValue> to_mnx_note_value(const engraving::TDuration &duration);
extern std::optional<model::OttavaAmount> to_mnx_ottava_amount(engraving::OttavaType ottava_type);
extern std::optional<model::Pitch> to_mnx_pitch(const engraving::Note *note);
extern std::optional<model::TimeSignatureDisplay> to_mnx_time_signature_display(engraving::TimeSigType type);
extern std::optional<model::TimeSignatureUnit> to_mnx_time_signature_unit(int denominator);
extern std::optional<int> to_mnx_tremolo_marks(engraving::TremoloType tt);
extern model::YesNoAuto to_mnx_tuplet_bracket_type(engraving::TupletBracketType bracket_option);
extern model::TupletDisplaySetting to_mnx_tuplet_number_type(engraving::TupletNumberType number_style);

// MuseScore -> MuseScore
extern engraving::NoteType dura_type_to_grace_note_type(engraving::DurationType type, bool use_left);

// utilities
extern std::string make_mnx_voice_id_from_track(int mnx_part_staff_num, engraving::track_idx_t cur_track_idx);
} // namespace mu::iex::mnxio
