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
#include "importmidi_voice.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>

#include "engraving/dom/durationtype.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/sig.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_tuplet.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiVoice {
// no more than VOICES

size_t to_int_voice_count(MidiOperations::VoiceCount value) {
    switch (value) {
        case MidiOperations::VoiceCount::V_1: return 1;
        case MidiOperations::VoiceCount::V_2: return 2;
        case MidiOperations::VoiceCount::V_3: return 3;
        case MidiOperations::VoiceCount::V_4: return 4;
    }
    return ::mu::engraving::voices;
}

int voice_limit() {
    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();
    const size_t allowed_voice_count = to_int_voice_count(opers.max_voice_count.value(current_track));

    Q_ASSERT_X(allowed_voice_count <= ::mu::engraving::voices, "MidiVoice::voiceLimit",
               "Allowed voice count exceeds MuseScore Studio voice limit");

    return static_cast<int>(allowed_voice_count);
}

#ifndef NDEBUG

bool are_notes_sorted_by_off_time_in_asc_order(const std::vector<MidiNote> &notes, const std::vector<int> &group_of_indexes) {
    for (size_t i = 0; i != group_of_indexes.size() - 1; ++i) {
        if (notes[group_of_indexes[i]].off_time > notes[group_of_indexes[i + 1]].off_time) {
            return false;
        }
    }
    return true;
}

bool are_notes_sorted_by_off_time_in_asc_order(const std::vector<MidiNote> &notes) {
    for (int i = 0; i != (int) notes.size() - 1; ++i) {
        if (notes[i].off_time > notes[i + 1].off_time) {
            return false;
        }
    }
    return true;
}

bool does_tuplet_already_exist(const ReducedFraction &tuplet_on_time, int voice,
                               const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets) {
    const auto range = tuplets.equal_range(tuplet_on_time);
    bool found = false;
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.voice == voice) {
            found = true;
            break;
        }
    }
    return found;
}

bool are_voices_same(const std::multimap<ReducedFraction, MidiChord> &chords) {
    for (const auto &chord : chords) {
        const MidiChord &c = chord.second;
        if (c.is_in_tuplet && c.tuplet->second.voice != c.voice) {
            return false;
        }
        for (const auto &note : c.notes) {
            if (note.is_in_tuplet && note.tuplet->second.voice != c.voice) {
                return false;
            }
        }
    }
    return true;
}

#endif

bool all_notes_have_equal_length(const std::vector<MidiNote> &notes) {
    const auto &off_time = notes[0].off_time;
    for (int i = 1; i != notes.size(); ++i) {
        if (notes[i].off_time != off_time) {
            return false;
        }
    }
    return true;
}

int find_duration_count_in_group(const ReducedFraction &chord_on_time, const std::vector<MidiNote> &notes, int voice,
                                 const std::vector<int> &group_of_indexes, const TimeSigMap *sigmap,
                                 const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets) {
#ifndef NDEBUG
    Q_ASSERT_X(are_notes_sorted_by_off_time_in_asc_order(notes, group_of_indexes), "MidiVoice::findDurationCountInGroup",
               "Notes are not sorted by off time in ascending order");
#endif
    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();
    const bool use_dots = opers.use_dots.value(current_track);

    int count = 0;
    auto on_time = chord_on_time;
    auto on_time_bar_start = MidiBar::find_bar_start(on_time, sigmap);
    auto on_time_bar_fraction = ReducedFraction(sigmap->timesig(on_time_bar_start.ticks()).timesig());

    for (int i : group_of_indexes) {
        const auto &off_time = notes[i].off_time;
        if (off_time == on_time) {
            continue;
        }
        const auto off_time_bar_start = MidiBar::find_bar_start(off_time, sigmap);

        if (off_time_bar_start != on_time_bar_start) {
            const auto off_time_bar_fraction = ReducedFraction(sigmap->timesig(off_time_bar_start.ticks()).timesig());

            const auto tuplets_for_duration =
                MidiTuplet::find_tuplets_in_bar_for_duration(voice, on_time_bar_start, on_time, off_time_bar_start - on_time, tuplets);

            // additional durations on measure boundary
            const auto durations =
                Meter::to_duration_list(on_time - on_time_bar_start, off_time_bar_start - on_time_bar_start, off_time_bar_fraction,
                                        tuplets_for_duration, Meter::DurationType::NOTE, use_dots, false);

            count += MidiDuration::duration_count(durations);

            on_time = off_time_bar_start;
            on_time_bar_start = off_time_bar_start;
            on_time_bar_fraction = off_time_bar_fraction;
        }

        const auto tuplets_for_duration =
            MidiTuplet::find_tuplets_in_bar_for_duration(voice, on_time_bar_start, on_time, off_time - on_time, tuplets);

        const auto durations = Meter::to_duration_list(on_time - on_time_bar_start, off_time - on_time_bar_start, on_time_bar_fraction,
                                                       tuplets_for_duration, Meter::DurationType::NOTE, use_dots, false);

        count += MidiDuration::duration_count(durations);

        on_time = off_time;
    }
    return count;
}

