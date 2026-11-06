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
#include "importmidi_tuplet_voice.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>

#include "importmidi_tuplet.h"
#include "importmidi_chord.h"
#include "importmidi_quant.h"
#include "importmidi_inner.h"
#include "importmidi_voice.h"
#include "importmidi_operations.h"
#include "engraving/dom/mscore.h"

namespace mu::iex::midi { namespace MidiTuplet {
int tuplet_voice_limit() {
    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();
    const size_t allowed_voices = MidiVoice::to_int_voice_count(opers.max_voice_count.value(current_track));

    Q_ASSERT_X(allowed_voices <= engraving::voices, "MidiTuplet::tupletVoiceLimit",
               "Allowed voice count exceeds MuseScore Studio voice limit");

    // for multiple voices: one voice is reserved for non-tuplet chords
    return (allowed_voices == 1) ? 1 : static_cast<int>(allowed_voices) - 1;
}

std::pair<ReducedFraction, ReducedFraction> chord_interval(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                           const std::multimap<ReducedFraction, MidiChord> &chords,
                                                           const ReducedFraction &basic_quant, const ReducedFraction &bar_start) {
    const auto on_time = MidiTuplet::find_on_time_between_chords(chord, chords, basic_quant, bar_start);
    auto off_time = Quantize::find_max_quantized_off_time(chord, basic_quant);
    if (off_time < on_time) {
        off_time = on_time;
    }
    if (off_time == on_time) {
        off_time += Quantize::quant_for_len(MChord::min_note_len(chord), basic_quant);
    }

    Q_ASSERT_X(off_time > on_time, "MidiTuplet::chordInterval", "Off time <= On time");

    return std::make_pair(on_time, off_time);
}

int find_tuplet_with_chord(const MidiChord &midi_chord, const std::vector<TupletInfo> &tuplets) {
    for (size_t i = 0; i != tuplets.size(); ++i) {
        for (const auto &chord : tuplets[i].chords) {
            if (&(chord.second->second) == &midi_chord) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

std::pair<ReducedFraction, ReducedFraction> back_tied_interval(const TiedTuplet &tied_tuplet, const std::vector<TupletInfo> &tuplets,
                                                               const std::multimap<ReducedFraction, MidiChord> &chords,
                                                               const ReducedFraction &basic_quant, const ReducedFraction &bar_start) {
    const TupletInfo &tuplet = tuplet_from_id(tied_tuplet.tuplet_id, tuplets);
    const auto end = tuplet_interval(tuplet, basic_quant).second;
    const MidiChord &midi_chord = tied_tuplet.chord->second;

    const int tuplet_index = find_tuplet_with_chord(midi_chord, tuplets);
    const auto beg = (tuplet_index != -1) ? tuplet_interval(tuplets[tuplet_index], basic_quant).first :
                                            chord_interval(*tied_tuplet.chord, chords, basic_quant, bar_start).first;

    return std::make_pair(beg, end);
}

void set_tuplet_voice(std::map<ReducedFraction, std::multimap<ReducedFraction, MidiChord>::iterator> &tuplet_chords, int voice) {
    for (auto &tuplet_chord : tuplet_chords) {
        MidiChord &midi_chord = tuplet_chord.second->second;
        midi_chord.voice = voice;
    }
}

void set_tuplet_voices(std::vector<TupletInfo> &tuplets, std::set<int> &pending_tuplets,
                       std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tuplet_intervals,
                       const ReducedFraction &basic_quant) {
    const int limit = tuplet_voice_limit();
    int voice = 0;
    while (!pending_tuplets.empty() && voice < limit) {
        for (auto it = pending_tuplets.begin(); it != pending_tuplets.end();) {
            TupletInfo &tuplet = tuplet_from_id(*it, tuplets);
            const auto interval = tuplet_interval(tuplet, basic_quant);
            if (!have_intersection(interval, tuplet_intervals[voice])) {
                set_tuplet_voice(tuplet.chords, voice);
                tuplet_intervals[voice].push_back(interval);
                it = pending_tuplets.erase(it);
                continue;
            }
            ++it;
        }
        ++voice;
    }
}

int find_pitch_dist(const std::vector<MidiNote> &notes, const std::vector<TupletInfo> &tuplets, int voice) {
    int pitch_dist = std::numeric_limits<int>::max(); // bad value - only for the last choice
    if (tuplets.empty()) {
        return pitch_dist;
    }

    int tuplet_pitch = 0;
    for (const auto &tuplet : tuplets) {
        if (tuplet.chords.begin()->second->second.voice != voice) {
            continue;
        }
        int counter = 0;
        for (const auto &chord : tuplet.chords) {
            const MidiChord &c = chord.second->second;
            tuplet_pitch += MChord::chord_average_pitch(c.notes);
            ++counter;
        }
        tuplet_pitch = static_cast<int>(std::lround(tuplet_pitch * 1.0 / counter));
        break;
    }

    if (tuplet_pitch == 0) {
        return pitch_dist;
    }

    const int chord_pitch = MChord::chord_average_pitch(notes);
    pitch_dist = std::abs(chord_pitch - tuplet_pitch);

    return pitch_dist;
}

void set_non_tuplet_voices(std::set<std::pair<const ReducedFraction, MidiChord> *> &pending_non_tuplets,
                           const std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tuplet_intervals,
                           const std::vector<TupletInfo> &tuplets, const std::multimap<ReducedFraction, MidiChord> &chords,
                           const ReducedFraction &basic_quant, const ReducedFraction &bar_start) {
    const int limit = MidiVoice::voice_limit();
    while (!pending_non_tuplets.empty()) {
        auto chord = *pending_non_tuplets.begin();
        const auto interval = chord_interval(*chord, chords, basic_quant, bar_start);
        // pick the voice such that the average pitch difference
        // between the non-tuplet chord and tuplets with this voice
        // is the smallest
        int best_voice = -1;
        int min_pitch_dist = -1;
        for (int voice = 0; voice < limit; ++voice) {
            const auto fit = tuplet_intervals.find(voice);
            if (fit == tuplet_intervals.end() || !have_intersection(interval, fit->second)) {
                const int pitch_dist = find_pitch_dist(chord->second.notes, tuplets, voice);
                if (min_pitch_dist == -1 || pitch_dist < min_pitch_dist) {
                    min_pitch_dist = pitch_dist;
                    best_voice = voice;
                }
            }
        }

        Q_ASSERT_X(best_voice >= 0, "MidiTuplet::setNonTupletVoices", "Best voice not found");

        chord->second.voice = best_voice;
        pending_non_tuplets.erase(pending_non_tuplets.begin());
        // don't insert chord interval here
    }
}

#ifndef NDEBUG

bool are_all_elements_unique(const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets) {
    std::set<std::pair<const ReducedFraction, MidiChord> *> chords;
    for (const auto &chord : non_tuplets) {
        if (chords.find(&*chord) == chords.end()) {
            chords.insert(&*chord);
        } else {
            return false;
        }
    }
    return true;
}

bool have_tuplets_empty_chords(const std::vector<TupletInfo> &tuplets) {
    for (const auto &tuplet : tuplets) {
        if (tuplet.chords.empty()) {
            return true;
        }
    }
    return false;
}

bool do_tuplet_chords_have_same_voice(const std::vector<TupletInfo> &tuplets) {
    for (const auto &tuplet : tuplets) {
        auto it = tuplet.chords.cbegin();
        const int voice = it->second->second.voice;
        ++it;
        for (; it != tuplet.chords.cend(); ++it) {
            if (it->second->second.voice != voice) {
                return false;
            }
        }
    }
    return true;
}

// back tied tuplets are not checked here

bool have_overlapping_voices(const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                             const std::vector<TupletInfo> &tuplets, const std::list<TiedTuplet> &back_tied_tuplets,
                             const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                             const ReducedFraction &bar_start) {
    // <voice, intervals>
    std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> intervals;

    for (const auto &tuplet : tuplets) {
        const int voice = tuplet.chords.begin()->second->second.voice;
        const auto interval = std::make_pair(tuplet.on_time, tuplet.on_time + tuplet.len);
        if (have_intersection(interval, intervals[voice])) {
            return true;
        } else {
            intervals[voice].push_back(interval);
        }
    }

    for (const auto &chord : non_tuplets) {
        const int voice = chord->second.voice;
        const auto interval = chord_interval(*chord, chords, basic_quant, bar_start);
        if (have_intersection(interval, intervals[voice])) {
            bool flag = false;
            // if chord is tied then it can intersect tuplet
            for (const TiedTuplet &tied_tuplet : back_tied_tuplets) {
                if (tied_tuplet.chord == (&*chord) && (tied_tuplet.voice == -1 || tied_tuplet.voice == voice)) {
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                return true;
            }
        }
    }

    return false;
}

size_t chord_count(const std::vector<TupletInfo> &tuplets,
                   const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets) {
    size_t sum = non_tuplets.size();
    for (const auto &tuplet : tuplets) {
        sum += tuplet.chords.size();
    }
    return sum;
}

bool voice_dont_exceed_limit(const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                             const std::vector<TupletInfo> &tuplets) {
    for (const auto &tuplet : tuplets) {
        const int voice = tuplet.chords.begin()->second->second.voice;
        if (voice >= MidiVoice::voice_limit()) {
            return true;
        }
    }

    for (const auto &chord : non_tuplets) {
        const int voice = chord->second.voice;
        if (voice >= MidiVoice::voice_limit()) {
            return true;
        }
    }

    return false;
}

#endif

void erase_back_tied_tuplet(int tuplet_id, std::list<TiedTuplet> &back_tied_tuplets) {
    for (auto it = back_tied_tuplets.begin(); it != back_tied_tuplets.end(); ++it) {
        if (it->tuplet_id == tuplet_id) {
            back_tied_tuplets.erase(it);
            break;
        }
    }
}

// for the case when voice limit = 1

bool exclude_extra_voice_tuplets(std::vector<TupletInfo> &tuplets,
                                 std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                                 std::list<TiedTuplet> &back_tied_tuplets, const std::multimap<ReducedFraction, MidiChord> &chords,
                                 const ReducedFraction &basic_quant, const ReducedFraction &bar_start, int bar_index) {
    size_t sz = tuplets.size();
    if (sz == 0) {
        return false;
    }

    std::list<std::multimap<ReducedFraction, MidiChord>::iterator> new_non_tuplets;
    size_t added_count = non_tuplets.size();
    while (added_count > 0) {
        non_tuplets.splice(non_tuplets.begin(), new_non_tuplets);
        // remove tuplets that are overlapped with non-tuplets
        for (size_t i = 0; i < sz;) {
            const auto interval = tuplet_interval(tuplets[i], basic_quant);
            bool shift = false;
            size_t counter = 0;
            for (const auto &non_tuplet : non_tuplets) {
                ++counter;
                if (counter > added_count) {
                    break;
                }
                if (have_intersection(interval, chord_interval(*non_tuplet, chords, basic_quant, bar_start))) {
                    bool is_tied = false;
                    for (const TiedTuplet &tied_tuplet : back_tied_tuplets) {
                        if (tied_tuplet.tuplet_id == tuplets[i].id && tied_tuplet.chord == &*non_tuplet) {
                            is_tied = true;
                            break;
                        }
                    }
                    if (!is_tied) {
                        for (const auto &chord : tuplets[i].chords) {
                            if (chord.second->second.bar_index == bar_index) {
                                new_non_tuplets.push_back(chord.second);
                            }
                        }
                        erase_back_tied_tuplet(tuplets[i].id, back_tied_tuplets);
                        --sz;
                        if (i < sz) {
                            shift = true;
                            tuplets[i] = tuplets[sz];
                        }
                        break;
                    }
                }
            }
            if (shift) {
                continue;
            }
            ++i;
        }
        added_count = new_non_tuplets.size();
    }
#ifndef NDEBUG
    Q_ASSERT_X(are_all_elements_unique(non_tuplets), "MidiTuplet::excludeExtraVoiceTuplets", "Non unique chords in non-tuplets");
#endif
    bool excluded = (sz != tuplets.size());
    tuplets.resize(sz);

    return excluded;
}

void remove_unused_tuplets(std::vector<TupletInfo> &tuplets, std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                           std::set<int> &pending_tuplets, std::list<TiedTuplet> &back_tied_tuplets,
                           std::set<std::pair<const ReducedFraction, MidiChord> *> &pending_non_tuplets, int bar_index) {
    if (pending_tuplets.empty()) {
        return;
    }

    std::vector<TupletInfo> new_tuplets;
    for (size_t i = 0; i != tuplets.size(); ++i) {
        if (pending_tuplets.find(tuplets[i].id) == pending_tuplets.end()) {
            new_tuplets.push_back(tuplets[i]);
        } else {
            erase_back_tied_tuplet(tuplets[i].id, back_tied_tuplets);
            for (const auto &chord : tuplets[i].chords) {
                if (chord.second->second.bar_index == bar_index) {
                    non_tuplets.push_back(chord.second);
                    pending_non_tuplets.insert(&*chord.second);
                }
            }
        }
    }
    pending_tuplets.clear();
    std::swap(tuplets, new_tuplets);
}

std::set<int> find_pending_tuplets(const std::vector<TupletInfo> &tuplets) {
    std::set<int> pending_tuplets; // tuplet indexes
    for (size_t i = 0; i != tuplets.size(); ++i) {
        pending_tuplets.insert(tuplets[i].id);
    }
    return pending_tuplets;
}

std::set<std::pair<const ReducedFraction, MidiChord> *>
find_pending_non_tuplets(const std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets) {
    std::set<std::pair<const ReducedFraction, MidiChord> *> pending_non_tuplets;
    for (const auto &c : non_tuplets) {
        pending_non_tuplets.insert(&*c);
    }
    return pending_non_tuplets;
}

std::list<TiedTuplet>::iterator erase_back_tied_tuplet(const std::list<TiedTuplet>::iterator &it, std::list<TiedTuplet> &back_tied_tuplets,
                                                       const TupletInfo &tuplet) {
    for (const auto &chord : tuplet.chords) {
        for (auto it2 = back_tied_tuplets.begin(); it2 != back_tied_tuplets.end(); ++it2) {
            if (&(chord.second->second) == &(it2->chord->second)) {
                back_tied_tuplets.erase(it2);
                break;
            }
        }
    }

    return back_tied_tuplets.erase(it);
}

void set_voices_from_prev_bars(std::list<TiedTuplet> &back_tied_tuplets, std::vector<TupletInfo> &tuplets, std::set<int> &pending_tuplets,
                               const std::set<std::pair<const ReducedFraction, MidiChord> *> &pending_non_tuplets,
                               std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tuplet_intervals,
                               std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &back_tuplet_intervals,
                               const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                               const ReducedFraction &bar_start) {
#ifdef NDEBUG
    Q_UNUSED(pending_non_tuplets);
#endif
    bool loop_again = false;
    do {
        for (auto it = back_tied_tuplets.begin(); it != back_tied_tuplets.end();) {
            const TiedTuplet &tied_tuplet = *it;
            TupletInfo &tuplet = tuplet_from_id(tied_tuplet.tuplet_id, tuplets);
            const auto back_interval = std::make_pair(tuplet.on_time, tuplet.on_time + tuplet.len);

            if (tied_tuplet.voice == -1) {
                ++it;
                continue;
            }
            if (have_intersection(back_interval, back_tuplet_intervals[tied_tuplet.voice])) {
                it = back_tied_tuplets.erase(it);
                continue;
            }

            for (const auto &chord : tuplet.chords) {
                for (auto it2 = back_tied_tuplets.begin(); it2 != back_tied_tuplets.end(); ++it2) {
                    if (it2 == it) {
                        continue;
                    }
                    if (&(chord.second->second) == &(it2->chord->second) && it2->voice == -1) {
                        it2->voice = tied_tuplet.voice;
                        loop_again = true;
                        break;
                    }
                }
            }

            set_tuplet_voice(tuplet.chords, tied_tuplet.voice);
            back_tuplet_intervals[tied_tuplet.voice].push_back(back_interval);
            tuplet_intervals[tied_tuplet.voice].push_back(back_interval);
            // add to intervals chord from previous bar
            tuplet_intervals[tied_tuplet.voice].push_back(chord_interval(*tied_tuplet.chord, chords, basic_quant, bar_start));
            pending_tuplets.erase(tied_tuplet.tuplet_id);

            Q_ASSERT_X(pending_non_tuplets.find(tied_tuplet.chord) == pending_non_tuplets.end(), "MidiTuplet::setBackTiedVoices",
                       "Tied non-tuplet chord should not be here");

            ++it;
        }
    } while (loop_again);
}

void set_tied_chord_voice(std::list<TiedTuplet> &back_tied_tuplets, std::vector<TupletInfo> &tuplets, std::set<int> &pending_tuplets,
                          std::set<std::pair<const ReducedFraction, MidiChord> *> &pending_non_tuplets,
                          std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tuplet_intervals,
                          std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &back_tuplet_intervals,
                          const std::multimap<ReducedFraction, MidiChord> &chords, const std::list<TiedTuplet>::iterator &back_tied_it,
                          bool is_non_tuplet_back_chord, const ReducedFraction &basic_quant, const ReducedFraction &bar_start) {
    const TiedTuplet &tied_tuplet = *back_tied_it;

    if (is_non_tuplet_back_chord) {
        // non-tuplet chord tied
        const auto interval = chord_interval(*tied_tuplet.chord, chords, basic_quant, bar_start);
        back_tuplet_intervals[tied_tuplet.voice].push_back(interval);
        tied_tuplet.chord->second.voice = tied_tuplet.voice;
        if (tuplet_voice_limit() > 1) {
            tuplet_intervals[tied_tuplet.voice].push_back(interval);
        }
        pending_non_tuplets.erase(tied_tuplet.chord);
    } else {
        // tuplet tied
        const int i = find_tuplet_with_chord(tied_tuplet.chord->second, tuplets);

        Q_ASSERT_X(i != -1, "MidiTuplet::setBackTiedVoices", "Tuplet chord not found in tuplets");

        auto it2 = std::next(back_tied_it);
        for (; it2 != back_tied_tuplets.end(); ++it2) {
            if (it2->tuplet_id == tuplets[i].id) {
                break;
            }
        }
        if (it2 != back_tied_tuplets.end()) {
            if (it2->voice == -1) {
                it2->voice = tied_tuplet.voice;
            }
        } else {
            // set voice of not back-tied tuplet that have tied chord
            set_tuplet_voice(tuplets[i].chords, tied_tuplet.voice);
            const auto interval = tuplet_interval(tuplets[i], basic_quant);
            tuplet_intervals[tied_tuplet.voice].push_back(interval);
            back_tuplet_intervals[tied_tuplet.voice].push_back(interval);
            pending_tuplets.erase(tuplets[i].id);
        }
    }
}

void set_voice_of_connected_back_tied(std::list<TiedTuplet> &back_tied_tuplets, int tied_tuplet_voice, const std::set<int> &pending_tuplets,
                                      const TupletInfo &tuplet, const std::list<TiedTuplet>::iterator &back_tied_it) {
    for (const auto &chord : tuplet.chords) {
        for (auto it2 = std::next(back_tied_it); it2 != back_tied_tuplets.end(); ++it2) {
            if (pending_tuplets.find(it2->tuplet_id) == pending_tuplets.end()) {
                continue;
            }
            if (&(chord.second->second) == &(it2->chord->second) && it2->voice == -1) {
                it2->voice = tied_tuplet_voice;
                break;
            }
        }
    }
}

int find_voice_for_back_tied(const std::pair<const ReducedFraction, MidiChord> &tied_tuplet_chord, int voice_limit,
                             const std::pair<ReducedFraction, ReducedFraction> &back_interval,
                             std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &back_tuplet_intervals,
                             const std::multimap<ReducedFraction, MidiChord> &chords, bool is_non_tuplet_back_chord,
                             const ReducedFraction &basic_quant, const ReducedFraction &bar_start) {
    int voice = 0;
    for (; voice != voice_limit; ++voice) {
        if (have_intersection(back_interval, back_tuplet_intervals[voice]) ||
            (is_non_tuplet_back_chord &&
             have_intersection(chord_interval(tied_tuplet_chord, chords, basic_quant, bar_start), back_tuplet_intervals[voice]))) {
            continue;
        }
        break;
    }

    return voice;
}

std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>>
find_back_tuplet_intervals(const std::list<TiedTuplet> &back_tied_tuplets, const std::vector<TupletInfo> &tuplets) {
    std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> back_tuplet_intervals;
    for (const auto &t : back_tied_tuplets) {
        const auto &tuplet = tuplet_from_id(t.tuplet_id, tuplets);
        const auto interval = std::make_pair(tuplet.on_time, tuplet.on_time + tuplet.len);
        back_tuplet_intervals[t.voice].push_back(interval);
    }
    return back_tuplet_intervals;
}

void set_back_tied_voices(std::list<TiedTuplet> &back_tied_tuplets, std::vector<TupletInfo> &tuplets, std::set<int> &pending_tuplets,
                          std::set<std::pair<const ReducedFraction, MidiChord> *> &pending_non_tuplets,
                          std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &tuplet_intervals,
                          const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                          const ReducedFraction &bar_start) {
    auto back_tuplet_intervals = find_back_tuplet_intervals(back_tied_tuplets, tuplets);

    // set voices that are already set from one of previous bars
    set_voices_from_prev_bars(back_tied_tuplets, tuplets, pending_tuplets, pending_non_tuplets, tuplet_intervals, back_tuplet_intervals,
                              chords, basic_quant, bar_start);

    // set yet unset back tied voices
    const int limit = tuplet_voice_limit();

    for (auto it = back_tied_tuplets.begin(); it != back_tied_tuplets.end();) {
        TiedTuplet &tied_tuplet = *it;
        if (pending_tuplets.find(tied_tuplet.tuplet_id) == pending_tuplets.end()) {
            ++it;
            continue;
        }

        TupletInfo &tuplet = tuplet_from_id(tied_tuplet.tuplet_id, tuplets);
        const auto back_interval = std::make_pair(tuplet.on_time, tuplet.on_time + tuplet.len);
        bool is_non_tuplet_back_chord = (pending_non_tuplets.find(tied_tuplet.chord) != pending_non_tuplets.end());

        if (tied_tuplet.voice == -1) {
            const int voice = find_voice_for_back_tied(*tied_tuplet.chord, limit, back_interval, back_tuplet_intervals, chords,
                                                       is_non_tuplet_back_chord, basic_quant, bar_start);
            if (voice < limit) {
                tied_tuplet.voice = voice;
            } else { // no available voices
                it = erase_back_tied_tuplet(it, back_tied_tuplets, tuplet);
                continue;
            }
        } else {
            if (have_intersection(back_interval, back_tuplet_intervals[tied_tuplet.voice]) ||
                (is_non_tuplet_back_chord && have_intersection(chord_interval(*tied_tuplet.chord, chords, basic_quant, bar_start),
                                                               back_tuplet_intervals[tied_tuplet.voice]))) {
                it = erase_back_tied_tuplet(it, back_tied_tuplets, tuplet);
                continue;
            }
        }

        set_voice_of_connected_back_tied(back_tied_tuplets, tied_tuplet.voice, pending_tuplets, tuplet, it);

        // set voices of tied tuplet chords
        set_tuplet_voice(tuplet.chords, tied_tuplet.voice);
        back_tuplet_intervals[tied_tuplet.voice].push_back(back_interval);
        tuplet_intervals[tied_tuplet.voice].push_back(tuplet_interval(tuplet, basic_quant));
        pending_tuplets.erase(tied_tuplet.tuplet_id);

        set_tied_chord_voice(back_tied_tuplets, tuplets, pending_tuplets, pending_non_tuplets, tuplet_intervals, back_tuplet_intervals,
                             chords, it, is_non_tuplet_back_chord, basic_quant, bar_start);
        ++it;
    }
}

std::map<std::pair<const ReducedFraction, MidiChord> *, size_t> find_mapped_tuplet_chords(const std::vector<TupletInfo> &tuplets) {
    // <chord address, tupletIndex>
    std::map<std::pair<const ReducedFraction, MidiChord> *, size_t> tuplet_chords;
    for (size_t i = 0; i != tuplets.size(); ++i) {
        for (const auto &tuplet_chord : tuplets[i].chords) {
            auto tuplet_it = tuplet_chord.second;
            tuplet_chords.insert({&*tuplet_it, i});
        }
    }
    return tuplet_chords;
}

bool are_tuplets_intersect(const TupletInfo &t1, const TupletInfo &t2) {
    const auto on_time1 = t1.on_time;
    const auto end_time1 = on_time1 + t1.len;
    const auto on_time2 = t2.on_time;
    const auto end_time2 = on_time1 + t2.len;
    return end_time1 > on_time2 && on_time1 < end_time2;
}

// result: tied notes indexes

std::vector<int> find_tied_notes(const TupletInfo &tuplet, const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                                 const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &start_bar_tick,
                                 const ReducedFraction &basic_quant) {
    std::vector<int> tied_notes;
    const auto tuplet_ratio = tuplet_limits(tuplet.tuplet_number).ratio;
    const auto first_tuplet_chord_on_time =
        Quantize::find_quantized_tuplet_chord_on_time(*tuplet.chords.begin()->second, tuplet.len, tuplet_ratio, start_bar_tick);

    const auto max_chord_off_time = Quantize::find_max_quantized_tuplet_off_time(*chord_it, tuplet.len, tuplet_ratio, start_bar_tick);

    if (max_chord_off_time > first_tuplet_chord_on_time) {
        return tied_notes;
    }

    const auto on_time = MidiTuplet::find_on_time_between_chords(*chord_it, chords, basic_quant, start_bar_tick);
    if (on_time >= tuplet.on_time) {
        return tied_notes;
    }

    for (int i = 0; i != chord_it->second.notes.size(); ++i) {
        const MidiNote &note = chord_it->second.notes[i];

        const auto off_time_in_tuplet =
            Quantize::find_quantized_tuplet_note_off_time(chord_it->first, note.off_time, tuplet.len, tuplet_ratio, start_bar_tick).first;

        if (off_time_in_tuplet < start_bar_tick || off_time_in_tuplet <= tuplet.on_time) {
            continue;
        }

        const auto regular_off_time = Quantize::find_quantized_note_off_time(*chord_it, note.off_time, basic_quant).first;
        const auto regular_error = (note.off_time - regular_off_time).abs_value();
        const auto tuplet_error = (note.off_time - off_time_in_tuplet).abs_value();
        if (tuplet_error > regular_error) {
            continue;
        }

        tied_notes.push_back(i);
    }

    return tied_notes;
}

// prepare tied tuplets - pairs of tuplet and chord back-tied to it
// voices of back-tied chords from previous bar are set explicitly
// other voices = -1
// tied tuplets with voice != -1 don't intersect each other
// tuplets can be tied only to one chord or tuplet (no 'branches')

std::list<TiedTuplet> find_back_tied_tuplets(const std::multimap<ReducedFraction, MidiChord> &chords,
                                             const std::vector<TupletInfo> &tuplets, const ReducedFraction &prev_bar_start,
                                             const ReducedFraction &start_bar_tick, const ReducedFraction &basic_quant,
                                             int current_bar_index) {
    std::list<TiedTuplet> tied_tuplets;
    // <voice, intervals>
    std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> back_tuplet_intervals;
    std::set<int> used_tuplets;
    std::set<std::pair<const ReducedFraction, MidiChord> *> used_chords;
    const auto tuplet_chords = find_mapped_tuplet_chords(tuplets);

    for (size_t i = 0; i != tuplets.size(); ++i) {
        Q_ASSERT_X(!tuplets[i].chords.empty(), "MidiTuplets::findBackTiedTuplets", "Tuplet chords are empty");

        auto chord_it = tuplets[i].chords.begin()->second;
        while (chord_it != chords.begin() && chord_it->first >= prev_bar_start) {
            --chord_it;

            const auto tuplet_it = tuplet_chords.find(&*chord_it);
            const bool is_in_tuplet_of_this_bar = (tuplet_it != tuplet_chords.end());
            // don't make back tie to the chord in overlapping tuplet
            if (is_in_tuplet_of_this_bar && are_tuplets_intersect(tuplets[tuplet_it->second], tuplets[i])) {
                continue;
            }
            // remember voices of tuplets that have tied chords from previous bar
            // and that chords don't belong to the tuplets of this bar
            const int voice = (chord_it->second.bar_index < current_bar_index) ? chord_it->second.voice : -1;
            const auto interval = std::make_pair(tuplets[i].on_time, tuplets[i].on_time + tuplets[i].len);
            // if voice is specified and the new interval have intersection
            // with already found back tuplets with the save voice
            // then discard the interval
            if (voice != -1 && have_intersection(interval, back_tuplet_intervals[voice])) {
                continue;
            }

            const auto tied_notes = find_tied_notes(tuplets[i], chord_it, chords, start_bar_tick, basic_quant);
            if (!tied_notes.empty()) {
                // don't tie back twice to the same chord or tuplet
                if (used_chords.find(&*chord_it) != used_chords.end()) {
                    continue;
                }
                // don't tie back twice to the same tuplet
                const int tuplet_index = find_tuplet_with_chord(chord_it->second, tuplets);
                if (used_tuplets.find(tuplet_index) != used_tuplets.end()) {
                    continue;
                }
                // we can add back-tied tuplet; voice here can be -1
                tied_tuplets.push_back({tuplets[i].id, voice, &*chord_it, tied_notes});
                back_tuplet_intervals[voice].push_back(interval);
                used_chords.insert(&*chord_it);
                used_tuplets.insert(tuplet_index);
                break;
            }
        }
    }
    return tied_tuplets;
}

// chord notes should not be rearranged here
// because note indexes are stored in tied tuplets

void assign_voices(std::vector<TupletInfo> &tuplets, std::list<std::multimap<ReducedFraction, MidiChord>::iterator> &non_tuplets,
                   std::list<TiedTuplet> &back_tied_tuplets, const std::multimap<ReducedFraction, MidiChord> &chords,
                   const ReducedFraction &basic_quant, const ReducedFraction &bar_start, int bar_index) {
#ifndef NDEBUG
    Q_ASSERT_X(!have_tuplets_empty_chords(tuplets), "MIDI tuplets: assignVoices", "Empty tuplet chords");
#endif
    auto pending_tuplets = find_pending_tuplets(tuplets);
    auto pending_non_tuplets = find_pending_non_tuplets(non_tuplets);

    // <voice, intervals>
    std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> tuplet_intervals;

    set_back_tied_voices(back_tied_tuplets, tuplets, pending_tuplets, pending_non_tuplets, tuplet_intervals, chords, basic_quant,
                         bar_start);
    set_tuplet_voices(tuplets, pending_tuplets, tuplet_intervals, basic_quant);
    remove_unused_tuplets(tuplets, non_tuplets, pending_tuplets, back_tied_tuplets, pending_non_tuplets, bar_index);

    if (tuplet_voice_limit() == 1) {
        bool excluded = exclude_extra_voice_tuplets(tuplets, non_tuplets, back_tied_tuplets, chords, basic_quant, bar_start, bar_index);
        if (excluded) { // to exclude tuplet intervals - rebuild all intervals
            tuplet_intervals.clear();
            for (const auto &tuplet : tuplets) {
                const int voice = tuplet.chords.begin()->second->second.voice;
                tuplet_intervals[voice].push_back(tuplet_interval(tuplet, basic_quant));
            }
        }
    }

    set_non_tuplet_voices(pending_non_tuplets, tuplet_intervals, tuplets, chords, basic_quant, bar_start);
#ifndef NDEBUG
    Q_ASSERT_X(pending_non_tuplets.empty(), "MIDI tuplets: assignVoices", "Unused non-tuplets");
    Q_ASSERT_X(!have_tuplets_empty_chords(tuplets), "MIDI tuplets: assignVoices", "Empty tuplet chords");
    Q_ASSERT_X(do_tuplet_chords_have_same_voice(tuplets), "MIDI tuplets: assignVoices", "Tuplet chords have different voices");
    Q_ASSERT_X(!have_overlapping_voices(non_tuplets, tuplets, back_tied_tuplets, chords, basic_quant, bar_start),
               "MIDI tuplets: assignVoices", "Overlapping tuplets of the same voice");
    Q_ASSERT_X(!voice_dont_exceed_limit(non_tuplets, tuplets), "MIDI tuplets: assignVoices", "Voice exceeds the limit");
#endif
}
}} // namespace mu::iex::midi::MidiTuplet
