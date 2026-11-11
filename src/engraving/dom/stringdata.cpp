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

#include "stringdata.h"

#include <map>

#include "chord.h"
#include "defer.h"
#include "engraving/internal/engravingconfiguration.h" // IWYU pragma: keep
#include "note.h"
#include "part.h"
#include "segment.h"
#include "staff.h"
#include "types/types.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   StringData
//---------------------------------------------------------

bool StringData::b_fretting = false;

StringData::StringData(int num_frets, int num_strings, int strings[], bool use_flats) {
    InstrString strg = {0, false, 0};
    frets_ = num_frets;

    const int safe_num_strings = std::max(0, num_strings);
    string_table_.reserve(static_cast<size_t>(safe_num_strings));
    for (int i = 0; i < safe_num_strings; i++) {
        strg.pitch = strings[i];
        strg.use_flat = use_flats;
        string_table_.push_back(strg);
    }
    use_flats_ = use_flats;
}

StringData::StringData(int num_frets, std::vector<InstrString> &strings) {
    frets_ = num_frets;

    string_table_.clear();
    string_table_.reserve(strings.size());
    for (const InstrString &i : strings) {
        string_table_.push_back(i);
    }
}

bool StringData::is_null() const { return *this == StringData(); }

// called from import (musicxml/guitarpro/...)
void StringData::set(const StringData &src) {
    *this = src;
    if (is_five_string_banjo()) {
        config_banjo5th_string();
    }
}

//---------------------------------------------------------
//   convertPitch
//   Finds string and fret for a note.
//
//   Fills *string and *fret with suitable values for pitch at given staff,
//   using the highest possible string.
//   If note cannot be fretted, uses fret 0 on nearest string and returns false
//
//    Note: Strings are stored internally from lowest (0) to highest (strings()-1),
//          but the returned *string value references strings in reversed, 'visual', order:
//          from highest (0) to lowest (strings()-1)
//---------------------------------------------------------

bool StringData::convert_pitch(int pitch, const Staff *staff, int *string, int *fret) const {
    return convert_pitch(pitch, pitch_offset_at(staff), string, fret);
}

//---------------------------------------------------------
//   convertPitch
//   Finds string and fret for a note.
//
//   Fills *string and *fret with suitable values for pitch at given tick of given staff,
//   using the highest possible string.
//   If note cannot be fretted, uses fret 0 on nearest string and returns false
//
//    Note: Strings are stored internally from lowest (0) to highest (strings()-1),
//          but the returned *string value references strings in reversed, 'visual', order:
//          from highest (0) to lowest (strings()-1)
//---------------------------------------------------------

bool StringData::convert_pitch(int pitch, const Staff *staff, const Fraction &tick, int *string, int *fret) const {
    return convert_pitch(pitch, pitch_offset_at(staff, tick), string, fret);
}

//---------------------------------------------------------
//   getPitch
//    Returns the pitch corresponding to the string / fret combination
//    at given staff.
//    Returns INVALID_PITCH if not possible
//    Note: frets above max fret are accepted.
//---------------------------------------------------------

int StringData::get_pitch(int string, int fret, const Staff *staff) const { return get_pitch(string, fret, pitch_offset_at(staff)); }

//---------------------------------------------------------
//   getPitch
//    Returns the pitch corresponding to the string / fret combination
//    at given tick of given staff.
//    Returns INVALID_PITCH if not possible
//    Note: frets above max fret are accepted.
//---------------------------------------------------------

int StringData::get_pitch(int string, int fret, const Staff *staff, const Fraction &tick) const {
    const CapoParams &capo = staff->capo(tick);
    bool ignored_string = capo.active && muse::contains(capo.ignored_strings, (string_idx_t) string);
    return ignored_string ? get_pitch(string, fret, pitch_offset_at(staff)) : get_pitch(string, fret, pitch_offset_at(staff, tick));
}

//---------------------------------------------------------
//   fret
//    Returns the fret corresponding to the pitch / string combination
//    at given staff.
//    Returns INVALID_FRET_INDEX if not possible
//---------------------------------------------------------