// count of resulting durations in music notation

int find_duration_count(const std::vector<MidiNote> &notes, int voice, int split_point, const ReducedFraction &chord_on_time,
                        const TimeSigMap *sigmap, const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets) {
    std::vector<int> low_group;
    std::vector<int> high_group;

    for (int i = 0; i != split_point; ++i) {
        low_group.push_back(i);
    }
    for (int i = split_point; i != notes.size(); ++i) {
        high_group.push_back(i);
    }

    std::sort(low_group.begin(), low_group.end(), [&](int i1, int i2) { return notes[i1].off_time < notes[i2].off_time; });
    std::sort(high_group.begin(), high_group.end(), [&](int i1, int i2) { return notes[i1].off_time < notes[i2].off_time; });

    return find_duration_count_in_group(chord_on_time, notes, voice, low_group, sigmap, tuplets) +
           find_duration_count_in_group(chord_on_time, notes, voice, high_group, sigmap, tuplets);
}

int find_optimal_split_point(const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it, const TimeSigMap *sigmap,
                             const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
                             const std::multimap<ReducedFraction, MidiChord> &chords) {
    const auto &notes = chord_it->second.notes;

#ifndef NDEBUG
    Q_ASSERT_X(!notes.empty(), "MidiVoice::findOptimalSplitPoint", "Notes are empty");
    Q_ASSERT_X(are_notes_sorted_by_off_time_in_asc_order(notes), "MidiVoice::findOptimalSplitPoint",
               "Notes are not sorted by length in ascending order");
#endif
    int opt_split = -1;

    if (!all_notes_have_equal_length(notes)) {
        int min_note_count = std::numeric_limits<int>::max();

        for (int split_point = 1; split_point != notes.size(); ++split_point) {
            // optimization: don't split notes with equal durations
            if (notes[split_point - 1].off_time == notes[split_point].off_time) {
                continue;
            }
            int note_count = find_duration_count(notes, chord_it->second.voice, split_point, chord_it->first, sigmap, tuplets);
            if (note_count < min_note_count) {
                min_note_count = note_count;
                opt_split = split_point;
            }
        }

        Q_ASSERT_X(opt_split != -1, "MidiVoice::findOptimalSplitPoint", "Optimal split point was not defined");
    } else {
        const auto off_time = notes.front().off_time;
        for (auto it = std::next(chord_it); it != chords.end(); ++it) {
            if (it->first >= off_time) {
                break;
            }
            if (it->second.voice != chord_it->second.voice) {
                continue;
            }
            if (it->first < off_time) {
                opt_split = 0;
                break;
            }
        }
    }

    return opt_split;
}

// which part of chord notes, sorted by length - low note indexes or high note indexes
// - should be moved to another voice

enum class MovedVoiceGroup { LOW, HIGH };

struct VoiceSplit {
    MovedVoiceGroup group;
    int voice = -1;
};

std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator>::const_iterator find_inserted_tuplet(
    const ReducedFraction &on_time, int voice,
    const std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets) {
    const auto range = inserted_tuplets.equal_range(on_time);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second->second.voice == voice) {
            return it;
        }
    }
    return inserted_tuplets.end();
}

// if new chord intersected with tuplet that already was inserted
// due to some previous chord separation - then it is not an intersection:
// the new chord belongs to this tuplet

bool has_intersection_with_tuplets(
    int voice, const ReducedFraction &on_time, const ReducedFraction &off_time,
    const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
    const std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
    const ReducedFraction &tuplet_on_time) {
    const auto found_tuplets = MidiTuplet::find_tuplets_for_time_range(voice, on_time, off_time - on_time, tuplets, true);
    for (const auto &tuplet_it : found_tuplets) {
        const auto ins = find_inserted_tuplet(tuplet_it->first, voice, inserted_tuplets);
        const bool belongs_to_inserted = (ins != inserted_tuplets.end() && ins->first == tuplet_on_time);
        if (!belongs_to_inserted) {
            return true;
        }
    }

    return false;
}

