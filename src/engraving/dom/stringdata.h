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

#include <cstddef>
#include <vector>
#include <map>

#include "../types/fraction.h"
#include "../types/types.h"

namespace mu::engraving {
struct Chord;
struct Note;

struct Staff;

//---------------------------------------------------------
//   StringData
//---------------------------------------------------------

// defines the string of an instrument
struct InstrString {
    InstrString(int p = 0, bool o = false, int s = 0) : pitch(p), open(o), start_fret(s) {}

    int pitch = 0;      // the pitch of the string
    bool open = false;  // true: string is open | false: string is fretted
    int start_fret = 0; // banjo 5th string starts on 5th fret
    bool use_flat = false;

    bool operator==(const InstrString &d) const { return d.pitch == pitch && d.open == open; }
};

struct StringData {
    StringData() {}

    StringData(int num_frets, int num_strings, int strings[], bool use_flats = false);
    StringData(int num_frets, std::vector<InstrString> &strings);

    bool is_null() const;

    void set(const StringData &src);
    bool convert_pitch(int pitch, int pitch_offset, int *string, int *fret, const CapoParams &capo = {}) const;
    bool convert_pitch(int pitch, const Staff *staff, int *string, int *fret) const;
    bool convert_pitch(int pitch, const Staff *staff, const Fraction &tick, int *string, int *fret) const;
    int fret(int pitch, int string, const Staff *staff) const;
    int fret(int pitch, int string, const Staff *staff, const Fraction &tick) const;
    void fret_chords(Chord *chord) const;
    int get_pitch(int string, int fret, int pitch_offset) const;
    int get_pitch(int string, int fret, const Staff *staff) const;
    int get_pitch(int string, int fret, const Staff *staff, const Fraction &tick) const;
    static int pitch_offset_at(const Staff *staff);
    static int pitch_offset_at(const Staff *staff, const Fraction &tick);
    static int pitch_offset_at(const Staff *staff, const Fraction &tick, int string);

    size_t strings() const { return string_table_.size(); }

    int fretted_strings() const;

    const std::vector<InstrString> &string_list() const { return string_table_; }

    std::vector<InstrString> &string_list() { return string_table_; }

    int frets() const { return frets_; }

    void set_frets(int val) { frets_ = val; }

    bool operator==(const StringData &d) const { return d.frets_ == frets_ && d.string_table_ == string_table_; }

    void config_banjo5th_string();
    int adjust_banjo5th_fret(int fret) const;
    bool is_five_string_banjo() const;

    bool use_flats() const { return use_flats_; }

private:

    int fret(int pitch, int string, int pitch_offset) const;
    void sort_chord_notes(std::map<int, Note *> &sorted_notes, const Chord *chord, int *count) const;
    void sort_chord_notes_use_same_string(const Chord *chord) const;
    bool has_pending_pitch_change(const Chord *chord) const;
    void update_frets_on_same_strings(const Chord *chord) const;
    void prefer_bass_string_for_negative_fret(const Chord *chord) const;
    void reassign_negative_fret_notes(const Chord *chord) const;
    bool try_resolve_string_conflict_with_out_of_range_fret(const Note *note, int num_strings, std::vector<int> &b_used, int &n_new_string,
                                                            int &n_new_fret) const;

    //      std::vector<int>  stringTable { 40, 45, 50, 55, 59, 64 };   // guitar is default
    //      int         _frets = 19;
    std::vector<InstrString> string_table_; // no strings by default
    int frets_ = 0;

    static bool b_fretting;
    bool use_flats_ = false;
};
} // namespace mu::engraving