int StringData::fret(int pitch, int string, const Staff *staff) const { return fret(pitch, string, pitch_offset_at(staff)); }

int StringData::fret(int pitch, int string, const Staff *staff, const Fraction &tick) const {
    return fret(pitch, string, pitch_offset_at(staff, tick));
}

//---------------------------------------------------------
//   fretChords
//    Assigns fretting to all the notes of each chord in the same segment of chord
//    re-using existing fretting wherever possible
//
//    Minimizes fret conflicts (multiple notes on the same string)
//    but marks as fretConflict notes which cannot be fretted
//    (outside tablature range) or which cannot be assigned
//    a separate string
//---------------------------------------------------------

void StringData::fret_chords(Chord *chord) const {
    if (b_fretting) {
        return;
    }
    b_fretting = true;
    DEFER { b_fretting = false; };

    int strings = static_cast<int>(this->strings());
    const bool skip_dead_notes = chord->configuration()->keep_dead_notes_unchanged_on_transpose();

    // we need the notes sorted in order of string (from highest to lowest) and then pitch
    std::map<int, Note *> sorted_notes;
    int count = 0;
    // if chord parent is not a segment, the chord is special (usually a grace chord):
    // fret it by itself, ignoring the segment
    if (!chord->ownership_parent()->is_segment()) {
        sort_chord_notes(sorted_notes, chord, &count);
    } else {
        // scan each chord of seg from same staff as 'chord', inserting each of its notes in sortedNotes
        Segment *seg = chord->segment();
        track_idx_t trk;
        track_idx_t trk_from = (chord->track() / ::mu::engraving::voices) * ::mu::engraving::voices;
        track_idx_t trk_to = trk_from + ::mu::engraving::voices;
        for (trk = trk_from; trk < trk_to; ++trk) {
            EngravingItem *ch = seg->elist().at(trk);
            if (ch && ch->is_chord()) {
                sort_chord_notes(sorted_notes, to_chord(ch), &count);
            }
        }
    }

    for (const auto &p : sorted_notes) {
        Note *note = p.second;
        if (note->string() >= strings) {
            note->undo_change_property(Pid::STRING, ::mu::engraving::invalid_string_index);
            note->undo_change_property(Pid::FRET, ::mu::engraving::invalid_fret_index);
        }
    }

    if (!strings) {
        return;
    }

    // we need to keep track of string allocation
    std::vector<int> b_used(strings);

    // determine used range of frets
    int min_fret = INT32_MAX;
    int max_fret = INT32_MIN;
    for (const auto &p : sorted_notes) {
        Note *note = p.second;
        if (note->string() != ::mu::engraving::invalid_string_index && note->display_fret() == Note::DisplayFretOption::NO_HARMONIC) {
            b_used[note->string()]++;
        }
        if (note->fret() != ::mu::engraving::invalid_fret_index && note->fret() < min_fret) {
            min_fret = note->fret();
        }
        if (note->fret() != ::mu::engraving::invalid_fret_index && note->fret() > max_fret) {
            max_fret = note->fret();
        }
    }

    // scan chord notes from highest, matching with strings from the highest
    for (const auto &p : sorted_notes) {
        Note *note = p.second;
        int n_string, n_new_string;
        int n_fret, n_new_fret, n_temp_fret;

        n_string = n_new_string = note->string();
        n_fret = n_new_fret = note->fret();
        note->set_fret_conflict(false); // assume no conflicts on this note
        if (skip_dead_notes && note->dead_note()) {
            continue;
        }
        // if no fretting (any invalid fretting has been erased by sortChordNotes() )
        if (n_string ==
            ::mu::engraving::invalid_string_index /*|| nFret == INVALID_FRET_INDEX || getPitch(nString, nFret) != note->pitch()*/) {
            const CapoParams &capo = note->staff()->capo(note->tick());
            // get a new fretting
            if (convert_pitch(note->pitch(), pitch_offset_at(chord->staff(), chord->tick()), &n_new_string, &n_new_fret, capo) &&
                note->display_fret() == Note::DisplayFretOption::NO_HARMONIC && !note->negative_fret_used()) {
                // Check if this string should be ignored in capo params and updated fret
                n_new_fret = fret(note->pitch(), n_new_string, pitch_offset_at(chord->staff(), chord->tick(), n_new_string));
                // note can be fretted: use string
                b_used[n_new_string]++;
            } else {
                // no way to fit this note in this tab:
                // mark as fretting conflict
                note->set_fret_conflict(true);
                // store fretting change without affecting chord context
                if (n_fret != n_new_fret) {
                    note->undo_change_property(Pid::FRET, n_new_fret);
                }
                if (n_string != n_new_string) {
                    note->undo_change_property(Pid::STRING, n_new_string);
                }
                continue;
            }
        }

        // if the note string (either original or newly assigned) is also used by another note
        if (note->display_fret() == Note::DisplayFretOption::NO_HARMONIC && b_used[n_new_string] > 1) {
            // attempt to find a suitable string, from topmost
            for (int n_temp_string = 0; n_temp_string < strings; n_temp_string++) {
                if (b_used[n_temp_string] < 1 &&
                    (n_temp_fret = fret(note->pitch(), n_temp_string, pitch_offset_at(chord->staff(), chord->tick(), n_temp_string))) !=
                        ::mu::engraving::invalid_fret_index) {
                    b_used[n_new_string]--;  // free previous string
                    b_used[n_temp_string]++; // and occupy new string
                    n_new_fret = n_temp_fret;
                    n_new_string = n_temp_string;
                    break;
                }
            }
        }

        // Still two notes on one string: try a free string with fret <0 or >maxFrets.
        if (note->configuration()->negative_frets_allowed() && note->display_fret() == Note::DisplayFretOption::NO_HARMONIC &&
            b_used[n_new_string] > 1) {
            try_resolve_string_conflict_with_out_of_range_fret(note, strings, b_used, n_new_string, n_new_fret);
        }

        // TODO : try to optimize used fret range, avoiding excessively open positions

        // if fretting did change, store as a fret change
        if (n_fret != n_new_fret) {
            note->undo_change_property(Pid::FRET, n_new_fret);
        }
        if (n_string != n_new_string) {
            note->undo_change_property(Pid::STRING, n_new_string);
        }
    }

    // check for any remaining fret conflict
    for (auto &p : sorted_notes) {
        Note *note = p.second;
        if (skip_dead_notes && note->dead_note()) {
            continue;
        }
        if (!note->negative_fret_used() && (note->string() == -1 || b_used[note->string()] > 1)) {
            note->set_fret_conflict(true);
        }
    }
}