void add_group_splits(
    std::vector<VoiceSplit> &splits, const ReducedFraction &max_chord_length, const std::multimap<ReducedFraction, MidiChord> &chords,
    const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
    const std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
    const ReducedFraction &tuplet_on_time, const ReducedFraction &on_time, const ReducedFraction &group_off_time, int orig_voice,
    MovedVoiceGroup group_type, int max_occupied_voice) {
    const int limit = voice_limit();
    bool split_added = false;

    for (int voice = 0; voice != limit; ++voice) {
        if (voice == orig_voice) {
            continue;
        }
        if (voice > max_occupied_voice && split_added) {
            break;
        }
        if (has_intersection_with_tuplets(voice, on_time, group_off_time, tuplets, inserted_tuplets, tuplet_on_time)) {
            continue;
        }
        const auto found_chords = MChord::find_chords_for_time_range(voice, on_time, group_off_time, chords, max_chord_length);
        if (!found_chords.empty()) {
            continue;
        }

        split_added = true;
        VoiceSplit split;
        split.group = group_type;
        split.voice = voice;
        splits.push_back(split);
    }
}

ReducedFraction maximize_off_time(const MidiNote &note, const ReducedFraction &off_time) {
    auto result = off_time;
    if (note.off_time > off_time) {
        result = note.off_time;
    }
    if (note.is_in_tuplet) {
        const auto &tuplet = note.tuplet->second;
        if (tuplet.on_time + tuplet.len > result) {
            result = tuplet.on_time + tuplet.len;
        }
    }
    return result;
}

std::vector<VoiceSplit> find_possible_voice_splits(
    int orig_voice, const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it, int split_point,
    const std::multimap<ReducedFraction, MidiChord> &chords, const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
    const std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
    int max_occupied_voice) {
    std::vector<VoiceSplit> splits;

    ReducedFraction on_time = chord_it->first;
    ReducedFraction low_group_off_time(0, 1);
    ReducedFraction high_group_off_time(0, 1);

    const auto &notes = chord_it->second.notes;
    for (int i = 0; i != split_point; ++i) {
        low_group_off_time = maximize_off_time(notes[i], low_group_off_time);
    }
    for (int i = split_point; i != notes.size(); ++i) {
        high_group_off_time = maximize_off_time(notes[i], high_group_off_time);
    }

    ReducedFraction tuplet_on_time(-1, 1);
    if (chord_it->second.is_in_tuplet) {
        const auto &tuplet = chord_it->second.tuplet->second;
        tuplet_on_time = tuplet.on_time;
        if (tuplet.on_time < on_time) {
            on_time = tuplet.on_time;
        }
        if (tuplet.on_time + tuplet.len > low_group_off_time) {
            low_group_off_time = tuplet.on_time + tuplet.len;
        }
        if (tuplet.on_time + tuplet.len > high_group_off_time) {
            high_group_off_time = tuplet.on_time + tuplet.len;
        }
    }

    const ReducedFraction max_chord_length = MChord::find_max_chord_length(chords);

    if (split_point > 0) {
        add_group_splits(splits, max_chord_length, chords, tuplets, inserted_tuplets, tuplet_on_time, on_time, low_group_off_time,
                         orig_voice, MovedVoiceGroup::LOW, max_occupied_voice);
    }
    if (split_point < notes.size()) {
        add_group_splits(splits, max_chord_length, chords, tuplets, inserted_tuplets, tuplet_on_time, on_time, high_group_off_time,
                         orig_voice, MovedVoiceGroup::HIGH, max_occupied_voice);
    }

    return splits;
}

const int max_pitch_dist = 1000;

int find_prev_pitch_dist(const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it, int average_pitch,
                         const std::multimap<ReducedFraction, MidiChord> &chords, int voice) {
    auto it = chord_it;
    while (it != chords.begin()) {
        --it;
        if (it->second.voice == voice) {
            return std::abs(MChord::chord_average_pitch(it->second.notes) - average_pitch);
        }
    }
    return ::mu::iex::midi::MidiVoice::max_pitch_dist;
}

int find_next_pitch_dist(const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it, int average_pitch,
                         const std::multimap<ReducedFraction, MidiChord> &chords, int voice) {
    auto it = (chord_it == chords.end()) ? chord_it : std::next(chord_it);
    while (it != chords.end()) {
        if (it->second.voice == voice) {
            return std::abs(MChord::chord_average_pitch(it->second.notes) - average_pitch);
        }
        ++it;
    }
    return ::mu::iex::midi::MidiVoice::max_pitch_dist;
}

