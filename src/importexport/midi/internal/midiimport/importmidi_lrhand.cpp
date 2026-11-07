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
#include "importmidi_lrhand.h"

#include <cstdlib>
#include <limits>
#include <vector>

#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_operations.h"

namespace mu::iex::midi { namespace LRHand {
bool need_to_split(const std::multimap<ReducedFraction, MidiChord> &chords, GM1Program midi_program, bool is_drum_track) {
    if (is_drum_track || !MChord::is_grand_staff_program(midi_program)) {
        return false;
    }

    const int octave = 12;
    for (const auto &chord : chords) {
        const MidiChord &c = chord.second;
        int min_pitch = std::numeric_limits<int>::max();
        int max_pitch = 0;
        for (const auto &note : c.notes) {
            if (note.pitch < min_pitch) {
                min_pitch = note.pitch;
            }
            if (note.pitch > max_pitch) {
                max_pitch = note.pitch;
            }
        }
        if (max_pitch - min_pitch > octave) {
            return true;
        }
    }

    return false;
}

#ifndef NDEBUG

bool are_notes_sorted_by_pitch_in_asc_order(const std::vector<MidiNote> &notes) {
    for (int i = 0; i != notes.size() - 1; ++i) {
        if (notes[i].pitch > notes[i + 1].pitch) {
            return false;
        }
    }
    return true;
}

#endif

struct SplitTry {
    int penalty = 0;
    // split point - note index, such that: LOW part = [0, split point)
    // and HIGH part = [split point, size)
    // if split point = size then the chord is assigned to the left hand
    int prev_split_point = -1;
};

struct ChordSplitData {
    std::multimap<ReducedFraction, MidiChord>::iterator chord;
    std::vector<SplitTry> possible_splits; // each index corresponds to the same note index
};

int find_last_split_point(const std::vector<ChordSplitData> &splits) {
    int split_point = -1;
    int min_penalty = std::numeric_limits<int>::max();
    const auto &possible_splits = splits[splits.size() - 1].possible_splits;

    for (size_t i = 0; i != possible_splits.size(); ++i) {
        if (possible_splits[i].penalty < min_penalty) {
            min_penalty = possible_splits[i].penalty;
            split_point = static_cast<int>(i);
        }
    }

    assert(split_point != -1);

    return split_point;
}

// backward dynamic programming step - collect optimal voice separations

void split_chords(const std::vector<ChordSplitData> &splits, std::multimap<ReducedFraction, MidiChord> &left_hand_chords,
                  std::multimap<ReducedFraction, MidiChord> &chords) {
    assert(splits.size() == chords.size());

    int split_point = find_last_split_point(splits);

    for (size_t pos = splits.size() - 1;; --pos) {
        MidiChord &old_chord = splits[pos].chord->second;
        MidiChord new_chord(old_chord);

        if (split_point > 0 && split_point < old_chord.notes.size()) {
            const auto old_notes = old_chord.notes;

            old_chord.notes.clear();
            for (int i = split_point; i != old_notes.size(); ++i) {
                old_chord.notes.push_back(old_notes[i]);
            }

            new_chord.notes.clear();
            for (int i = 0; i != split_point; ++i) {
                new_chord.notes.push_back(old_notes[i]);
            }

            left_hand_chords.insert({splits[pos].chord->first, new_chord});
        } else if (split_point == old_chord.notes.size()) {
            left_hand_chords.insert({splits[pos].chord->first, new_chord});
            chords.erase(splits[pos].chord);
        }

        if (pos == 0) {
            break;
        }
        split_point = splits[pos].possible_splits[split_point].prev_split_point;
    }
}

int find_pitch_width_penalty(const std::vector<MidiNote> &notes, int split_point) {
    const int octave = 12;
    const int max_pitch_width = octave + 2;
    int penalty = 0;

    if (split_point > 0) {
        const int low_pitch_width = std::abs(notes[0].pitch - notes[split_point - 1].pitch);
        if (low_pitch_width <= octave) {
            penalty += 0;
        } else if (low_pitch_width <= max_pitch_width) {
            penalty += 20;
        } else {
            penalty += 100;
        }
    }

    if (split_point < notes.size()) {
        const int high_pitch_width = std::abs(notes[split_point].pitch - notes[notes.size() - 1].pitch);
        if (high_pitch_width <= octave) {
            penalty += 0;
        } else if (high_pitch_width <= max_pitch_width) {
            penalty += 20;
        } else {
            penalty += 100;
        }
    }

    return penalty;
}

bool is_octave(const std::vector<MidiNote> &notes, int beg, int end) {
    assert(end > 0 && beg >= 0 && end > beg);

    const int octave = 12;
    return end - beg == 2 && notes[end - 1].pitch - notes[beg].pitch == octave;
}

int find_similarity_penalty(const std::vector<MidiNote> &notes, const std::vector<MidiNote> &prev_notes, int split_point,
                            int prev_split_point) {
    int penalty = 0;
    // check for octaves and accompaniment
    if (split_point > 0 && prev_split_point > 0) {
        const bool is_low_octave = is_octave(notes, 0, split_point);
        const bool is_prev_low_octave = is_octave(prev_notes, 0, prev_split_point);

        if (is_low_octave && is_prev_low_octave) { // octaves
            penalty -= 12;
        } else if (split_point > 1 && prev_split_point > 1) { // accompaniment
            penalty -= 5;
        }
    }
    if (split_point < notes.size() && prev_split_point < prev_notes.size()) {
        const bool is_high_octave = is_octave(notes, split_point, notes.size());
        const bool is_prev_high_octave = is_octave(prev_notes, prev_split_point, prev_notes.size());

        if (is_high_octave && is_prev_high_octave) {
            penalty -= 12;
        } else if (notes.size() - split_point > 1 && prev_notes.size() - prev_split_point > 1) {
            penalty -= 5;
        }
    }
    // check for one-note melody
    if (split_point - 0 == 1 && prev_split_point - 0 == 1) {
        penalty -= 12;
    }
    if (notes.size() - split_point == 1 && prev_notes.size() - prev_split_point == 1) {
        penalty -= 12;
    }

    return penalty;
}

bool are_off_times_equal(const std::vector<MidiNote> &notes, int beg, int end) {
    assert(end > 0 && beg >= 0 && end > beg);

    bool are_equal = true;
    const ReducedFraction first_off_time = notes[beg].off_time;
    for (int i = beg + 1; i < end; ++i) {
        if (notes[i].off_time != first_off_time) {
            are_equal = false;
            break;
        }
    }
    return are_equal;
}

int find_duration_penalty(const std::vector<MidiNote> &notes, int split_point) {
    int penalty = 0;
    if (split_point > 0 && are_off_times_equal(notes, 0, split_point)) {
        penalty -= 10;
    }
    if (split_point < notes.size() && are_off_times_equal(notes, split_point, notes.size())) {
        penalty -= 10;
    }
    return penalty;
}

int find_note_count_penalty(const std::vector<MidiNote> &notes, int split_point) {
    const int left_hand_count = split_point;
    const int right_hand_count = notes.size() - split_point;
    if (right_hand_count > 0 && left_hand_count > 0 && left_hand_count < right_hand_count) {
        return 5;
    }
    if (right_hand_count == 0 && left_hand_count > 1) {
        return 10;
    }
    return 0;
}

int find_intersection_penalty(const ReducedFraction &current_on_time, int prev_pos, int prev_split_point,
                              const ReducedFraction &max_chord_len, const std::vector<ChordSplitData> &splits, bool has_low_notes,
                              bool has_high_notes) {
    int penalty = 0;
    int pos = prev_pos;
    int split_point = prev_split_point;

    while (splits[pos].chord->first + max_chord_len > current_on_time) {
        const MidiChord &chord = splits[pos].chord->second;

        if (has_low_notes) {
            ReducedFraction max_note_off_time;
            for (int i = 0; i != split_point; ++i) {
                if (chord.notes[i].off_time > max_note_off_time) {
                    max_note_off_time = chord.notes[i].off_time;
                }
            }
            if (max_note_off_time > current_on_time) {
                penalty += 10;
            }
        }
        if (has_high_notes) {
            ReducedFraction max_note_off_time;
            for (int i = split_point; i != chord.notes.size(); ++i) {
                if (chord.notes[i].off_time > max_note_off_time) {
                    max_note_off_time = chord.notes[i].off_time;
                }
            }
            if (max_note_off_time > current_on_time) {
                penalty += 10;
            }
        }

        if (pos == 0) {
            break;
        }
        const SplitTry &split_try = splits[pos].possible_splits[split_point];
        split_point = split_try.prev_split_point;
        --pos;
    }

    return penalty;
}

std::vector<ChordSplitData> find_splits(std::multimap<ReducedFraction, MidiChord> &chords) {
    std::vector<ChordSplitData> splits;
    int pos = 0;
    ReducedFraction max_chord_len;

    for (auto it = chords.begin(); it != chords.end(); ++it) {
        const auto &notes = it->second.notes;
#ifndef NDEBUG
        assert(!notes.empty());
        assert(are_notes_sorted_by_pitch_in_asc_order(notes));
#endif
        const auto len = MChord::max_note_off_time(notes) - it->first;
        if (len > max_chord_len) {
            max_chord_len = len;
        }

        ChordSplitData split;
        split.chord = it;

        for (int split_point = 0; split_point <= notes.size(); ++split_point) {
            SplitTry split_try;
            split_try.penalty = find_pitch_width_penalty(notes, split_point) + find_duration_penalty(notes, split_point) +
                                find_note_count_penalty(notes, split_point);

            if (pos > 0) {
                int best_prev_split_point = -1;
                int min_penalty = std::numeric_limits<int>::max();
                const auto &prev_notes = std::prev(it)->second.notes;

                for (int prev_split_point = 0; prev_split_point <= prev_notes.size(); ++prev_split_point) {
                    const int prev_penalty = splits[pos - 1].possible_splits[prev_split_point].penalty +
                                             find_similarity_penalty(notes, prev_notes, split_point, prev_split_point) +
                                             find_intersection_penalty(it->first, pos - 1, prev_split_point, max_chord_len, splits,
                                                                       split_point > 0, split_point < notes.size());

                    if (prev_penalty < min_penalty) {
                        min_penalty = prev_penalty;
                        best_prev_split_point = prev_split_point;
                    }
                }

                assert(best_prev_split_point != -1);

                split_try.penalty += min_penalty;
                split_try.prev_split_point = best_prev_split_point;
            }

            split.possible_splits.push_back(split_try);
        }
        splits.push_back(split);

        ++pos;
    }

    return splits;
}

void insert_new_left_hand_track(std::multimap<int, MTrack> &tracks, std::multimap<int, MTrack>::iterator &it,
                                const std::multimap<ReducedFraction, MidiChord> &left_hand_chords) {
    auto left_hand_track = it->second;
    left_hand_track.chords = left_hand_chords;
    it = tracks.insert({it->first, left_hand_track});
}

// maybe todo later: if range of right-hand chords > OCTAVE
// => assign all bottom right-hand chords to another, third track

void split_staff(std::multimap<int, MTrack> &tracks, std::multimap<int, MTrack>::iterator &it) {
    auto &chords = it->second.chords;
    if (chords.empty()) {
        return;
    }
    MChord::sort_notes_by_pitch(chords);
    std::vector<ChordSplitData> splits = find_splits(chords);

    assert(!splits.empty());

    std::multimap<ReducedFraction, MidiChord> left_hand_chords;
    split_chords(splits, left_hand_chords, chords);

    if (!left_hand_chords.empty()) {
        insert_new_left_hand_track(tracks, it, left_hand_chords);
    }
}

void add_new_left_hand_chord(std::multimap<ReducedFraction, MidiChord> &left_hand_chords, const std::vector<MidiNote> &left_hand_notes,
                             const std::multimap<ReducedFraction, MidiChord>::iterator &it) {
    MidiChord left_hand_chord = it->second;
    left_hand_chord.notes = left_hand_notes;
    left_hand_chords.insert({it->first, left_hand_chord});
}

void split_into_left_right_hands(std::multimap<int, MTrack> &tracks) {
    for (auto it = tracks.begin(); it != tracks.end(); ++it) {
        if (it->second.mtrack->drum_track() || it->second.chords.empty()) {
            continue;
        }
        const auto &opers = midi_import_operations.data()->track_opers;
        // iterator 'it' will change after track split to ++it
        // C++11 guarantees that newly inserted item with equal key will go after:
        //    "The relative ordering of elements with equivalent keys is preserved,
        //     and newly inserted elements follow those with equivalent keys
        //     already in the container"
        if (opers.do_staff_split.value(it->second.index_of_operation)) {
            split_staff(tracks, it);
        }
    }
}
}} // namespace mu::iex::midi::LRHand