//---------------------------------------------------------
//   frettedStrings
//    Returns the number of fretted strings.
//---------------------------------------------------------

int StringData::fretted_strings() const {
    int num = 0;
    for (auto s : string_table_) {
        if (!s.open) {
            num++;
        }
    }
    return num;
}

//********************
// STATIC METHODS
//********************

//---------------------------------------------------------
//   pitchOffsetAt
//   Computes the pitch offset relevant for string data calculation at the given point.
//
//   For string data calculations, pitch offset may depend on transposition, capos and, possibly, ottavas.
//---------------------------------------------------------

int StringData::pitch_offset_at(const Staff *staff) {
    return -(staff ? staff->origin_part(Part::main_instrument_tick)->instrument()->transpose().chromatic : 0);
}

int StringData::pitch_offset_at(const Staff *staff, const Fraction &tick) {
    if (!staff) {
        return 0;
    }
    int transp = staff->origin_part(tick)->instrument(tick)->transpose().chromatic;
    int offset = -transp + staff->pitch_offset(tick);
    const CapoParams &capo = staff->capo(tick);
    if (capo.active) {
        switch (capo.transpose_mode) {
            case CapoParams::TransposeMode::PLAYBACK_ONLY: break;
            case CapoParams::TransposeMode::STANDARD_ONLY:
            case CapoParams::TransposeMode::TAB_ONLY: offset -= capo.fret_position; break;
        }
    }
    return offset;
}

