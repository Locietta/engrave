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
#include "importmidi_beat.h"

#include <cmath>
#include <functional>
#include <vector>

#include "thirdparty/beatroot/BeatTracker.h"

#include "engraving/dom/mscore.h"
#include "engraving/dom/sig.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_quant.h"
#include "importmidi_tempo.h"

namespace mu::iex::midi { namespace MidiBeat {
int beats_in_bar(const ReducedFraction &bar_fraction) {
    const auto beat_len = Meter::beat_length(bar_fraction);
    const auto div = bar_fraction / beat_len;
    return div.numerator() / div.denominator();
}

double find_chord_salience1(const std::pair<const ReducedFraction, MidiChord> &chord, double ticks_per_sec) {
    ReducedFraction duration(0, 1);
    int pitch = std::numeric_limits<int>::max();
    int velocity = 0;

    for (const MidiNote &note : chord.second.notes) {
        if (note.off_time - chord.first > duration) {
            duration = note.off_time - chord.first;
        }
        if (note.pitch < pitch) {
            pitch = note.pitch;
        }
        velocity += note.velo;
    }
    const double duration_in_seconds = duration.ticks() / ticks_per_sec;

    const double c4 = 84;
    const int pmin = 48;
    const int pmax = 72;

    if (pitch < pmin) {
        pitch = pmin;
    } else if (pitch > pmax) {
        pitch = pmax;
    }

    if (velocity <= 0) {
        velocity = 1;
    }

    return duration_in_seconds * (c4 - pitch) * log(velocity);
}

double find_chord_salience2(const std::pair<const ReducedFraction, MidiChord> &chord, double) {
    int velocity = 0;
    for (const MidiNote &note : chord.second.notes) {
        velocity += note.velo;
    }
    if (velocity <= 0) {
        velocity = 1;
    }

    return velocity;
}

::EventList
prepare_chord_events(const std::multimap<ReducedFraction, MidiChord> &chords,
                     const std::function<double(const std::pair<const ReducedFraction, MidiChord> &, double)> &find_chord_salience,
                     double ticks_per_sec) {
    ::EventList events;
    double min_salience = std::numeric_limits<double>::max();
    for (const auto &chord : chords) {
        ::Event e;
        e.time = chord.first.ticks() / ticks_per_sec;
        e.salience = find_chord_salience(chord, ticks_per_sec);
        if (e.salience < min_salience) {
            min_salience = e.salience;
        }
        events.push_back(e);
    }
    // all saliences should be non-negative
    if (min_salience < 0) {
        for (auto &e : events) {
            e.salience -= min_salience;
        }
    }

    return events;
}

ReducedFraction find_last_chord_tick(const std::multimap<ReducedFraction, MidiChord> &chords) {
    ReducedFraction last_off_time(0, 1);
    for (const auto &chord : chords) {
        for (const auto &note : chord.second.notes) {
            if (note.off_time > last_off_time) {
                last_off_time = note.off_time;
            }
        }
    }
    return last_off_time;
}

// first beat time can be larger than first chord onTime
// so insert additional beats at the beginning to cover all chords

void add_first_beats(std::set<ReducedFraction> &beat_set, const ReducedFraction &first_tick, int beats_in_bar, int &added_beat_count) {
    if (beat_set.empty()) {
        return;
    }

    added_beat_count = 0;
    auto first_beat = *beat_set.begin();
    if (first_tick < first_beat) {
        if (beat_set.size() > 1) {
            const auto beat_len = *std::next(beat_set.begin()) - first_beat;
            do {
                first_beat -= beat_len;
                beat_set.insert(first_beat);
                ++added_beat_count;
            } while (first_beat > first_tick || added_beat_count % beats_in_bar);
        }
    }
}

// last beat time can be smaller than the last chord onTime
// so insert additional beats at the end to cover all chords

void add_last_beats(std::set<ReducedFraction> &beat_set, const ReducedFraction &last_tick, int beats_in_bar, int &added_beat_count) {
    if (beat_set.empty()) {
        return;
    }

    added_beat_count = 0;
    // theoretically it's possible that every chord have off time
    // at the end of the piece - so check all chords for max off time
    auto last_beat = *(std::prev(beat_set.end()));
    if (last_tick > last_beat) {
        if (beat_set.size() > 1) {
            const auto beat_len = last_beat - *std::prev(beat_set.end(), 2);
            do {
                last_beat += beat_len;
                beat_set.insert(last_beat);
                ++added_beat_count;
            } while (last_beat < last_tick || added_beat_count % beats_in_bar);
        }
    }
}

MidiOperations::HumanBeatData prepare_human_beat_data(const std::vector<double> &beat_times,
                                                      const std::multimap<ReducedFraction, MidiChord> &chords, double ticks_per_sec,
                                                      int beats_in_bar) {
    MidiOperations::HumanBeatData beat_data;
    if (chords.empty()) {
        return beat_data;
    }

    for (const auto &beat_time : beat_times) {
        beat_data.beat_set.insert(MidiTempo::time2_tick(beat_time, ticks_per_sec));
    }

    beat_data.first_chord_tick = chords.begin()->first;
    beat_data.last_chord_tick = find_last_chord_tick(chords);

    add_first_beats(beat_data.beat_set, beat_data.first_chord_tick, beats_in_bar, beat_data.added_first_beats);
    add_last_beats(beat_data.beat_set, beat_data.last_chord_tick, beats_in_bar, beat_data.added_last_beats);

    return beat_data;
}

double find_match_rank(const std::set<ReducedFraction> &beat_set, const ::EventList &events, const std::vector<int> &levels,
                       int beats_in_bar, double ticks_per_sec) {
    std::map<ReducedFraction, double> saliences;
    for (const auto &e : events) {
        saliences.insert({MidiTempo::time2_tick(e.time, ticks_per_sec), e.salience});
    }
    std::vector<ReducedFraction> beats_of_bar;
    double match_frac = 0;
    int match_count = 0;
    int beat_count = 0;

    for (const auto &beat : beat_set) {
        beats_of_bar.push_back(beat);
        ++beat_count;
        if (beat_count == beats_in_bar) {
            beat_count = 0;
            int relation_count = 0;
            int relation_matches = 0;
            for (size_t i = 0; i != beats_of_bar.size() - 1; ++i) {
                const auto s1 = saliences.find(beats_of_bar[i]);
                for (size_t j = i + 1; j != beats_of_bar.size(); ++j) {
                    ++relation_count; // before s1 search check
                    if (s1 == saliences.end()) {
                        continue;
                    }
                    const auto s2 = saliences.find(beats_of_bar[j]);
                    if (s2 == saliences.end()) {
                        continue;
                    }
                    if ((s1->second < s2->second) == (levels[i] < levels[j])) {
                        ++relation_matches;
                    }
                }
            }
            if (relation_count) {
                match_frac += relation_matches * 1.0 / relation_count;
                ++match_count;
            }
            beats_of_bar.clear();
        }
    }
    if (match_count) {
        match_frac /= match_count;
    }

    return match_frac;
}

void remove_every2nd_beat(std::set<ReducedFraction> &beat_set) {
    auto it = beat_set.begin();
    while (it != beat_set.end() && std::next(it) != beat_set.end()) {
        it = beat_set.erase(std::next(it));
    }
    if (it == beat_set.end()) {
        // insert additional beat at the end to cover all chords
        const auto beat_len = *std::prev(it) - *std::prev(it, 2);
        beat_set.insert(*std::prev(it) + beat_len);
    }
}

// we can use ReducedFraction for time signature (bar fraction)
// because it reduces itself only if numerator or denominator
// is greater than some big number (see ReducedFraction class definition)

std::vector<ReducedFraction> find_time_signatures(const ReducedFraction &time_sig_from_midi_file) {
    std::vector<ReducedFraction> fractions{ReducedFraction(4, 4), ReducedFraction(3, 4)};
    bool match = false;
    for (const ReducedFraction &f : fractions) {
        if (f.is_identical_to(time_sig_from_midi_file)) {
            match = true;
            break;
        }
    }
    if (!match) { // some special time sig in MIDI file - use only it
        fractions.clear();
        fractions.push_back(time_sig_from_midi_file);
    }
    return fractions;
}

void set_time_sig(engraving::TimeSigMap *sigmap, const ReducedFraction &time_sig) {
    sigmap->clear();
    sigmap->add(0, time_sig.fraction());
}

void find_beat_locations(const std::multimap<ReducedFraction, MidiChord> &all_chords, engraving::TimeSigMap *sigmap, double ticks_per_sec) {
    const size_t min_beat_count = 8;
    const auto bar_fractions = find_time_signatures(ReducedFraction(sigmap->timesig(0).timesig()));
    const std::vector<std::function<double(const std::pair<const ReducedFraction, MidiChord> &, double)>> salience_funcs = {
        find_chord_salience1, find_chord_salience2};

    // <match rank, beat data, comparator>
    std::map<double, MidiOperations::HumanBeatData, std::greater<double>> beat_results;

    for (const auto &func : salience_funcs) {
        const auto events = prepare_chord_events(all_chords, func, ticks_per_sec);
        const auto beat_times = BeatTracker::beatTrack(events);
        if (beat_times.size() <= min_beat_count) {
            continue;
        }

        for (const ReducedFraction &bar_fraction : bar_fractions) {
            const auto beat_len = Meter::beat_length(bar_fraction);
            const auto div = bar_fraction / beat_len;
            const int beats_in_bar = div.numerator() / div.denominator();

            const std::vector<Meter::DivisionInfo> divs_info = {Meter::metric_divisions_of_bar(bar_fraction)};
            const auto levels = Meter::metric_levels_of_bar(bar_fraction, divs_info, beat_len);

            Q_ASSERT_X((int) levels.size() == beats_in_bar, "MidiBeat::findBeatLocations", "Wrong count of bar levels");

            // beat set - first case
            MidiOperations::HumanBeatData beat_data = prepare_human_beat_data(beat_times, all_chords, ticks_per_sec, beats_in_bar);
            beat_data.time_sig = bar_fraction;
            const double match_rank = find_match_rank(beat_data.beat_set, events, levels, beats_in_bar, ticks_per_sec);
            beat_results.insert({match_rank, beat_data});
        }
    }

    auto *data = midi_import_operations.data();
    if (!beat_results.empty()) {
        const MidiOperations::HumanBeatData &beat_data = beat_results.begin()->second;
        set_time_sig(sigmap, beat_data.time_sig);
        data->human_beat_data = beat_data;
        data->track_opers.measure_count2x_less.set_default_value(beat_data.measure_count2x_less);
        data->track_opers.time_sig_numerator.set_default_value(Meter::fraction_numerator_to_user_value(beat_data.time_sig.numerator()));
        data->track_opers.time_sig_denominator.set_default_value(
            Meter::fraction_denominator_to_user_value(beat_data.time_sig.denominator()));
    } else {
        const auto current_time_sig = ReducedFraction(sigmap->timesig(0).timesig());
        data->track_opers.time_sig_numerator.set_default_value(Meter::fraction_numerator_to_user_value(current_time_sig.numerator()));
        data->track_opers.time_sig_denominator.set_default_value(Meter::fraction_denominator_to_user_value(current_time_sig.denominator()));
    }
}

void scale_off_times(std::vector<MidiNote> &notes, const std::set<ReducedFraction> &beats,
                     const std::set<ReducedFraction>::const_iterator &on_time_beat_end_it, const ReducedFraction &new_on_time_beat_start,
                     const ReducedFraction &new_beat_len) {
    for (auto &note : notes) {
        int beat_count = 0; // beat count between note on time and off time

        Q_ASSERT_X(on_time_beat_end_it != beats.begin(), "MidiBeat::scaleOffTimes", "End beat iterator cannot be the first beat iterator");

        auto b_start = *std::prev(on_time_beat_end_it);
        for (auto bit = on_time_beat_end_it; bit != beats.end(); ++bit) {
            const auto &b_end = *bit;

            Q_ASSERT_X(b_end > b_start, "MidiBeat::scaleOffTimes", "Beat end <= beat start for note off time that is incorrect");

            if (note.off_time >= b_start && note.off_time < b_end) {
                const auto scale = new_beat_len / (b_end - b_start);
                auto new_off_time_in_beat = (note.off_time - b_start) * scale;
                new_off_time_in_beat = Quantize::quantize_value(new_off_time_in_beat, MChord::min_allowed_duration());
                const auto desired_beat_start = new_on_time_beat_start + new_beat_len * beat_count;
                note.off_time = desired_beat_start + new_off_time_in_beat;
                break;
            }

            b_start = b_end;
            ++beat_count;
        }
    }
}

void adjust_chords_to_beats(std::multimap<int, MTrack> &tracks) {
    const auto &opers = midi_import_operations;
    std::set<ReducedFraction> beats = opers.data()->human_beat_data.beat_set; // copy
    if (beats.empty()) {
        return;
    }

    if (opers.data()->track_opers.is_human_performance.value()) {
        if (opers.data()->track_opers.measure_count2x_less.value()) {
            remove_every2nd_beat(beats);
        }

        Q_ASSERT_X(beats.size() > 1, "MidiBeat::adjustChordsToBeats", "Human beat count < 2");

        const auto new_beat_len = ReducedFraction::from_ticks(engraving::Constants::division);

        for (auto track_it = tracks.begin(); track_it != tracks.end(); ++track_it) {
            auto &chords = track_it->second.chords;
            if (chords.empty()) {
                continue;
            }
            // do chord alignment according to recognized beats
            std::multimap<ReducedFraction, MidiChord> new_chords;
            auto chord_it = chords.begin();
            auto it = beats.begin();
            auto beat_start = *it;
            auto new_beat_start = ReducedFraction(0, 1);

            for (++it; it != beats.end(); ++it) {
                const auto &beat_end = *it;

                Q_ASSERT_X(beat_end > beat_start, "MidiBeat::adjustChordsToBeats", "Beat end <= beat start that is incorrect");

                const auto scale = new_beat_len / (beat_end - beat_start);

                for (; chord_it != chords.end() && chord_it->first < beat_end; ++chord_it) {
                    auto new_on_time_in_beat = (chord_it->first - beat_start) * scale;
                    // quantize to prevent ReducedFraction overflow
                    new_on_time_in_beat = Quantize::quantize_value(new_on_time_in_beat, MChord::min_allowed_duration());
                    scale_off_times(chord_it->second.notes, beats, it, new_beat_start, new_beat_len);
                    const auto new_on_time = new_beat_start + new_on_time_in_beat;
                    for (auto &note : chord_it->second.notes) {
                        if (note.off_time - new_on_time < MChord::min_allowed_duration()) {
                            note.off_time = new_on_time + MChord::min_allowed_duration();
                        }
                    }
                    new_chords.insert({new_on_time, chord_it->second});
                }

                if (chord_it == chords.end()) {
                    break;
                }

                beat_start = beat_end;
                new_beat_start += new_beat_len;
            }

            std::swap(chords, new_chords);
#ifndef NDEBUG
            Q_ASSERT_X(MChord::are_notes_long_enough(chords), "MidiBeat::adjustChordsToBeats", "There are too short notes");
#endif
        }
    }
}

void update_first_last_beats(MidiOperations::HumanBeatData &beat_data, const ReducedFraction &time_sig) {
    for (int i = 0; i != beat_data.added_first_beats; ++i) {
        Q_ASSERT_X(!beat_data.beat_set.empty(), "MidiBeat::updateFirstLastBeats", "Empty beat set after first beats deletion");

        beat_data.beat_set.erase(beat_data.beat_set.begin());
    }
    for (int i = 0; i != beat_data.added_last_beats; ++i) {
        Q_ASSERT_X(!beat_data.beat_set.empty(), "MidiBeat::updateFirstLastBeats", "Empty beat set after last beats deletion");

        beat_data.beat_set.erase(std::prev(beat_data.beat_set.end()));
    }

    const int beats_in_bar = MidiBeat::beats_in_bar(time_sig);

    MidiBeat::add_first_beats(beat_data.beat_set, beat_data.first_chord_tick, beats_in_bar, beat_data.added_first_beats);
    MidiBeat::add_last_beats(beat_data.beat_set, beat_data.last_chord_tick, beats_in_bar, beat_data.added_last_beats);
}

void set_time_signature(engraving::TimeSigMap *sigmap) {
    auto *data = midi_import_operations.data();
    const std::set<ReducedFraction> &beats = data->human_beat_data.beat_set;
    if (beats.empty()) {
        return; // don't set time sig for non-human performed MIDI files
    }
    const auto time_sig =
        Meter::user_time_sig_to_fraction(data->track_opers.time_sig_numerator.value(), data->track_opers.time_sig_denominator.value());
    set_time_sig(sigmap, time_sig);
    update_first_last_beats(data->human_beat_data, time_sig);
}
}} // namespace mu::iex::midi::MidiBeat
