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
#include "importmidi_tuplet_detect.h"

#include <algorithm>
#include <set>

#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_quant.h"
#include "importmidi_tuplet.h"

namespace mu::iex::midi { namespace MidiTuplet {
bool is_tuplet_allowed(const TupletInfo &tuplet_info) {
    {
        // special check for duplets and triplets
        const std::vector<int> nums = {2, 3};
        // for duplet: if note first and single - only 1/2*tupletLen duration is allowed
        // for triplet: if note first and single - only 1/3*tupletLen duration is allowed
        for (int num : nums) {
            if (tuplet_info.tuplet_number == num && tuplet_info.chords.size() == 1 && tuplet_info.first_chord_index == 0) {
                const auto &chord_event_it = tuplet_info.chords.begin()->second;
                const auto tuplet_note_len = tuplet_info.len / num;
                for (const auto &note : chord_event_it->second.notes) {
                    if ((note.off_time - chord_event_it->first - tuplet_note_len).abs_value() > tuplet_note_len / 2) {
                        return false;
                    }
                }
            }
        }
    }
    // for all tuplets
    const auto &opers = midi_import_operations.data()->track_opers;
    const int min_allowed_note_count = (opers.is_human_performance.value()) ?
                                           tuplet_limits(tuplet_info.tuplet_number).min_note_count_human :
                                           tuplet_limits(tuplet_info.tuplet_number).min_note_count;
    if ((int) tuplet_info.chords.size() < min_allowed_note_count) {
        return false;
    }
    // allow duplets and quadruplets with error == regular error
    // because tuplet notation is simpler in that case
    if (tuplet_info.tuplet_number == 2 || tuplet_info.tuplet_number == 4) {
        if (tuplet_info.tuplet_sum_error > tuplet_info.regular_sum_error) {
            return false;
        }
    } else {
        if (tuplet_info.tuplet_sum_error >= tuplet_info.regular_sum_error) {
            return false;
        }
    }
    // at least one note has to have len >= (half tuplet note len)
    const auto tuplet_note_len = tuplet_info.len / tuplet_info.tuplet_number;
    for (const auto &tuplet_chord : tuplet_info.chords) {
        for (const auto &note : tuplet_chord.second->second.notes) {
            if (note.off_time - tuplet_chord.first >= tuplet_note_len / 2) {
                return true;
            }
        }
    }
    return false;
}

std::vector<int> find_tuplet_numbers(const ReducedFraction &div_len, const ReducedFraction &bar_fraction) {
    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();
    std::vector<int> tuplet_numbers;

    if (Meter::is_compound(bar_fraction) && div_len == Meter::beat_length(bar_fraction)) {
        if (opers.search2plets.value(current_track)) {
            tuplet_numbers.push_back(2);
        }
        if (opers.search4plets.value(current_track)) {
            tuplet_numbers.push_back(4);
        }
    } else {
        if (opers.search3plets.value(current_track)) {
            tuplet_numbers.push_back(3);
        }
        if (opers.search5plets.value(current_track)) {
            tuplet_numbers.push_back(5);
        }
        if (opers.search7plets.value(current_track)) {
            tuplet_numbers.push_back(7);
        }
        if (opers.search9plets.value(current_track)) {
            tuplet_numbers.push_back(9);
        }
    }

    return tuplet_numbers;
}

// find sum length of gaps between successive chords
// less is better

ReducedFraction find_sum_length_of_rests(const TupletInfo &tuplet_info, const ReducedFraction &start_bar_tick) {
    auto beg = tuplet_info.on_time;
    const auto tuplet_end_time = tuplet_info.on_time + tuplet_info.len;
    const auto tuplet_note_len = tuplet_info.len / tuplet_info.tuplet_number;
    ReducedFraction sum_len = {0, 1};

    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();

    for (const auto &chord : tuplet_info.chords) {
        const auto staccato_it = (opers.simplify_durations.value(current_track)) ? tuplet_info.staccato_chords.find(chord.first) :
                                                                                   tuplet_info.staccato_chords.end();

        const MidiChord &midi_chord = chord.second->second;
        const auto &chord_on_time = (chord.second->first < start_bar_tick) ?
                                        start_bar_tick :
                                        Quantize::find_quantized_tuplet_chord_on_time(
                                            *chord.second, tuplet_info.len, tuplet_limits(tuplet_info.tuplet_number).ratio, start_bar_tick);
        if (beg < chord_on_time) {
            sum_len += (chord_on_time - beg);
        }
        ReducedFraction max_off_time(0, 1);
        for (int i = 0; i != midi_chord.notes.size(); ++i) {
            auto note_off_time = midi_chord.notes[i].off_time;
            if (staccato_it != tuplet_info.staccato_chords.end() && i == staccato_it->second) {
                note_off_time = chord_on_time + tuplet_note_len;
            }
            if (note_off_time > max_off_time) {
                max_off_time = note_off_time;
            }
        }
        beg = Quantize::find_quantized_tuplet_note_off_time(chord.first, max_off_time, tuplet_info.len,
                                                            tuplet_limits(tuplet_info.tuplet_number).ratio, start_bar_tick)
                  .first;
        if (beg >= tuplet_end_time) {
            break;
        }
    }
    if (beg < tuplet_end_time) {
        sum_len += (tuplet_end_time - beg);
    }
    return sum_len;
}

TupletInfo find_tuplet_approximation(const ReducedFraction &tuplet_len, int tuplet_number, const ReducedFraction &basic_quant,
                                     const ReducedFraction &start_tuplet_time,
                                     const std::multimap<ReducedFraction, MidiChord>::iterator &start_chord_it,
                                     const std::multimap<ReducedFraction, MidiChord>::iterator &end_chord_it) {
    TupletInfo tuplet_info;
    tuplet_info.tuplet_number = tuplet_number;
    tuplet_info.on_time = start_tuplet_time;
    tuplet_info.len = tuplet_len;
    const auto tuplet_note_len = tuplet_len / tuplet_number;

    struct Error {
        bool operator<(const Error &e) const {
            if (tuplet_error < e.tuplet_error) {
                return true;
            }
            if (tuplet_error > e.tuplet_error) {
                return false;
            }
            return tuplet_regular_diff < e.tuplet_regular_diff;
        }

        ReducedFraction tuplet_error;
        ReducedFraction tuplet_regular_diff;
    };

    struct Candidate {
        int pos_index;
        std::multimap<ReducedFraction, MidiChord>::iterator chord;
        ReducedFraction regular_error;
    };

    std::multimap<Error, Candidate> chord_candidates;

    for (int pos_index = 0; pos_index != tuplet_number; ++pos_index) {
        const auto tuplet_note_pos = start_tuplet_time + tuplet_note_len * pos_index;
        for (auto it = start_chord_it; it != end_chord_it; ++it) {
            if (it->first < tuplet_note_pos - tuplet_note_len / 2) {
                continue;
            }
            if (it->first > tuplet_note_pos + tuplet_note_len / 2) {
                break;
            }

            const auto tuplet_error = (it->first - tuplet_note_pos).abs_value();
            const auto regular_error = Quantize::find_on_time_quant_error(*it, basic_quant);
            const auto diff = tuplet_error - regular_error;
            chord_candidates.insert({{tuplet_error, diff}, {pos_index, it, regular_error}});
        }
    }

    std::set<std::pair<const ReducedFraction, MidiChord> *> used_chords;
    std::set<int> used_pos_indexes;
    ReducedFraction diff_sum;
    int first_chord_index = 0;

    for (const auto &candidate : chord_candidates) {
        if (diff_sum + candidate.first.tuplet_regular_diff > ReducedFraction(0, 1)) {
            break;
        }
        const Candidate &c = candidate.second;
        if (used_pos_indexes.find(c.pos_index) != used_pos_indexes.end()) {
            continue;
        }
        if (used_chords.find(&*c.chord) != used_chords.end()) {
            continue;
        }

        used_chords.insert(&*c.chord);
        used_pos_indexes.insert(c.pos_index);
        diff_sum += candidate.first.tuplet_regular_diff;
        tuplet_info.chords.insert({c.chord->first, c.chord});
        tuplet_info.tuplet_sum_error += candidate.first.tuplet_error;
        tuplet_info.regular_sum_error += c.regular_error;
        // if chord was inserted to the beginning - remember its pos index
        if (c.chord->first == tuplet_info.chords.begin()->first) {
            first_chord_index = c.pos_index;
        }
    }

    tuplet_info.first_chord_index = first_chord_index;

    return tuplet_info;
}

// detect staccato notes; later sum length of rests of this notes
// will be reduced by enlarging the length of notes to the tuplet note length

void detect_staccato(TupletInfo &tuplet) {
    if ((int) tuplet.chords.size() >= tuplet_limits(tuplet.tuplet_number).min_note_count_staccato) {
        const auto tuplet_note_len = tuplet.len / tuplet.tuplet_number;
        for (auto &chord : tuplet.chords) {
            MidiChord &midi_chord = chord.second->second;
            for (int i = 0; i != midi_chord.notes.size(); ++i) {
                if (midi_chord.notes[i].off_time - chord.first < tuplet_note_len / 2) {
                    // later if chord have one or more notes
                    // with staccato -> entire chord is staccato

                    // don't mark note as staccato here, only remember it
                    // because different tuplets may contain this note,
                    // it will be resolved after tuplet filtering
                    tuplet.staccato_chords.insert({chord.first, i});
                }
            }
        }
    }
}

// this function is needed because if there are additional chords
// that can be in the middle between tuplet chords,
// and tuplet chords are staccato, i.e. have short length,
// then tied staccato chords would be not pretty-looked converted to notation

bool have_chords_in_the_middle_between_tuplet_chords(const std::multimap<ReducedFraction, MidiChord>::iterator start_div_chord_it,
                                                     const std::multimap<ReducedFraction, MidiChord>::iterator end_div_chord_it,
                                                     const TupletInfo &tuplet) {
    std::set<std::pair<const ReducedFraction, MidiChord> *> tuplet_chords;
    for (const auto &chord : tuplet.chords) {
        tuplet_chords.insert(&*chord.second);
    }

    const auto tuplet_note_len = tuplet.len / tuplet.tuplet_number;
    for (auto it = start_div_chord_it; it != end_div_chord_it; ++it) {
        if (tuplet_chords.find(&*it) != tuplet_chords.end()) {
            continue;
        }
        for (int i = 0; i != tuplet.tuplet_number; ++i) {
            const auto pos = tuplet.on_time + tuplet_note_len * i + tuplet_note_len / 2;
            if ((pos - it->first).abs_value() < tuplet_note_len / 2) {
                return true;
            }
        }
    }
    return false;
}

bool is_tuplet_len_allowed(const ReducedFraction &tuplet_len, int tuplet_number,
                           const std::multimap<ReducedFraction, MidiChord>::const_iterator beg,
                           const std::multimap<ReducedFraction, MidiChord>::const_iterator end, const ReducedFraction &basic_quant) {
    const auto tuplet_note_len = tuplet_len / tuplet_number;
    const auto regular_quant = Quantize::find_quant_for_range(beg, end, basic_quant);
    return tuplet_note_len >= regular_quant;
}

// only one chord from the next bar can be used to search tuplets
// and this chord should be the last in the current bar

bool is_next_bar_ownership_ok(const std::multimap<ReducedFraction, MidiChord>::iterator &start_it,
                              const std::multimap<ReducedFraction, MidiChord>::iterator &end_it,
                              const std::multimap<ReducedFraction, MidiChord> &chords, int bar_index) {
    if (end_it == chords.begin() || end_it == start_it) {
        return false;
    }
    if (std::prev(end_it)->second.bar_index == bar_index) {
        return true;
    }

    int next_bar_counter = 0;
    for (auto it = std::prev(end_it);; --it) {
        if (it->second.bar_index > bar_index) {
            ++next_bar_counter;
        }
        if (it == start_it) {
            break;
        }
    }
    return next_bar_counter == 1;
}

std::vector<TupletInfo> detect_tuplets(const std::multimap<ReducedFraction, MidiChord>::iterator &start_bar_chord_it,
                                       const std::multimap<ReducedFraction, MidiChord>::iterator &end_bar_chord_it,
                                       const ReducedFraction &start_bar_tick, const ReducedFraction &bar_fraction,
                                       std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                                       int bar_index) {
    const auto div_lengths = Meter::divisions_of_bar_for_tuplets(bar_fraction);

    std::vector<TupletInfo> tuplets;
    int id = 0;
    const auto tol = basic_quant / 2;

    for (const auto &div_len : div_lengths) {
        const auto tuplet_numbers = find_tuplet_numbers(div_len, bar_fraction);
        const auto div = bar_fraction / div_len;
        const int div_count = div.numerator() / div.denominator();

        for (int i = 0; i != div_count; ++i) {
            const auto start_div_time = start_bar_tick + div_len * i;
            const auto end_div_time = start_bar_tick + div_len * (i + 1);
            // check which chords can be inside tuplet period
            // [startDivTime - tol, endDivTime]
            const auto start_div_time_with_tol = std::max(start_bar_tick, start_div_time - tol);
            auto start_div_chord_it =
                MChord::find_first_chord_in_range(start_div_time_with_tol, end_div_time, start_bar_chord_it, end_bar_chord_it);
            if (start_div_chord_it == end_bar_chord_it) {
                continue;
            }

            assert(!start_div_chord_it->second.is_in_tuplet);

            // end iterator, as usual, point to the next - invalid chord
            auto end_div_chord_it = chords.lower_bound(end_div_time);
            if (!is_next_bar_ownership_ok(start_div_chord_it, end_div_chord_it, chords, bar_index)) {
                continue;
            }
            // try different tuplets, nested tuplets are not allowed
            // here chords from next bar can be captured
            // if their on time < next bar start
            for (const auto &tuplet_number : tuplet_numbers) {
                if (!is_tuplet_len_allowed(div_len, tuplet_number, start_div_chord_it, end_div_chord_it, basic_quant)) {
                    continue;
                }
                auto tuplet_info =
                    find_tuplet_approximation(div_len, tuplet_number, basic_quant, start_div_time, start_div_chord_it, end_div_chord_it);

                const auto &opers = midi_import_operations.data()->track_opers;
                const int current_track = midi_import_operations.current_track();

                if (opers.simplify_durations.value(current_track)) {
                    if (!have_chords_in_the_middle_between_tuplet_chords(start_div_chord_it, end_div_chord_it, tuplet_info)) {
                        detect_staccato(tuplet_info);
                    }
                }
                tuplet_info.sum_length_of_rests = find_sum_length_of_rests(tuplet_info, start_bar_tick);

                if (!is_tuplet_allowed(tuplet_info)) {
                    continue;
                }
                tuplet_info.id = id++;
                tuplets.push_back(tuplet_info); // tuplet found
            } // next tuplet type
        }
    }

    return tuplets;
}
}} // namespace mu::iex::midi::MidiTuplet