int StringData::pitch_offset_at(const Staff *staff, const Fraction &tick, int string) {
    if (!staff) {
        return 0;
    }
    int transp = staff->origin_part(tick)->instrument(tick)->transpose().chromatic;
    int offset = -transp + staff->pitch_offset(tick);
    const CapoParams &capo = staff->capo(tick);
    if (capo.active && muse::contains(capo.ignored_strings, (string_idx_t) string)) {
        return offset;
    }
    if (capo.active) {
        switch (capo.transpose_mode) {
            case CapoParams::TransposeMode::PLAYBACK_ONLY: break;
            case CapoParams::TransposeMode::STANDARD_ONLY:
            case CapoParams::TransposeMode::TAB_ONLY: offset -= capo.fret_position; break;
        }
    }
    return offset;
}

//********************
// PRIVATE METHODS
//********************

//---------------------------------------------------------
//   convertPitch
//   Finds string and fret for a note.
//
//   Fills *string and *fret with suitable values for pitch / pitchOffset,
//   using the highest possible string.
//   If note cannot be fretted, uses fret 0 on nearest string and returns false
//
//    Note: Strings are stored internally from lowest (0) to highest (strings()-1),
//          but the returned *string value references strings in reversed, 'visual', order:
//          from highest (0) to lowest (strings()-1)
//---------------------------------------------------------

bool StringData::convert_pitch(int pitch, int pitch_offset, int *string, int *fret, const CapoParams &capo) const {
    int strings = static_cast<int>(string_table_.size());
    if (strings < 1) {
        return false;
    }

    pitch += pitch_offset;

    // if above max fret on highest string, fret on first string, but return failure
    if (pitch > string_table_.at(strings - 1).pitch + frets_) {
        *string = 0;
        *fret = 0;
        return false;
    }

    if (is_five_string_banjo()) {
        // special case: open banjo 5th string
        if (pitch == string_table_.at(0).pitch) {
            *string = 4;
            *fret = 0;
            return true;
        }
        // test remaining 4 strings from highest to lowest
        for (int i = 4; i > 0; i--) {
            InstrString strg = string_table_.at(i);
            if (pitch >= strg.pitch) {
                *string = strings - i - 1;
                *fret = pitch - strg.pitch;
                return true;
            }
        }
    } else {
        // look for a suitable string, starting from the highest
        // NOTE: this assumes there are always enough frets to fill
        // the interval between any fretted string and the next
        for (int i = strings - 1; i >= 0; i--) {
            InstrString strg = string_table_.at(i);
            if (pitch >= strg.pitch) {
                *string = strings - i - 1;
                int fret_correction =
                    (capo.active && muse::contains(capo.ignored_strings, (string_idx_t) *string)) ? capo.fret_position : 0;
                *fret = pitch - strg.pitch + fret_correction;
                return true;
            }
        }
    }

    // if no string found, pitch is below lowest string:
    // fret on last string, but return failure
    *string = strings - 1;
    *fret = 0;
    return false;
}

//---------------------------------------------------------
//   getPitch
//    Returns the pitch corresponding to the string / fret / pitchOffset combination.
//    Returns INVALID_PITCH if not possible
//    Note: frets above max fret are accepted.
//---------------------------------------------------------

int StringData::get_pitch(int string, int fret, int pitch_offset) const {
    size_t strings = string_table_.size();
    if (string < 0 || string >= static_cast<int>(strings)) {
        return ::mu::engraving::invalid_pitch;
    }
    InstrString strg = string_table_.at(strings - string - 1);
    int pitch = strg.pitch - pitch_offset + (strg.open ? 0 : fret);
    if (strg.start_fret > 0 && fret >= strg.start_fret) {
        pitch -= strg.start_fret; // banjo 5th string adjustment
    }
    return pitch;
}

//---------------------------------------------------------
//   fret
//    Returns the fret corresponding to the pitch / string / pitchOffset combination.
//    returns INVALID_FRET_INDEX if not possible
//---------------------------------------------------------

