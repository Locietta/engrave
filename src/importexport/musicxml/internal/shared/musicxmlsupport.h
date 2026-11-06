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

#include "framework/global/types/string.h"
#include "serialization/xmlstreamreader.h"

namespace mu::engraving {
enum class AccidentalType : unsigned char;
class Articulation;
class Chord;
struct ChordDescription;
class Fraction;
class Harmony;
class HarmonyInfo;
class HDegree;
class Score;
enum class SymId;
enum class Key : signed char;
} // namespace mu::engraving

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   MusicXmlSupport -- MusicXML import support functions
//---------------------------------------------------------

class MusicXmlSupport {
public:
    static int string_to_int(const muse::String &s, bool *ok);
    static engraving::Fraction note_type_to_fraction(const muse::String &type);
    static engraving::Fraction calculate_fraction(const muse::String &type, int dots, int normal_notes, int actual_notes);
};

extern muse::String acc_sym_id2_music_xml_string(const engraving::SymId id);
extern muse::String acc_sym_id2_smufl_music_xml_string(const engraving::SymId id);
extern muse::String accidental_type2_music_xml_string(const engraving::AccidentalType type);
extern muse::String accidental_type2_smufl_music_xml_string(const engraving::AccidentalType type);
extern engraving::AccidentalType music_xml_string2accidental_type(const muse::String mxml_name, const muse::String smufl);
extern muse::String music_xml_accidental_text_to_char(const muse::String mxml_name);
extern engraving::SymId music_xml_string2acc_sym_id(const muse::String mxml_name, const muse::String smufl = {});
extern engraving::AccidentalType microtonal_guess(double val);
extern bool is_laissez_vibrer(const engraving::SymId id);
extern muse::String error_string_with_location(int64_t byte_offset, const muse::String &error);
extern muse::String check_at_end_element(const muse::XmlStreamReader &e, const muse::String &exp_name);

extern muse::String harmony_xml_function(const engraving::HarmonyInfo *info, const engraving::Harmony *h);
extern muse::String harmony_xml_function(const engraving::HarmonyInfo *info, const engraving::Harmony *h, engraving::Key k);
extern muse::String harmony_xml_kind(const engraving::HarmonyInfo *h);
extern muse::String harmony_xml_text(const engraving::HarmonyInfo *h);
extern muse::String harmony_xml_symbols(const engraving::HarmonyInfo *h);
extern muse::String harmony_xml_parens(const engraving::HarmonyInfo *h);
extern muse::StringList harmony_xml_degrees(const engraving::HarmonyInfo *h);
extern const engraving::ChordDescription *harmony_from_xml(engraving::HarmonyInfo *info, engraving::Score *score, const muse::String &kind,
                                                           const muse::String &kind_text, const muse::String &symbols,
                                                           const muse::String &parens, const std::vector<engraving::HDegree> &dl);
extern void set_harmony_root_tpc_from_function(engraving::HarmonyInfo *info, const engraving::Harmony *h, const muse::String &s,
                                               engraving::Key key);
} // namespace mu::iex::musicxml