int find_min_pitch_dist(int average_pitch, const int voice, const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                        const std::multimap<ReducedFraction, MidiChord> &chords) {
    const int octave = 12;
    const int prev_pitch_dist = find_prev_pitch_dist(chord_it, average_pitch, chords, voice);
    const int next_pitch_dist = find_next_pitch_dist(chord_it, average_pitch, chords, voice);

    int pitch_dist = ::mu::iex::midi::MidiVoice::max_pitch_dist;

    if (prev_pitch_dist < next_pitch_dist && prev_pitch_dist <= octave) {
        pitch_dist = prev_pitch_dist;
    } else if (next_pitch_dist <= prev_pitch_dist && next_pitch_dist <= octave) {
        pitch_dist = next_pitch_dist;
    }

    return pitch_dist;
}

int find_average_low_pitch(const std::vector<MidiNote> &notes, int split_point) {
    int average_low_pitch = 0;
    for (int j = 0; j != split_point; ++j) {
        average_low_pitch += notes[j].pitch;
    }
    average_low_pitch = static_cast<int>(std::lround(average_low_pitch * 1.0 / split_point));

    return average_low_pitch;
}

int find_average_high_pitch(const std::vector<MidiNote> &notes, int split_point) {
    int average_high_pitch = 0;
    for (int j = split_point; j != notes.size(); ++j) {
        average_high_pitch += notes[j].pitch;
    }
    average_high_pitch = static_cast<int>(std::lround(average_high_pitch * 1.0 / (notes.size() - split_point)));

    return average_high_pitch;
}