int StringData::fret(int pitch, int string, int pitch_offset) const {
    int strings = static_cast<int>(string_table_.size());
    if (strings < 1) { // no strings at all!
        return ::mu::engraving::invalid_fret_index;
    }

    if (string < 0 || string >= strings) { // no such a string
        return ::mu::engraving::invalid_fret_index;
    }

    pitch += pitch_offset;

    const InstrString &strg = string_table_[strings - string - 1];
    int fret = pitch - strg.pitch;
    if (fret > 0 && strg.start_fret > 0) {
        fret += strg.start_fret; // banjo 5th string adjustment
    }

    // fret number is invalid or string cannot be fretted
    if (fret < 0 || fret > frets_ || (fret > 0 && strg.open)) {
        return ::mu::engraving::invalid_fret_index;
    }
    return fret;
}

//---------------------------------------------------------
//   tryResolveStringConflictWithOutOfRangeFret
//    After in-range conflict resolution fails: assign an unused string using raw
//    fret (may be <0 or >maxFrets). Chooses smallest distance to [0, maxFrets];
//    ties break toward lower string index.
//---------------------------------------------------------

bool StringData::try_resolve_string_conflict_with_out_of_range_fret(const Note *note, int num_strings, std::vector<int> &b_used,
                                                                    int &n_new_string, int &n_new_fret) const {
    int best_string = -1;
    int best_distance = INT32_MAX;
    int best_raw_fret = 0;

    for (int s = 0; s < num_strings; s++) {
        if (b_used[static_cast<size_t>(s)] >= 1) {
            continue;
        }
        const int open_pitch = string_table_.at(string_table_.size() - s - 1).pitch;
        const int raw_fret = note->pitch() + pitch_offset_at(note->staff(), note->tick(), s) - open_pitch; // not clamped to 0..frets_
        int distance;
        if (raw_fret < 0) {
            distance = -raw_fret;
        } else if (raw_fret > frets_) {
            distance = raw_fret - frets_;
        } else {
            continue; // only out-of-range frets here; in-range tried above in fretChords
        }

        if (distance < best_distance || (distance == best_distance && (best_string < 0 || s < best_string))) {
            best_distance = distance;
            best_raw_fret = raw_fret;
            best_string = s;
        }
    }

    if (best_string < 0) {
        return false;
    }

    b_used[static_cast<size_t>(n_new_string)]--;
    b_used[static_cast<size_t>(best_string)]++;
    n_new_fret = best_raw_fret;
    n_new_string = best_string;
    return true;
}

static bool skip_tab_note(const Note *n, bool skip_dead_notes) {
    return n->display_fret() != Note::DisplayFretOption::NO_HARMONIC || (skip_dead_notes && n->dead_note());
}

//---------------------------------------------------------
//   hasPendingPitchChange
//    True if any note's pitch doesn't match its current string/fret.
//---------------------------------------------------------

