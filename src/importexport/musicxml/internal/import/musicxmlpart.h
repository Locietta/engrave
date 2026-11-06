/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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

#include "../shared/musicxmltypes.h"
#include "musicxmlvoicedesc.h"

namespace mu::iex::musicxml {
class MusicXmlOctaveShiftList : public std::map<engraving::Fraction, int> {
public:
    MusicXmlOctaveShiftList() {}

    int octave_shift(const engraving::Fraction f) const;
    void add_octave_shift(const int shift, const engraving::Fraction f);
    void calc_octave_shift_shifts();
};

class MusicXmlPart {
public:
    MusicXmlPart(muse::String id = {}, muse::String name = {});
    void add_measure_number_and_duration(muse::String measure_number, engraving::Fraction measure_duration);

    muse::String get_id() const { return id_; }

    muse::String to_string() const;
    VoiceList voicelist; // the voice map information TODO: make private
    engraving::Fraction measure_duration(size_t i) const;

    size_t n_measures() const { return measure_durations_.size(); }

    MusicXmlInstrList instr_list;   // TODO: make private
    MusicXmlIntervalList intervals; ///< Transpositions
    engraving::Interval interval(const engraving::Fraction f) const;
    int octave_shift(const engraving::staff_idx_t staff, const engraving::Fraction f) const;
    void add_octave_shift(const engraving::staff_idx_t staff, const int shift, const engraving::Fraction f);
    void calc_octave_shifts();

    void set_name(muse::String nm) { name_ = nm; }

    muse::String get_name() const { return name_; }

    void set_print_name(const bool b) { print_name_ = b; }

    bool get_print_name() const { return print_name_; }

    void set_abbr(muse::String ab) { abbr_ = ab; }

    muse::String get_abbr() const { return abbr_; }

    void set_print_abbr(const bool b) { print_abbr_ = b; }

    bool get_print_abbr() const { return print_abbr_; }

    std::map<int, int> staff_number_to_index() const { return staff_number_to_index_; }

    int staff_number_to_index(const int staff_number) const;

    void insert_staff_number_to_index(const int staff_number, const int staff_index) {
        staff_number_to_index_.insert({staff_number, staff_index});
    }

    LyricNumberHandler &lyric_number_handler() { return lyric_number_handler_; }

    const LyricNumberHandler &lyric_number_handler() const { return lyric_number_handler_; }

    void set_max_staff(const int staff);

    int max_staff() const { return max_staff_; }

    bool is_vocal_staff() const;
    bool is_percussion_staff() const;

    void has_lyrics(bool b) { has_lyrics_ = b; }
private:
    muse::String id_;
    muse::String name_;
    bool print_name_ = true;
    muse::String abbr_;
    bool print_abbr_ = true;
    std::vector<muse::String> measure_numbers_;          // MusicXML measure number attribute
    std::vector<engraving::Fraction> measure_durations_; // duration in fraction for every measure
    std::vector<MusicXmlOctaveShiftList> octave_shifts_; // octave shift list for every staff
    LyricNumberHandler lyric_number_handler_;
    int max_staff_ = -1; // maximum staff value found (0 based), -1 = none
    bool has_lyrics_ = false;
    std::map<int, int> staff_number_to_index_; // Mapping from staff number to index in staff list.
    // Only for when staves are discarded in MusicXmlParserPass1::attributes.
};
} // namespace mu::iex::musicxml