VoiceSplit find_best_split(const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                           const std::multimap<ReducedFraction, MidiChord> &chords, const std::vector<VoiceSplit> &possible_splits,
                           int split_point) {
    // to minimize <pitch distance, voice>
    std::pair<int, int> min_error{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
    int best_split = -1;

    for (size_t i = 0; i != possible_splits.size(); ++i) {
        const int voice = possible_splits[i].voice;
        const auto &notes = chord_it->second.notes;
        int total_pitch_dist = 0;

        if (split_point > 0) {
            const int average_low_pitch = find_average_low_pitch(notes, split_point);
            const int low_voice = (possible_splits[i].group == MovedVoiceGroup::LOW) ? voice : chord_it->second.voice;
            total_pitch_dist += find_min_pitch_dist(average_low_pitch, low_voice, chord_it, chords);
        }

        if (split_point < notes.size()) {
            const int average_high_pitch = find_average_high_pitch(notes, split_point);
            const int high_voice = (possible_splits[i].group == MovedVoiceGroup::HIGH) ? voice : chord_it->second.voice;
            total_pitch_dist += find_min_pitch_dist(average_high_pitch, high_voice, chord_it, chords);
        }

        const std::pair<int, int> error{total_pitch_dist, voice};

        if (error < min_error) {
            min_error = error;
            best_split = static_cast<int>(i);
        }
    }

    Q_ASSERT_X(best_split != -1, "MidiVoice::findBestSplit", "Best split was not found");

    return possible_splits[best_split];
}

void insert_new_tuplet(std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator &tuplet, const ReducedFraction &tuplet_on_time,
                       int new_voice, std::multimap<ReducedFraction, MidiChord> &chords,
                       std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
                       std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets) {
    MidiTuplet::TupletData new_tuplet = tuplet->second;
    new_tuplet.voice = new_voice;
#ifndef NDEBUG
    Q_ASSERT_X(!does_tuplet_already_exist(new_tuplet.on_time, new_voice, tuplets), "MidiVoice::addOrUpdateTuplet", "Tuplet already exists");
#endif
    tuplet = tuplets.insert({tuplet_on_time, new_tuplet});
    inserted_tuplets.insert({tuplet_on_time, tuplet});
    // maybe impossible due to intersection check but check anyway:
    // if there is a non-tuplet chord with on time = tuplet on time
    // then add that chord to the new tuplet
    const auto range = chords.equal_range(tuplet_on_time);
    for (auto it = range.first; it != range.second; ++it) {
        MidiChord &chord = it->second;
        if (chord.voice == new_voice && !chord.is_in_tuplet) {
            chord.is_in_tuplet = true;
            chord.tuplet = tuplet;
            break;
        }
    }
}

bool can_split_tuplet(
    const std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator &tuplet, int new_voice, const ReducedFraction &chord_on_time,
    const std::vector<MidiNote> &notes,
    const std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
    const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &max_chord_length) {
    const auto tuplet_on_time = tuplet->first;
    const auto &t = tuplet->second;
    const auto inserted_tuplet = find_inserted_tuplet(tuplet_on_time, new_voice, inserted_tuplets);

    bool need_insert_tuplet = false;
    if (inserted_tuplet == inserted_tuplets.end() && MidiTuplet::has_non_trivial_chord(chord_on_time, notes, t.on_time, t.len)) {
        need_insert_tuplet = true;
    }

    const bool need_delete_old_tuplet = MidiTuplet::is_tuplet_useless(t.voice, tuplet_on_time, t.len, max_chord_length, chords);
    // insert new tuplet only if old tuplet should be deleted
    // because parallel equal tuplets with different voices aren't pretty
    if (need_insert_tuplet && !need_delete_old_tuplet) {
        // need to amend chord split - need but cannot insert tuplet,
        // in that case no changes should be done earlier in this function!
        return false;
    }

    return true;
}

void split_tuplet(std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator &tuplet, int new_voice,
                  const ReducedFraction &chord_on_time, const std::vector<MidiNote> &notes, bool &is_in_tuplet,
                  std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
                  std::multimap<ReducedFraction, MidiChord> &chords, std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
                  const ReducedFraction &max_chord_length, bool allow_parallel_tuplets, bool is_this_a_chord) {
    Q_ASSERT_X(is_in_tuplet, "MidiVoice::splitTuplet", "Tuplet chord/note is not actually in tuplet");

    const auto old_tuplet = tuplet;
    const auto tuplet_on_time = tuplet->first;
    const auto inserted_tuplet = find_inserted_tuplet(tuplet_on_time, new_voice, inserted_tuplets);

    bool need_insert_tuplet = false;

    if (inserted_tuplet == inserted_tuplets.end()) {
        const auto &t = tuplet->second;
        if (MidiTuplet::has_non_trivial_chord(chord_on_time, notes, t.on_time, t.len)) {
            need_insert_tuplet = true;
        } else {
            is_in_tuplet = false;
        }
    } else {
        tuplet = inserted_tuplet->second;
    }

    const auto &t = old_tuplet->second;
    const bool need_delete_old_tuplet = MidiTuplet::is_tuplet_useless(t.voice, t.on_time, t.len, max_chord_length, chords);
    // insert new tuplet only if old tuplet was erased
    // because parallel equal tuplets with different voices aren't pretty
    const bool can_insert_tuplet = (allow_parallel_tuplets) ? need_insert_tuplet : (need_insert_tuplet && need_delete_old_tuplet);
    if (can_insert_tuplet) {
        insert_new_tuplet(tuplet, tuplet_on_time, new_voice, chords, tuplets, inserted_tuplets);
        is_in_tuplet = true;
    }
    if (need_delete_old_tuplet) { // delete after insert, because oldTuplet can be used
        bool can_remove_tuplet = true;
        if (is_this_a_chord) {
            // don't remove tuplet if chord notes have the same tuplet,
            // it will be removed on note split
            for (const auto &note : notes) {
                if (note.is_in_tuplet && note.tuplet == old_tuplet) {
                    can_remove_tuplet = false;
                    break;
                }
            }
        }
        if (can_remove_tuplet) {
            MidiTuplet::remove_tuplet(old_tuplet, tuplets, max_chord_length, chords);
        }
    }

    Q_ASSERT_X(allow_parallel_tuplets || !need_insert_tuplet || need_delete_old_tuplet, "MidiVoice::splitTuplet",
               "Tuplet need to be added but the old tuplet was not deleted");
}

bool update_chord_tuplets(
    MidiChord &chord, const ReducedFraction &on_time,
    std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
    std::multimap<ReducedFraction, MidiChord> &chords, std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
    const ReducedFraction &max_chord_length, bool allow_parallel_tuplets) {
    if (!allow_parallel_tuplets) {
        bool can_do_split = true;
        if (chord.is_in_tuplet &&
            !can_split_tuplet(chord.tuplet, chord.voice, on_time, chord.notes, inserted_tuplets, chords, max_chord_length)) {
            can_do_split = false;
        }
        if (can_do_split) {
            for (auto &note : chord.notes) {
                if (note.is_in_tuplet &&
                    !can_split_tuplet(note.tuplet, chord.voice, on_time, {note}, inserted_tuplets, chords, max_chord_length)) {
                    can_do_split = false;
                    break;
                }
            }
        }
        if (!can_do_split) {
            return false;
        }
    }

    if (chord.is_in_tuplet) {
        split_tuplet(chord.tuplet, chord.voice, on_time, chord.notes, chord.is_in_tuplet, inserted_tuplets, chords, tuplets,
                     max_chord_length, allow_parallel_tuplets, true);
    }
    for (auto &note : chord.notes) {
        if (note.is_in_tuplet) {
            split_tuplet(note.tuplet, chord.voice, on_time, {note}, note.is_in_tuplet, inserted_tuplets, chords, tuplets, max_chord_length,
                         allow_parallel_tuplets, false);
        }
    }

    return true;
}

int find_max_occupied_voice_in_bar(const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                                   const std::multimap<ReducedFraction, MidiChord> &chords) {
    const unsigned int bar_index = chord_it->second.bar_index;
    int max_voice = 0;
    // look forward
    for (auto it = chord_it; it != chords.end(); ++it) {
        const MidiChord &chord = it->second;
        if ((unsigned int) chord.bar_index > bar_index + 1) {
            break;
        }
        if ((unsigned int) chord.bar_index == bar_index && chord.voice > max_voice) {
            max_voice = chord.voice;
        }
    }
    // look backward
    for (auto it = chord_it;;) {
        const MidiChord &chord = it->second;
        if ((unsigned int) chord.bar_index + 1 < bar_index) {
            break;
        }
        if ((unsigned int) chord.bar_index == bar_index && chord.voice > max_voice) {
            max_voice = chord.voice;
        }
        if (it == chords.begin()) {
            break;
        }
        --it;
    }

    return max_voice;
}

bool split_chord_to_voice(
    std::multimap<ReducedFraction, MidiChord>::iterator &chord_it, const std::set<int> &notes_to_move, int new_voice,
    std::multimap<ReducedFraction, MidiChord> &chords, std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets,
    std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> &inserted_tuplets,
    const ReducedFraction &max_chord_length, bool allow_parallel_tuplets) {
    if (notes_to_move.empty()) {
        return false;
    }

    bool split_done = true;
    const ReducedFraction on_time = chord_it->first;
    MidiChord &chord = chord_it->second;
    auto &notes = chord.notes;
#ifndef NDEBUG
    Q_ASSERT_X(MidiTuplet::is_tuplet_range_ok(*chord_it, tuplets), "MidiVoice::splitChordToVoice, before split",
               "Tuplet chord/note is outside tuplet "
               "or non-tuplet chord/note is inside tuplet before simplification");
#endif
    if (notes_to_move.size() == notes.size()) {
        // don't split chord, just move it to another voice
        const int old_voice = chord.voice; // remember for possible undo
        chord.voice = new_voice;

        const bool success =
            update_chord_tuplets(chord, on_time, inserted_tuplets, chords, tuplets, max_chord_length, allow_parallel_tuplets);
        if (!success) {
            split_done = false;
            chord.voice = old_voice; // rollback
        }
    } else { // split chord
        MidiChord new_chord(chord);
        new_chord.notes.clear();
        new_chord.voice = new_voice;
        std::vector<MidiNote> updated_old_notes;

        for (int i = 0; i != static_cast<int>(notes.size()); ++i) {
            if (notes_to_move.find(i) != notes_to_move.end()) {
                new_chord.notes.push_back(notes[i]);
            } else {
                updated_old_notes.push_back(notes[i]);
            }
        }

        const auto remembered_notes = notes; // to undo split if necessary
        // update notes before tuplet update because there will be check
        // for empty tuplets
        notes = updated_old_notes;

        const bool success =
            update_chord_tuplets(new_chord, on_time, inserted_tuplets, chords, tuplets, max_chord_length, allow_parallel_tuplets);
        if (success) {
            Q_ASSERT_X(!notes.empty(), "MidiVoice::splitChordToVoice", "Old chord notes are empty");
            Q_ASSERT_X(!new_chord.notes.empty(), "MidiVoice::splitChordToVoice", "New chord notes are empty");

            chord_it = chords.insert({on_time, new_chord});
        } else {
            notes = remembered_notes; // rollback
            split_done = false;
        }
    }
#ifndef NDEBUG
    Q_ASSERT_X(MidiTuplet::is_tuplet_range_ok(*chord_it, tuplets), "MidiVoice::splitChordToVoice, after split",
               "Tuplet chord/note is outside tuplet "
               "or non-tuplet chord/note is inside tuplet before simplification");
#endif
    return split_done;
}

std::set<int> find_notes_to_move(const std::vector<MidiNote> &notes, int split_point, MovedVoiceGroup split_group) {
    std::set<int> notes_to_move;
    if (split_point == 0 || split_point == notes.size()) {
        // don't split chord, just move it to another voice
        for (int i = 0; i != notes.size(); ++i) {
            notes_to_move.insert(i);
        }
    } else {
        switch (split_group) {
            case MovedVoiceGroup::LOW:
                for (int i = 0; i != split_point; ++i) {
                    notes_to_move.insert(i);
                }
                break;
            case MovedVoiceGroup::HIGH:
                for (int i = split_point; i != notes.size(); ++i) {
                    notes_to_move.insert(i);
                }
                break;
        }
    }
    return notes_to_move;
}

bool do_voice_separation(std::multimap<ReducedFraction, MidiChord> &chords, const TimeSigMap *sigmap,
                         std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets) {
#ifndef NDEBUG
    Q_ASSERT_X(MidiTuplet::are_tuplet_ranges_ok(chords, tuplets), "MidiVoice::doVoiceSeparation",
               "Tuplet chord/note is outside tuplet "
               "or non-tuplet chord/note is inside tuplet before voice separation");
#endif
    MChord::sort_notes_by_length(chords);
    std::multimap<ReducedFraction, std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator> inserted_tuplets;
    const ReducedFraction max_chord_length = MChord::find_max_chord_length(chords);

    bool changed = false;
    std::map<int, int> max_occupied_voices; // <bar index, max occupied voice>

    for (auto it = chords.begin(); it != chords.end(); ++it) {
        MidiChord &chord = it->second;
        auto &notes = chord.notes;

        const int split_point = find_optimal_split_point(it, sigmap, tuplets, chords);
        if (split_point == -1) {
            continue;
        }

        auto max_voice_it = max_occupied_voices.find(chord.bar_index);
        if (max_voice_it == max_occupied_voices.end()) {
            const int max_voice = find_max_occupied_voice_in_bar(it, chords);
            max_voice_it = max_occupied_voices.insert({chord.bar_index, max_voice}).first;
        }
        const auto possible_splits =
            find_possible_voice_splits(chord.voice, it, split_point, chords, tuplets, inserted_tuplets, max_voice_it->second);
        if (possible_splits.empty()) {
            continue;
        }

        const VoiceSplit best_split = find_best_split(it, chords, possible_splits, split_point);
        changed = true;

        const std::set<int> notes_to_move = find_notes_to_move(notes, split_point, best_split.group);
        const bool split_done =
            split_chord_to_voice(it, notes_to_move, best_split.voice, chords, tuplets, inserted_tuplets, max_chord_length);
        if (split_done && best_split.voice > max_voice_it->second) {
            max_voice_it->second = best_split.voice;
        }
    }
#ifndef NDEBUG
    Q_ASSERT_X(MidiTuplet::are_tuplet_ranges_ok(chords, tuplets), "MidiVoice::doVoiceSeparation",
               "Tuplet chord/note is outside tuplet "
               "or non-tuplet chord/note is inside tuplet after voice separation");
#endif
    return changed;
}

int find_bar_index_for_off_time(const ReducedFraction &off_time, const TimeSigMap *sigmap) {
    int bar_index, beat, tick;
    sigmap->tick_values(off_time.ticks(), &bar_index, &beat, &tick);
    if (beat == 0 && tick == 0) {
        --bar_index;
    }
    return bar_index;
}

int average_pitch_of_chords(const std::vector<std::multimap<ReducedFraction, MidiChord>::iterator> &chords) {
    if (chords.empty()) {
        return -1;
    }

    int sum_pitch = 0;
    int note_counter = 0;
    for (const auto &chord : chords) {
        const auto &midi_notes = chord->second.notes;
        for (const auto &midi_note : midi_notes) {
            sum_pitch += midi_note.pitch;
            ++note_counter;
        }
    }

    return static_cast<int>(std::lround(sum_pitch * 1.0 / note_counter));
}

void sort_voices_by_pitch(const std::map<int, std::vector<std::multimap<ReducedFraction, MidiChord>::iterator>> &voice_chords) {
    // [newVoice] = <average pitch, old voice>
    std::vector<std::pair<int, int>> pitch_voices;
    for (const auto &v : voice_chords) {
        pitch_voices.push_back({average_pitch_of_chords(v.second), v.first});
    }

    struct {
        bool operator()(const std::pair<int, int> &p1, const std::pair<int, int> &p2) const {
            if (p1.first != p2.first) {
                return p1.first > p2.first;
            }
            return p1.second < p2.second;
        }
    } comparator;

    std::sort(pitch_voices.begin(), pitch_voices.end(), comparator);

    for (int new_voice = 0; new_voice != (int) pitch_voices.size(); ++new_voice) {
        const int old_voice = pitch_voices[new_voice].second;
        if (new_voice == old_voice) {
            continue;
        }
        const auto it = voice_chords.find(old_voice);

        Q_ASSERT_X(it != voice_chords.end(), "MidiVoice::sortVoicesByPitch", "Old voice not found");

        for (auto &chord : it->second) {
            MidiChord &c = chord->second;
            c.voice = new_voice;
            if (c.is_in_tuplet) {
                c.tuplet->second.voice = new_voice;
            }
            for (auto &note : c.notes) {
                if (note.is_in_tuplet) {
                    note.tuplet->second.voice = new_voice;
                }
            }
        }
    }
}

void sort_voices(std::multimap<ReducedFraction, MidiChord> &chords, const TimeSigMap *sigmap) {
    // <voice, chords>
    std::map<int, std::vector<std::multimap<ReducedFraction, MidiChord>::iterator>> voice_chords;
    int max_bar_index = 0;

    for (auto it = chords.begin(); it != chords.end(); ++it) {
        const auto &chord = it->second;

        // some notes: if chord off time belongs to tuplet
        // then this tuplet belongs to the same bar as the chord off time;
        // same is for chord on time

        Q_ASSERT_X(chord.bar_index != -1, "MidiVoice::sortVoices", "Chord bar index is undefined");
        Q_ASSERT_X((!voice_chords.empty()) ? chord.bar_index <= max_bar_index : true, "MidiVoice::sortVoices",
                   "Chord bar index is greater than current index");

        voice_chords[chord.voice].push_back(it);
        const int bar_index = find_bar_index_for_off_time(MChord::max_note_off_time(chord.notes), sigmap);
        if (bar_index > max_bar_index) {
            max_bar_index = bar_index;
        }

        if (std::next(it) == chords.end() || std::next(it)->second.bar_index > max_bar_index) {
            sort_voices_by_pitch(voice_chords);
            voice_chords.clear();
        }
    }
}

bool separate_voices(std::multimap<int, MTrack> &tracks, const TimeSigMap *sigmap) {
    auto &opers = midi_import_operations;
    bool changed = false;

    for (auto &track : tracks) {
        MTrack &mtrack = track.second;
        if (mtrack.mtrack->drum_track()) {
            continue;
        }
        auto &chords = track.second.chords;
        if (chords.empty()) {
            continue;
        }
        const auto user_voice_count = to_int_voice_count(opers.data()->track_opers.max_voice_count.value(mtrack.index_of_operation));
        // pass current track index through MidiImportOperations
        // for further usage
        MidiOperations::CurrentTrackSetter set_current_track{opers, mtrack.index_of_operation};

        if (user_voice_count > 1 && static_cast<int>(user_voice_count) <= voice_limit()) {
#ifndef NDEBUG
            Q_ASSERT_X(MidiTuplet::are_all_tuplets_referenced(mtrack.chords, mtrack.tuplets), "MidiVoice::separateVoices",
                       "Not all tuplets are referenced in chords or notes "
                       "before voice separation");
            Q_ASSERT_X(are_voices_same(mtrack.chords), "MidiVoice::separateVoices",
                       "Different voices of chord and tuplet "
                       "before voice separation");
#endif
            if (do_voice_separation(mtrack.chords, sigmap, mtrack.tuplets)) {
                changed = true;
            }
#ifndef NDEBUG
            Q_ASSERT_X(MidiTuplet::are_all_tuplets_referenced(mtrack.chords, mtrack.tuplets), "MidiVoice::separateVoices",
                       "Not all tuplets are referenced in chords or notes "
                       "after voice separation, before voice sort");
            Q_ASSERT_X(are_voices_same(mtrack.chords), "MidiVoice::separateVoices",
                       "Different voices of chord and tuplet "
                       "after voice separation, before voice sort");
#endif
            sort_voices(mtrack.chords, sigmap);
#ifndef NDEBUG
            Q_ASSERT_X(MidiTuplet::are_all_tuplets_referenced(mtrack.chords, mtrack.tuplets), "MidiVoice::separateVoices",
                       "Not all tuplets are referenced in chords or notes "
                       "after voice sort");
            Q_ASSERT_X(are_voices_same(mtrack.chords), "MidiVoice::separateVoices",
                       "Different voices of chord and tuplet "
                       "after voice sort");
#endif
        }
    }

    return changed;
}
}} // namespace mu::iex::midi::MidiVoice