bool StringData::has_pending_pitch_change(const Chord *chord) const {
    const bool skip_dead = chord->configuration()->keep_dead_notes_unchanged_on_transpose();
    for (Note *note : chord->notes()) {
        if (skip_tab_note(note, skip_dead)) {
            continue;
        }
        if (note->string() < 0) {
            continue;
        }
        if (get_pitch(note->string(), note->fret(), chord->staff(), chord->tick()) != note->pitch()) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   updateFretsOnSameStrings
//    Keep each note on its string, adjust fret to match the new pitch.
//    Set INVALID if the note can't stay on its string.
//---------------------------------------------------------

void StringData::update_frets_on_same_strings(const Chord *chord) const {
    const bool skip_dead = chord->configuration()->keep_dead_notes_unchanged_on_transpose();

    for (Note *note : chord->notes()) {
        if (skip_tab_note(note, skip_dead)) {
            continue;
        }
        if (note->string() < 0) {
            note->set_string(::mu::engraving::invalid_string_index);
            note->set_fret(::mu::engraving::invalid_fret_index);
            continue;
        }
        int pitch = get_pitch(note->string(), note->fret(), chord->staff(), chord->tick());
        int new_fret = note->fret() + note->pitch() - pitch;
        if (new_fret < 0 && !note->configuration()->negative_frets_allowed()) {
            note->set_string(::mu::engraving::invalid_string_index);
            note->set_fret(::mu::engraving::invalid_fret_index);
            continue;
        }
        if (new_fret < 0) {
            // Don't create a negative fret on a string that already holds another note — this would cause a false conflict
            bool same_string_conflict = false;
            for (Note *other : chord->notes()) {
                if (other != note && !skip_tab_note(other, skip_dead) && other->string() == note->string()) {
                    same_string_conflict = true;
                    break;
                }
            }
            if (same_string_conflict) {
                note->set_string(::mu::engraving::invalid_string_index);
                note->set_fret(::mu::engraving::invalid_fret_index);
                continue;
            }
        }
        note->set_fret(new_fret);
    }
}

//---------------------------------------------------------
//   preferBassStringForNegativeFret
//    If a non-bass note landed on a deep negative fret and the bass
//    string is free, move it there when the bass fret is shallower.
//---------------------------------------------------------

void StringData::prefer_bass_string_for_negative_fret(const Chord *chord) const {
    const bool skip_dead = chord->configuration()->keep_dead_notes_unchanged_on_transpose();
    const int bass_str = static_cast<int>(strings()) - 1;
    if (bass_str <= 0) {
        return;
    }
    const int open_low = string_table_.at(0).pitch;

    auto bass_taken = [&](const Note *except) {
        for (Note *o : chord->notes()) {
            if (o != except && !skip_tab_note(o, skip_dead) && o->string() == bass_str) {
                return true;
            }
        }
        return false;
    };

    for (Note *note : chord->notes()) {
        if (skip_tab_note(note, skip_dead) || note->string() < 0 || note->string() == bass_str || note->fret() >= 0) {
            continue;
        }
        if (bass_taken(note)) {
            continue;
        }
        const int raw_low = note->pitch() + pitch_offset_at(chord->staff(), chord->tick(), bass_str) - open_low;
        if (raw_low < 0 && raw_low > note->fret()) {
            note->set_string(bass_str);
            note->set_fret(raw_low);
        }
    }
}

//---------------------------------------------------------
//   reassignNegativeFretNotes
//    Move negative-fret notes to a non-negative string if available.
//    Reset displaced chord-mates for fretChords to reassign.
//---------------------------------------------------------

void StringData::reassign_negative_fret_notes(const Chord *chord) const {
    const bool skip_dead = chord->configuration()->keep_dead_notes_unchanged_on_transpose();
    const int pitch_offset_with_capo = pitch_offset_at(chord->staff(), chord->tick());
    const CapoParams &capo = chord->staff()->capo(chord->tick());

    for (Note *note : chord->notes()) {
        if (skip_tab_note(note, skip_dead) || note->string() < 0 || note->fret() >= 0) {
            continue;
        }
        int target_str = -1;
        int target_fret = -1;
        if (!convert_pitch(note->pitch(), pitch_offset_with_capo, &target_str, &target_fret, capo)) {
            continue;
        }
        Note *positive_on_target = nullptr;
        for (Note *o : chord->notes()) {
            if (o != note && !skip_tab_note(o, skip_dead) && o->string() == target_str && o->fret() >= 0) {
                positive_on_target = o;
                break;
            }
        }
        if (positive_on_target) {
            note->set_string(target_str);
            note->set_fret(target_fret);
            positive_on_target->set_string(::mu::engraving::invalid_string_index);
            positive_on_target->set_fret(::mu::engraving::invalid_fret_index);
        } else {
            note->set_string(::mu::engraving::invalid_string_index);
            note->set_fret(::mu::engraving::invalid_fret_index);
        }
    }
}

//---------------------------------------------------------
//   sortChordNotesUseSameString
//    Tries to keep each note on its currently assigned string when the
//    chord's pitches are transposed, updating only the fret.
//---------------------------------------------------------

void StringData::sort_chord_notes_use_same_string(const Chord *chord) const {
    if (!has_pending_pitch_change(chord)) {
        return;
    }

    update_frets_on_same_strings(chord);

    if (!chord->configuration()->negative_frets_allowed()) {
        return;
    }

    prefer_bass_string_for_negative_fret(chord);
    reassign_negative_fret_notes(chord);
}

//---------------------------------------------------------
//   sortChordNotes
//    Adds to sortedNotes the notes of Chord in string/pitch order
//    Note: notes are sorted first by string (top string being 0),
//          then by negated pitch (higher pitches resulting in lower key),
//          then by order of submission to disambiguate notes with the same pitch.
//          Everything else being equal, this makes notes in higher-numbered voices
//          to be sorted after notes in lower-numbered voices (voice 2 after voice 1 and so on)
//    Notes without a string assigned yet, are sorted according to the lowest string which can accommodate them.
//---------------------------------------------------------

void StringData::sort_chord_notes(std::map<int, Note *> &sorted_notes, const Chord *chord, int *count) const {
    bool use_same_string = chord->configuration()->prefer_same_string_for_transpose();
    const bool skip_dead_notes = chord->configuration()->keep_dead_notes_unchanged_on_transpose();
    int transp = chord->staff() ? chord->origin_part()->instrument(chord->tick())->transpose().chromatic : 0;
    int pitch_offset = -transp + chord->staff()->pitch_offset(chord->segment()->tick());

    if (use_same_string) {
        sort_chord_notes_use_same_string(chord);
    }

    for (Note *note : chord->notes()) {
        if (note->display_fret() != Note::DisplayFretOption::NO_HARMONIC) {
            continue;
        }

        int string = note->string();
        int note_fret = note->fret();

        int pitch = get_pitch(string, note_fret, pitch_offset_at(chord->staff(), chord->tick(), string));
        // if note not fretted yet or current fretting no longer valid,
        // use most convenient string as key
        if (!(skip_dead_notes && note->dead_note()) && !note->negative_fret_used() &&
            (string <= ::mu::engraving::invalid_string_index || note_fret <= ::mu::engraving::invalid_fret_index ||
             (pitch_is_valid(pitch) && pitch != note->pitch()))) {
            note->set_string(::mu::engraving::invalid_string_index);
            note->set_fret(::mu::engraving::invalid_fret_index);
        }

        int key = string * 100000;
        if (string == ::mu::engraving::invalid_string_index && note->configuration()->negative_frets_allowed()) {
            // No string assigned yet: lower note first, then higher (transpose in one go or in steps must match).
            key += (note->pitch() + pitch_offset) * 100 + *count;
        } else {
            key += -(note->pitch() + pitch_offset) * 100 + *count; // disambiguate notes of equal pitch
        }
        sorted_notes.insert({key, note});
        (*count)++;
    }
}

//---------------------------------------------------------
//   configBanjo5thString
//   Assumes isFiveStringBanjo() has already been called.
//   This method looks at the banjo tuning and sets startFret
//   appropriately.
//---------------------------------------------------------

void StringData::config_banjo5th_string() {
    // banjo 5th string (pitch 67 == G)
    InstrString &strg5 = string_table_[0];

    frets_ = 24; // not needed after bug #316931 is fixed

    // adjust startFret if using a 5th string capo (6..12)
    if (strg5.pitch > 67 && strg5.pitch < 74) {
        strg5.start_fret = strg5.pitch - 62;
    } else {
        strg5.start_fret = 5; // no 5th string capo
    }
}

//---------------------------------------------------------
//   adjustBanjo5thFret
//   Convert 5th string fret number from (0, 1, 2, 3...) to (0, 6, 7, 8...).
//   Called from import (GuitarPro mostly)
//   Returns adjusted fret number
//---------------------------------------------------------

int StringData::adjust_banjo5th_fret(int fret) const {
    return (fret > 0 && is_five_string_banjo()) ? fret + string_table_[0].start_fret : fret;
}

//---------------------------------------------------------
//    isFiveStringBanjo
//    Based only on number of strings and tuning - other info
//    may not be available when this is called. Checks 5th string
//    pitch is higher than 4th (i.e. not a 5 string bass)
//---------------------------------------------------------

bool StringData::is_five_string_banjo() const { return string_table_.size() == 5 && string_table_[0].pitch > string_table_[1].pitch; }
} // namespace mu::engraving
