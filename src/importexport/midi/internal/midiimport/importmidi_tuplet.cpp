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
#include "importmidi_tuplet.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <set>
#include <vector>

#include "engraving/dom/sig.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_operations.h"
#include "importmidi_quant.h"
#include "importmidi_tuplet_detect.h"
#include "importmidi_tuplet_filter.h"
#include "importmidi_tuplet_voice.h"
#include "log.h"

namespace mu::iex::midi { namespace MidiTuplet {
const std::map<int, TupletLimits> &tuplets_limits() {
    const static std::map<int, TupletLimits> values = {{2, {{2, 3}, 1, 2, 2, 2}}, {3, {{3, 2}, 1, 3, 3, 2}}, {4, {{4, 3}, 3, 4, 3, 3}},
                                                       {5, {{5, 4}, 3, 4, 4, 4}}, {7, {{7, 8}, 4, 6, 5, 6}}, {9, {{9, 8}, 6, 7, 7, 8}}};
    return values;
}

const TupletLimits &tuplet_limits(int tuplet_number) {
    auto it = tuplets_limits().find(tuplet_number);

    assert(it != tuplets_limits().end());

    return it->second;
}

const TupletInfo &tuplet_from_id(int id, const std::vector<TupletInfo> &tuplets) {
    auto it = std::find_if(tuplets.begin(), tuplets.end(), [=](const TupletInfo &t) { return t.id == id; });

    assert(it != tuplets.end());

    return *it;
}

TupletInfo &tuplet_from_id(int id, std::vector<TupletInfo> &tuplets) {
    return const_cast<TupletInfo &>(tuplet_from_id(id, const_cast<const std::vector<TupletInfo> &>(tuplets)));
}

bool has_non_trivial_chord(const ReducedFraction &chord_on_time, const std::vector<MidiNote> &notes, const ReducedFraction &tuplet_on_time,
                           const ReducedFraction &tuplet_len) {
    if (chord_on_time == tuplet_on_time) {
        for (const auto &note : notes) {
            if (note.off_time - chord_on_time < tuplet_len) {
                return true;
            }
        }
    } else {
        if (chord_on_time > tuplet_on_time && chord_on_time < tuplet_on_time + tuplet_len) {
            return true;
        }
        if (chord_on_time >= tuplet_on_time + tuplet_len) {
            return false;
        }

        assert(chord_on_time < tuplet_on_time);

        for (const auto &note : notes) {
            if (note.off_time < tuplet_on_time + tuplet_len) {
                return true;
            }
        }
    }
    return false;
}

bool is_tuplet_useless(int voice, const ReducedFraction &on_time, const ReducedFraction &len, const ReducedFraction &max_chord_length,
                       const std::multimap<ReducedFraction, MidiChord> &chords) {
    bool have_intersection_with_chord = false;
    const auto found_chords = MChord::find_chords_for_time_range(voice, on_time, on_time + len, chords, max_chord_length);
    for (const auto &chord_it : found_chords) {
        // ok, tuplet contains at least one chord
        // check now does it have notes with len < tuplet.len
        if (has_non_trivial_chord(chord_it->first, chord_it->second.notes, on_time, len)) {
            have_intersection_with_chord = true;
            break;
        }
    }

    return !have_intersection_with_chord;
}

std::multimap<ReducedFraction, TupletData>::iterator remove_tuplet(const std::multimap<ReducedFraction, TupletData>::iterator &tuplet_it,
                                                                   std::multimap<ReducedFraction, TupletData> &tuplets,
                                                                   const ReducedFraction &max_chord_length,
                                                                   std::multimap<ReducedFraction, MidiChord> &chords) {
    // remove references to this tuplet in chords and notes
    auto chord_it = chords.lower_bound(tuplet_it->second.on_time + tuplet_it->second.len);
    if (chord_it != chords.begin()) {
        --chord_it;
        while (chord_it->first + max_chord_length > tuplet_it->first) {
            MidiChord &c = chord_it->second;
            if (c.is_in_tuplet && c.tuplet == tuplet_it) {
                c.is_in_tuplet = false;
            }
            for (auto &note : c.notes) {
                if (note.is_in_tuplet && note.tuplet == tuplet_it) {
                    note.is_in_tuplet = false;
                }
            }
            if (chord_it == chords.begin()) {
                break;
            }
            --chord_it;
        }
    }

    return tuplets.erase(tuplet_it);
}

std::multimap<ReducedFraction, TupletData>::iterator
remove_tuplet_if_empty(const std::multimap<ReducedFraction, TupletData>::iterator &tuplet_it,
                       std::multimap<ReducedFraction, TupletData> &tuplets, const ReducedFraction &max_chord_length,
                       std::multimap<ReducedFraction, MidiChord> &chords) {
    auto result_it = tuplet_it;
    const auto &tuplet = tuplet_it->second;

    if (is_tuplet_useless(tuplet.voice, tuplet.on_time, tuplet.len, max_chord_length, chords)) {
        result_it = remove_tuplet(tuplet_it, tuplets, max_chord_length, chords);
    }

    return result_it;
}

// tuplets with no chords are removed
// tuplets with single chord with chord.onTime = tuplet.onTime
//    and chord.len = tuplet.len are removed as well

// better to call this function after quantization

void remove_empty_tuplets(MTrack &track) {
    auto &tuplets = track.tuplets;
    if (tuplets.empty()) {
        return;
    }
    auto &chords = track.chords;
    const ReducedFraction max_chord_length = MChord::find_max_chord_length(chords);

    for (auto tuplet_it = tuplets.begin(); tuplet_it != tuplets.end();) {
        const auto it = remove_tuplet_if_empty(tuplet_it, tuplets, max_chord_length, chords);
        if (it != tuplet_it) {
            tuplet_it = it;
            continue;
        }
        ++tuplet_it;
    }
}

int average_pitch(const std::map<ReducedFraction, std::multimap<ReducedFraction, MidiChord>::iterator> &chords) {
    if (chords.empty()) {
        return -1;
    }

    int sum_pitch = 0;
    int note_counter = 0;
    for (const auto &chord : chords) {
        const auto &midi_notes = chord.second->second.notes;
        for (const auto &midi_note : midi_notes) {
            sum_pitch += midi_note.pitch;
            ++note_counter;
        }
    }

    return static_cast<int>(std::lround(sum_pitch * 1.0 / note_counter));
}

void sort_notes_by_pitch(const std::multimap<ReducedFraction, MidiChord>::iterator &start_bar_chord_it,
                         const std::multimap<ReducedFraction, MidiChord>::iterator &end_bar_chord_it) {
    struct {
        bool operator()(const MidiNote &n1, const MidiNote &n2) { return n1.pitch > n2.pitch; }
    } pitch_comparator;

    for (auto it = start_bar_chord_it; it != end_bar_chord_it; ++it) {
        auto &midi_notes = it->second.notes;
        std::sort(midi_notes.begin(), midi_notes.end(), pitch_comparator);
    }
}

void sort_tuplets_by_average_pitch(std::vector<TupletInfo> &tuplets) {
    struct {
        bool operator()(const TupletInfo &t1, const TupletInfo &t2) { return average_pitch(t1.chords) > average_pitch(t2.chords); }
    } average_pitch_comparator;

    std::sort(tuplets.begin(), tuplets.end(), average_pitch_comparator);
}

std::pair<ReducedFraction, ReducedFraction> tuplet_interval(const TupletInfo &tuplet, const ReducedFraction &basic_quant) {
    ReducedFraction tuplet_end = tuplet.on_time + tuplet.len;

    for (const auto &chord : tuplet.chords) {
        const auto off_time = Quantize::find_max_quantized_off_time(*chord.second, basic_quant);
        if (off_time > tuplet_end) {
            tuplet_end = off_time;
        }
    }

    assert(tuplet_end > tuplet.on_time);

    return std::make_pair(tuplet.on_time, tuplet_end);
}

std::vector<std::pair<ReducedFraction, ReducedFraction>> find_tuplet_intervals(const std::vector<TupletInfo> &tuplets,
                                                                               const ReducedFraction &basic_quant) {
    std::vector<std::pair<ReducedFraction, ReducedFraction>> tuplet_intervals;
    for (const auto &tuplet : tuplets) {
        tuplet_intervals.push_back(tuplet_interval(tuplet, basic_quant));
    }

    return tuplet_intervals;
}

// find tuplets over which duration lies

std::vector<TupletData> find_tuplets_in_bar_for_duration(int voice, const ReducedFraction &bar_start_tick,
                                                         const ReducedFraction &duration_on_time, const ReducedFraction &duration_len,
                                                         const std::multimap<ReducedFraction, TupletData> &tuplet_events) {
    std::vector<TupletData> tuplets_data;
    if (tuplet_events.empty()) {
        return tuplets_data;
    }
    auto tuplet_it = tuplet_events.lower_bound(bar_start_tick);

    while (tuplet_it != tuplet_events.end() && tuplet_it->first < duration_on_time + duration_len) {
        if (tuplet_it->second.voice == voice && duration_on_time < tuplet_it->first + tuplet_it->second.len) {
            // if tuplet and duration intersect each other
            auto tuplet_data = tuplet_it->second;
            // convert tuplet onTime to local bar ticks
            tuplet_data.on_time -= bar_start_tick;
            tuplets_data.push_back(tuplet_data);
        }
        ++tuplet_it;
    }
    return tuplets_data;
}

std::vector<std::multimap<ReducedFraction, TupletData>::const_iterator>
find_tuplets_for_time_range(int voice, const ReducedFraction &on_time, const ReducedFraction &len,
                            const std::multimap<ReducedFraction, TupletData> &tuplet_events, bool strict_comparison) {
    assert(len >= ReducedFraction(0, 1));

    std::vector<std::multimap<ReducedFraction, TupletData>::const_iterator> result;

    if (tuplet_events.empty()) {
        return result;
    }

    auto it = tuplet_events.upper_bound(on_time + len);
    if (it == tuplet_events.begin()) {
        return result;
    }
    --it;
    while (true) {
        const auto &tuplet_data = it->second;
        if (tuplet_data.voice == voice) {
            const auto interval = std::make_pair(on_time, on_time + len);
            const auto tuplet_interval = std::make_pair(tuplet_data.on_time, tuplet_data.on_time + tuplet_data.len);
            if (have_intersection(interval, tuplet_interval, strict_comparison)) {
                result.push_back(it);
            }
        }
        if (it == tuplet_events.begin()) {
            break;
        }
        --it;
    }
    return result;
}

std::multimap<ReducedFraction, TupletData>::const_iterator
find_tuplet_containing_time(int voice, const ReducedFraction &time, const std::multimap<ReducedFraction, TupletData> &tuplet_events,
                            bool strict_comparison) {
    const auto tuplets = find_tuplets_for_time_range(voice, time, ReducedFraction(0, 1), tuplet_events, strict_comparison);
    if (tuplets.empty()) {
        return tuplet_events.end();
    }

    assert(tuplets.size() == 1);

    return tuplets.front();
}

std::set<std::pair<const ReducedFraction, MidiChord> *> find_tuplet_chords(const std::vector<TupletInfo> &tuplets) {
    std::set<std::pair<const ReducedFraction, MidiChord> *> tuplet_chords;
    for (const auto &tuplet_info : tuplets) {
        for (const auto &tuplet_chord : tuplet_info.chords) {
            auto tuplet_it = tuplet_chord.second;
            tuplet_chords.insert(&*tuplet_it);
        }
    }
    return tuplet_chords;
}

std::list<std::multimap<ReducedFraction, MidiChord>::iterator>
find_non_tuplet_chords(const std::vector<TupletInfo> &tuplets,
                       const std::multimap<ReducedFraction, MidiChord>::iterator &start_bar_chord_it,
                       const std::multimap<ReducedFraction, MidiChord>::iterator &end_bar_chord_it) {
    const auto tuplet_chords = find_tuplet_chords(tuplets);
    std::list<std::multimap<ReducedFraction, MidiChord>::iterator> non_tuplets;
    for (auto it = start_bar_chord_it; it != end_bar_chord_it; ++it) {
        if (tuplet_chords.find(&*it) == tuplet_chords.end()) {
            non_tuplets.push_back(it);
        }
    }

    return non_tuplets;
}

// split first tuplet chord, that belong to 2 tuplets, into 2 chords

void split_tuplet_chord(const std::vector<TupletInfo>::iterator &last_match, std::multimap<ReducedFraction, MidiChord> &chords) {
    auto &chord_event = last_match->chords.begin()->second;
    MidiChord &prev_chord = chord_event->second;
    const auto on_time = chord_event->first;
    MidiChord new_chord = prev_chord;
    // erase all notes except the first one
    auto beg = new_chord.notes.begin();
    new_chord.notes.erase(++beg, new_chord.notes.end());
    // erase the first note
    prev_chord.notes.erase(prev_chord.notes.begin());
    chord_event = chords.insert({on_time, new_chord});

    assert(!prev_chord.notes.empty());
}

void split_first_tuplet_chords(std::vector<TupletInfo> &tuplets, std::multimap<ReducedFraction, MidiChord> &chords) {
    for (auto now = tuplets.begin(); now != tuplets.end(); ++now) {
        auto last_match = tuplets.end();
        const auto now_chord_it = now->chords.begin();
        for (auto prev = tuplets.begin(); prev != now; ++prev) {
            auto prev_chord_it = prev->chords.begin();
            if (now->first_chord_index == 0 && prev->first_chord_index == 0 && now_chord_it->second == prev_chord_it->second) {
                last_match = prev;
            }
        }
        if (last_match != tuplets.end()) {
            split_tuplet_chord(last_match, chords);
        }
    }
}

// first tuplet notes with offTime quantization error,
// that is greater for tuplet quant rather than for regular quant,
// are removed from tuplet, except that was the last note

// if noteLen <= tupletLen
//     remove notes with big offTime quant error;
//     and here is a tuning for clearer tuplet processing:
//         if tuplet has only one (first) chord -
//             we can remove all notes and erase tuplet;
//         if tuplet has multiple chords -
//             we should leave at least one note in the first chord

void minimize_off_time_error(std::vector<TupletInfo> &tuplets, std::multimap<ReducedFraction, MidiChord> &chords,
                             std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                             const ReducedFraction &start_bar_tick, const ReducedFraction &basic_quant) {
    for (auto it = tuplets.begin(); it != tuplets.end();) {
        TupletInfo &tuplet_info = *it;
        const auto first_chord = tuplet_info.chords.begin();
        if (first_chord == tuplet_info.chords.end() || tuplet_info.first_chord_index != 0) {
            ++it;
            continue;
        }
        auto on_time = first_chord->second->first;
        if (on_time < start_bar_tick) {
            on_time = start_bar_tick;
        }
        MidiChord &midi_chord = first_chord->second->second;
        auto &notes = midi_chord.notes;

        std::vector<int> removed_indexes;
        std::vector<int> leaved_indexes;
        const auto tuplet_note_len = tuplet_info.len / tuplet_info.tuplet_number;

        for (int i = 0; i != notes.size(); ++i) {
            const auto &note = notes[i];
            if (note.off_time - on_time <= tuplet_info.len && note.off_time - on_time > tuplet_note_len) {
                // if note is longer than tuplet note length
                // then it's simpler to move it outside the tuplet
                // if the error is not larger than error inside tuplet
                if ((tuplet_info.chords.size() == 1 && notes.size() > (int) removed_indexes.size()) ||
                    (tuplet_info.chords.size() > 1 && notes.size() > (int) removed_indexes.size() + 1)) {
                    const auto tuplet_error = Quantize::find_off_time_tuplet_quant_error(
                        on_time, note.off_time, tuplet_info.len, tuplet_limits(tuplet_info.tuplet_number).ratio, start_bar_tick);
                    const auto regular_error = Quantize::find_off_time_quant_error(*first_chord->second, note.off_time, basic_quant);

                    if (tuplet_error >= regular_error) {
                        removed_indexes.push_back(i);
                        continue;
                    }
                }
            }
            leaved_indexes.push_back(i);
        }
        if (!removed_indexes.empty()) {
            MidiChord new_tuplet_chord = midi_chord;
            new_tuplet_chord.notes.clear();
            for (int i : leaved_indexes) {
                new_tuplet_chord.notes.push_back(notes[i]);
            }

            std::vector<MidiNote> new_notes;
            for (int i : removed_indexes) {
                new_notes.push_back(notes[i]);
            }
            notes = new_notes;
            // force add chord to this bar, even if it has barIndex of another bar
            non_tuplets.push_back(first_chord->second);
            if (!new_tuplet_chord.notes.empty()) {
                first_chord->second = chords.insert({first_chord->second->first, new_tuplet_chord});
            } else {
                tuplet_info.chords.erase(tuplet_info.chords.begin());
                if (tuplet_info.chords.empty()) {
                    it = tuplets.erase(it); // remove tuplet without chords
                    continue;
                }
            }
        }
        ++it;
    }
}

void add_chords_between_tuplet_notes(std::vector<TupletInfo> &tuplets,
                                     std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                                     const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &start_bar_tick,
                                     const ReducedFraction &basic_quant) {
    for (TupletInfo &tuplet : tuplets) {
        for (auto it = non_tuplets.begin(); it != non_tuplets.end();) {
            const auto &chord_it = *it;
            const auto &on_time = chord_it->first;
            const auto tuplet_interv = std::make_pair(tuplet.on_time, tuplet.on_time + tuplet.len);
            const auto chord_interv = chord_interval(*chord_it, chords, basic_quant, start_bar_tick);
            if (on_time > tuplet.on_time && have_intersection(tuplet_interv, chord_interv)) {
                const auto tuplet_ratio = tuplet_limits(tuplet.tuplet_number).ratio;

                auto tuplet_error = Quantize::find_on_time_tuplet_quant_error(*chord_it, tuplet.len, tuplet_ratio, start_bar_tick);
                auto regular_error = Quantize::find_on_time_quant_error(*chord_it, basic_quant);

                const auto off_time = MChord::max_note_off_time(chord_it->second.notes);
                if (off_time < tuplet.on_time + tuplet.len) {
                    tuplet_error += Quantize::find_off_time_tuplet_quant_error(on_time, off_time, tuplet.len, tuplet_ratio, start_bar_tick);
                    regular_error += Quantize::find_off_time_quant_error(*chord_it, off_time, basic_quant);
                }
                if (tuplet_error < regular_error) {
                    tuplet.chords.insert({on_time, chord_it});
                    it = non_tuplets.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
}

#ifndef NDEBUG

bool do_tuplets_have_common_chords(const std::vector<TupletInfo> &tuplets) {
    if (tuplets.empty()) {
        return false;
    }
    std::set<std::pair<const ReducedFraction, MidiChord> *> chords_i;
    for (const auto &tuplet : tuplets) {
        for (const auto &chord : tuplet.chords) {
            if (chords_i.find(&*chord.second) != chords_i.end()) {
                return true;
            }
            chords_i.insert(&*chord.second);
        }
    }
    return false;
}

// check - do all tuplets have references from some chords

bool check_tuplets_for_existence(const std::multimap<ReducedFraction, MidiChord> &chords,
                                 const std::multimap<ReducedFraction, TupletData> &tuplet_events) {
    // check - do all tuplets have references from some chords
    std::map<const std::pair<const ReducedFraction, TupletData> *, bool> tuplet_map;
    for (const auto &tuplet : tuplet_events) {
        tuplet_map.insert({&tuplet, false});
    }

    for (const auto &chord : chords) {
        const MidiChord &c = chord.second;
        if (c.is_in_tuplet) {
            const auto it = tuplet_map.find(&*c.tuplet);
            if (it == tuplet_map.end()) {
                LOGD() << "Chords have references to non-existing tuplets";
                return false;
            }
        }
        for (const auto &note : c.notes) {
            if (note.is_in_tuplet) {
                const auto it = tuplet_map.find(&*note.tuplet);
                if (it == tuplet_map.end()) {
                    LOGD() << "Notes have references to non-existing tuplets";
                    return false;
                }
            }
        }
    }
    return true;
}

// check - do all tuplets have references from some chords

bool check_for_dangling_tuplets(const std::multimap<ReducedFraction, MidiChord> &chords,
                                const std::multimap<ReducedFraction, TupletData> &tuplet_events) {
    const ReducedFraction max_chord_length = MChord::find_max_chord_length(chords);

    for (auto tuplet_it = tuplet_events.begin(); tuplet_it != tuplet_events.end(); ++tuplet_it) {
        const auto &tuplet = tuplet_it->second;
        bool has_reference = false;
        auto chord_it = chords.lower_bound(tuplet.on_time + tuplet.len);

        if (chord_it != chords.begin()) {
            --chord_it;
            while (chord_it->first + max_chord_length > tuplet_it->first) {
                const MidiChord &c = chord_it->second;
                if (c.is_in_tuplet && c.tuplet == tuplet_it) {
                    has_reference = true;
                    break;
                }
                for (const auto &note : c.notes) {
                    if (note.is_in_tuplet && note.tuplet == tuplet_it) {
                        has_reference = true;
                        break;
                    }
                }
                if (has_reference || chord_it == chords.begin()) {
                    break;
                }
                --chord_it;
            }
        }

        if (!has_reference) {
            LOGD() << "Not all tuplets have references in chords - "
                      "there are dangling tuplets";
            return false;
        }
    }
    return true;
}

// check tuplet events for uniqueness

bool check_are_tuplets_unique(const std::multimap<ReducedFraction, TupletData> &tuplet_events) {
    std::set<std::pair<ReducedFraction, int>> referenced_tuplets; // <onTime, voice>
    for (const auto &tuplet : tuplet_events) {
        const auto &t = tuplet.second;
        const auto result = referenced_tuplets.insert({t.on_time, t.voice});

        if (!result.second) {
            LOGD() << "Not unique tuplets in tupletEvents";
            return false;
        }
    }
    return true;
}

// check is referenced tuplet count equal to tuplet event count

bool check_for_equal_tuplet_count(const std::multimap<ReducedFraction, MidiChord> &chords,
                                  const std::multimap<ReducedFraction, TupletData> &tuplet_events) {
    std::set<std::pair<ReducedFraction, int>> referenced_tuplets; // <onTime, voice>
    for (const auto &chord : chords) {
        if (chord.second.is_in_tuplet) {
            const auto &tuplet = chord.second.tuplet->second;
            referenced_tuplets.insert({tuplet.on_time, tuplet.voice});
        }
        for (const auto &note : chord.second.notes) {
            if (note.is_in_tuplet) {
                const auto &tuplet = note.tuplet->second;
                referenced_tuplets.insert({tuplet.on_time, tuplet.voice});
            }
        }
    }

    if (referenced_tuplets.size() < tuplet_events.size()) {
        LOGD() << "Referenced tuplets count (" << referenced_tuplets.size() << ") < tuplet events count (" << tuplet_events.size() << ")";
    }
    if (referenced_tuplets.size() > tuplet_events.size()) {
        LOGD() << "Referenced tuplets count (" << referenced_tuplets.size() << ") > tuplet events count (" << tuplet_events.size() << ")";
    }

    return tuplet_events.size() == referenced_tuplets.size();
}

bool are_all_tuplets_referenced(const std::multimap<ReducedFraction, MidiChord> &chords,
                                const std::multimap<ReducedFraction, TupletData> &tuplet_events) {
    if (!check_are_tuplets_unique(tuplet_events)) {
        return false;
    }
    if (!check_tuplets_for_existence(chords, tuplet_events)) {
        return false;
    }
    if (!check_for_dangling_tuplets(chords, tuplet_events)) {
        return false;
    }
    if (!check_for_equal_tuplet_count(chords, tuplet_events)) {
        return false;
    }
    return true;
}

// this check is not full but often if use invalid iterator for comparison
// it causes crash or something

bool are_tuplet_references_valid(const std::multimap<ReducedFraction, MidiChord> &chords) {
    for (const auto &chord : chords) {
        const MidiChord &c = chord.second;
        if (c.is_in_tuplet) {
            const auto &tuplet = c.tuplet->second;
            if (tuplet.on_time < ReducedFraction(0, 1) || tuplet.len < ReducedFraction(0, 1)) {
                return false;
            }
        }
        for (const auto &note : c.notes) {
            if (note.is_in_tuplet) {
                const auto &tuplet = note.tuplet->second;
                if (tuplet.on_time < ReducedFraction(0, 1) || tuplet.len < ReducedFraction(0, 1)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool are_tuplet_non_tuplet_chords_distinct(const std::vector<TupletInfo> &tuplets,
                                           const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets) {
    std::set<std::pair<const ReducedFraction, MidiChord> *> chords;
    for (const TupletInfo &tuplet : tuplets) {
        for (const auto &chord : tuplet.chords) {
            chords.insert(&*chord.second);
        }
    }
    for (const auto &non_tuplet : non_tuplets) {
        const auto it = chords.find(&*non_tuplet);
        if (it != chords.end()) {
            return false;
        }
    }
    return true;
}

bool is_tuplet_range_ok(const std::pair<const ReducedFraction, MidiChord> &chord,
                        const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets) {
    const MidiChord &c = chord.second;
    const auto found_tuplets = find_tuplets_for_time_range(c.voice, chord.first, ReducedFraction(0, 1), tuplets, false);
    if (c.is_in_tuplet && found_tuplets.empty()) {
        LOGD() << "Tuplet chord is actually outside tuplets, "
                  "bar number (from 1):"
               << (c.bar_index + 1);
        return false;
    }
    if (!c.is_in_tuplet && !found_tuplets.empty()) {
        // chord can touch the tuplet at the end and doesn't belong to it
        for (const auto &t : found_tuplets) {
            if (chord.first != t->second.on_time + t->second.len) {
                LOGD() << "Non-tuplet chord is actually inside tuplet, "
                          "bar number (from 1):"
                       << (c.bar_index + 1);
                return false;
            }
        }
    }
    for (const auto &note : c.notes) {
        const auto found_tuplets1 = find_tuplets_for_time_range(c.voice, note.off_time, ReducedFraction(0, 1), tuplets, false);
        if (note.is_in_tuplet && found_tuplets1.empty()) {
            LOGD() << "Tuplet note off time is actually outside tuplets, "
                      "bar number (from 1):"
                   << (c.bar_index + 1);
            return false;
        }
        if (!note.is_in_tuplet && !found_tuplets1.empty()) {
            // note off time can touch the tuplet
            // at the beg/end and doesn't belong to it
            for (const auto &t : found_tuplets1) {
                if (note.off_time != t->second.on_time && note.off_time != t->second.on_time + t->second.len) {
                    LOGD() << "Non-tuplet note off time is actually inside tuplet, "
                              "bar number (from 1):"
                           << (c.bar_index + 1);
                    return false;
                }
            }
        }
    }
    return true;
}

// should be called after quantization

bool are_tuplet_ranges_ok(const std::multimap<ReducedFraction, MidiChord> &chords,
                          const std::multimap<ReducedFraction, TupletData> &tuplets) {
    for (const auto &chord : chords) {
        if (!is_tuplet_range_ok(chord, tuplets)) {
            return false;
        }
    }
    return true;
}

bool are_all_tuplets_different(const std::multimap<ReducedFraction, TupletData> &tuplets) {
    std::set<std::pair<ReducedFraction, int>> tuplets_set; // <on time, voice>
    for (const auto &tuplet : tuplets) {
        const auto result = tuplets_set.insert({tuplet.first, tuplet.second.voice});
        const bool is_already_in_set = !result.second;
        if (is_already_in_set) {
            return false;
        }
    }
    return true;
}

#endif

void add_tuplet_events(std::multimap<ReducedFraction, TupletData> &tuplet_events, const std::vector<TupletInfo> &tuplets,
                       const std::list<TiedTuplet> &back_tied_tuplets) {
    for (size_t i = 0; i != tuplets.size(); ++i) {
        const auto &tuplet_info = tuplets[i];
        TupletData tuplet_data = {
            tuplet_info.chords.begin()->second->second.voice, tuplet_info.on_time, tuplet_info.len, tuplet_info.tuplet_number, {}};

        const auto it = tuplet_events.insert({tuplet_data.on_time, tuplet_data});
        for (auto &chord : tuplet_info.chords) {
            MidiChord &midi_chord = chord.second->second;
            midi_chord.tuplet = it;

            assert(!midi_chord.is_in_tuplet);

            midi_chord.is_in_tuplet = true;
        }

        for (const TiedTuplet &tied_tuplet : back_tied_tuplets) {
            if (tied_tuplet.tuplet_id == tuplet_info.id) {
                MidiChord &midi_chord = tied_tuplet.chord->second;

#ifndef NDEBUG
                muse::String message = u"Tied tuplet and tied chord have different voices, "
                                       "tuplet voice = ";
                message += muse::String::number(tied_tuplet.voice) + u", chord voice = ";
                message += muse::String::number(midi_chord.voice) + u", bar number (from 1) = ";
                message += muse::String::number(midi_chord.bar_index + 1);
                assert(tied_tuplet.voice == midi_chord.voice);
#endif

                for (int j : tied_tuplet.tied_note_indexes) {
                    midi_chord.notes[j].tuplet = it;
                    midi_chord.notes[j].is_in_tuplet = true;
                }
                break;
            }
        }

        for (auto &chord : tuplet_info.chords) {
            MidiChord &midi_chord = chord.second->second;
            for (auto &note : midi_chord.notes) {
                if (note.off_time <= tuplet_info.on_time + tuplet_info.len) {
                    assert(!note.is_in_tuplet);

                    note.tuplet = it;
                    note.is_in_tuplet = true;
                }
            }
        }
    }
}

void mark_staccato_tuplet_notes(std::vector<TupletInfo> &tuplets) {
    for (auto &tuplet : tuplets) {
        for (const auto &staccato : tuplet.staccato_chords) {
            const auto it = tuplet.chords.find(staccato.first);
            if (it != tuplet.chords.end()) {
                MidiChord &midi_chord = it->second->second;
                midi_chord.notes[staccato.second].staccato = true;
            }
        }
        tuplet.staccato_chords.clear();
    }
}

// if notes with staccato were in tuplets previously - remove staccato

void clean_staccato_of_non_tuplets(std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets) {
    for (auto &non_tuplet : non_tuplets) {
        for (auto &note : non_tuplet->second.notes) {
            if (note.staccato) {
                note.staccato = false;
            }
        }
    }
}

// chord on time shouldn't go before previous chord on time or after next chord on time
// this function finds such on time value between previous and next chords

ReducedFraction find_on_time_between_chords(const std::pair<const ReducedFraction, MidiChord> &chord,
                                            const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                                            const ReducedFraction &bar_start) {
    ReducedFraction on_time(-1, 1);
    auto quant = basic_quant;

    const auto range = chords.equal_range(chord.first);
    auto chord_it = chords.end();

    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.voice == chord.second.voice) {
            chord_it = it;
            break;
        }
    }

    assert(chord_it != chords.end());

    if (chord_it->first < bar_start) {
        return bar_start;
    }

    // chords with equal voices here can have equal on time values
    // so skip such chords
    const int voice = chord_it->second.voice;
    while (true) {
        on_time = Quantize::find_min_quantized_on_time(*chord_it, quant);
        bool changed = false;

        if (chord_it != chords.begin()) {
            auto it = std::prev(chord_it);
            while (true) {
                if (it->first < chord.first && it->second.voice == voice) {
                    const auto prev_chord_on_time = Quantize::find_min_quantized_on_time(*it, quant);
                    if (on_time < prev_chord_on_time) {
                        assert(quant >= MChord::min_allowed_duration() * 2);

                        quant /= 2;
                        changed = true;
                    }
                    break;
                }
                if (it == chords.begin() || it->first < chord.first - basic_quant * 2) {
                    break;
                }
                --it;
            }
        }

        if (!changed) {
            for (auto it = std::next(chord_it); it != chords.end() && it->first < chord.first + basic_quant * 2; ++it) {
                if (it->first == chord.first || it->second.voice != voice) {
                    continue;
                }
                const auto next_chord_on_time = Quantize::find_min_quantized_on_time(*it, quant);
                if (on_time > next_chord_on_time) {
                    assert(quant >= MChord::min_allowed_duration() * 2);

                    quant /= 2;
                    changed = true;
                }
                break;
            }
        }

        if (!changed) {
            break;
        }
    }

    assert(on_time != ReducedFraction(-1, 1));

    return on_time;
}

ReducedFraction find_prev_bar_start(const ReducedFraction &bar_start, const ReducedFraction &bar_len) {
    auto prev_bar_start = bar_start - bar_len;
    if (prev_bar_start < ReducedFraction(0, 1)) {
        prev_bar_start = ReducedFraction(0, 1);
    }
    return prev_bar_start;
}

void set_bar_indexes_of_next_bar_chords(std::vector<TupletInfo> &tuplets,
                                        std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets, int bar_index) {
    for (auto &tuplet : tuplets) {
        for (auto &chord : tuplet.chords) {
            if (chord.second->second.bar_index > bar_index) {
                chord.second->second.bar_index = bar_index;
            }
        }
    }
    for (auto &chord : non_tuplets) {
        if (chord->second.bar_index > bar_index) {
            chord->second.bar_index = bar_index;
        }
    }
}

// indexes of each new bar should be -1 except possibly chords at the end of prev bar

void find_tuplets(const std::multimap<ReducedFraction, MidiChord>::iterator &start_bar_chord_it,
                  const std::multimap<ReducedFraction, MidiChord>::iterator &end_bar_chord_it,
                  std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                  std::multimap<ReducedFraction, TupletData> &tuplet_events, const engraving::TimeSigMap *sigmap, int bar_index) {
    if (chords.empty() || start_bar_chord_it == end_bar_chord_it) {
        return;
    }

    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();
    if (!opers.search_tuplets.value(current_track)) {
        return;
    }

    const auto start_bar_tick = ReducedFraction::from_ticks(sigmap->bar2tick(start_bar_chord_it->second.bar_index, 0));
    const auto end_bar_tick = ReducedFraction::from_ticks(sigmap->bar2tick(start_bar_chord_it->second.bar_index + 1, 0));

    const auto bar_fraction = ReducedFraction(sigmap->timesig(start_bar_tick.ticks()).timesig());
    std::vector<TupletInfo> tuplets =
        detect_tuplets(start_bar_chord_it, end_bar_chord_it, start_bar_tick, bar_fraction, chords, basic_quant, bar_index);
    if (tuplets.empty()) {
        return;
    }

    filter_tuplets(tuplets, basic_quant);
    // later notes will be sorted and their indexes become invalid
    // so assign staccato information to notes now
    if (opers.simplify_durations.value(current_track)) {
        mark_staccato_tuplet_notes(tuplets);
    }

    auto non_tuplets = find_non_tuplet_chords(tuplets, start_bar_chord_it, end_bar_chord_it);
    add_chords_between_tuplet_notes(tuplets, non_tuplets, chords, start_bar_tick, basic_quant);
    sort_notes_by_pitch(start_bar_chord_it, end_bar_chord_it);
    sort_tuplets_by_average_pitch(tuplets);

    if (tuplet_voice_limit() > 1) {
        split_first_tuplet_chords(tuplets, chords);
        minimize_off_time_error(tuplets, chords, non_tuplets, start_bar_tick, basic_quant);
    }

    if (opers.simplify_durations.value(current_track)) {
        clean_staccato_of_non_tuplets(non_tuplets);
    }
#ifndef NDEBUG
    assert(!do_tuplets_have_common_chords(tuplets));
#endif
    const auto prev_bar_start = find_prev_bar_start(start_bar_tick, end_bar_tick - start_bar_tick);
    auto back_tied_tuplets =
        find_back_tied_tuplets(chords, tuplets, prev_bar_start, start_bar_tick, basic_quant, start_bar_chord_it->second.bar_index);
    // backTiedTuplets can be changed here (incompatible are removed)
    assign_voices(tuplets, non_tuplets, back_tied_tuplets, chords, basic_quant, start_bar_tick, bar_index);
#ifndef NDEBUG
    assert(are_tuplet_non_tuplet_chords_distinct(tuplets, non_tuplets));
#endif
    add_tuplet_events(tuplet_events, tuplets, back_tied_tuplets);
    set_bar_indexes_of_next_bar_chords(tuplets, non_tuplets, bar_index);
}

void set_all_tuplet_off_times(std::multimap<ReducedFraction, TupletData> &tuplet_events, std::multimap<ReducedFraction, MidiChord> &chords,
                              const engraving::TimeSigMap *sigmap) {
    for (auto &chord_event : chords) {
        MidiChord &chord = chord_event.second;
        for (MidiNote &note : chord.notes) {
            if (note.is_in_tuplet) {
                continue;
            }
            const auto bar_end = ReducedFraction::from_ticks(sigmap->bar2tick(chord.bar_index + 1, 0));
            if (note.off_time > bar_end) {
                const auto it = find_tuplet_containing_time(chord.voice, note.off_time, tuplet_events, true);
                if (it != tuplet_events.end()) {
                    note.is_in_tuplet = true;
                    // hack to remove constness of iterator
                    note.tuplet = tuplet_events.erase(it, it);
                }
            }
        }
    }
}

void find_all_tuplets(std::multimap<ReducedFraction, TupletData> &tuplets, std::multimap<ReducedFraction, MidiChord> &chords,
                      const engraving::TimeSigMap *sigmap, const ReducedFraction &basic_quant) {
    if (chords.empty()) {
        return;
    }
#ifndef NDEBUG
    assert(MChord::are_notes_long_enough(chords));
    assert(MChord::are_bar_indexes_set(chords));
    assert(MChord::are_bar_indexes_successive(chords));
#endif
    {
        auto start_bar_it = chords.begin();
        for (auto end_bar_it = std::next(start_bar_it); end_bar_it != chords.end(); ++end_bar_it) {
            assert(end_bar_it->second.bar_index >= start_bar_it->second.bar_index);

            const int current_bar_index = start_bar_it->second.bar_index;
            if (end_bar_it->second.bar_index > current_bar_index) {
                const size_t old_tuplet_count = tuplets.size();
                find_tuplets(start_bar_it, end_bar_it, chords, basic_quant, tuplets, sigmap, current_bar_index);

                assert(tuplets.size() >= old_tuplet_count);

                if (tuplets.size() > old_tuplet_count) { // new tuplets were found
                                                         // chords at the end of the current bar
                                                         // may have changed bar index - from next bar to the current bar
                                                         // because they were included in tuplets
#ifndef NDEBUG
                    bool next_bar_found = false;
#endif
                    const auto end_bar_tick = ReducedFraction::from_ticks(sigmap->bar2tick(current_bar_index + 1, 0));
                    for (auto it = end_bar_it; it != chords.end() && it->first < end_bar_tick; ++it) {
                        if (it->second.bar_index == current_bar_index) {
#ifndef NDEBUG
                            assert(!next_bar_found);
#endif
                            assert(end_bar_it != chords.end());

                            ++end_bar_it;
                        }
#ifndef NDEBUG
                        if (it->second.bar_index > current_bar_index) {
                            next_bar_found = true;
                        }
#endif
                    }
                }

                start_bar_it = end_bar_it;
                if (end_bar_it == chords.end()) {
                    break;
                }
            }
        }
        // handle the last bar containing chords
        find_tuplets(start_bar_it, chords.end(), chords, basic_quant, tuplets, sigmap, start_bar_it->second.bar_index);
    }
    // check if there are not detected off times inside tuplets
    set_all_tuplet_off_times(tuplets, chords, sigmap);
#ifndef NDEBUG
    assert(are_all_tuplets_referenced(chords, tuplets));
    assert(MChord::are_notes_long_enough(chords));
    assert(are_all_tuplets_different(tuplets));
#endif
}
}} // namespace mu::iex::midi::MidiTuplet
