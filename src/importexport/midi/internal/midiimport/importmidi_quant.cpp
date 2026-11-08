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
#include "importmidi_quant.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <set>
#include <vector>

#include "engraving/dom/mscore.h"
#include "engraving/dom/sig.h"
#include "importmidi_beat.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_tempo.h"
#include "importmidi_tuplet.h"
#include "importmidi_voice.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace Quantize {
ReducedFraction quant_value_to_fraction(MidiOperations::QuantValue quant_value) {
    const auto division = ReducedFraction::from_ticks(Constants::division);
    ReducedFraction fraction;

    switch (quant_value) {
        case MidiOperations::QuantValue::Q_4: fraction = division; break;
        case MidiOperations::QuantValue::Q_8: fraction = division / 2; break;
        case MidiOperations::QuantValue::Q_16: fraction = division / 4; break;
        case MidiOperations::QuantValue::Q_32: fraction = division / 8; break;
        case MidiOperations::QuantValue::Q_64: fraction = division / 16; break;
        case MidiOperations::QuantValue::Q_128: fraction = division / 32; break;
        case MidiOperations::QuantValue::Q_256: fraction = division / 64; break;
        case MidiOperations::QuantValue::Q_512: fraction = division / 128; break;
        case MidiOperations::QuantValue::Q_1024: fraction = division / 256; break;
        default: assert(false); break;
    }

    return fraction;
}

MidiOperations::QuantValue fraction_to_quant_value(const ReducedFraction &fraction) {
    const auto division = ReducedFraction::from_ticks(Constants::division);
    MidiOperations::QuantValue quant_value = MidiOperations::QuantValue::Q_4;

    if (fraction == division) {
        quant_value = MidiOperations::QuantValue::Q_4;
    } else if (fraction == division / 2) {
        quant_value = MidiOperations::QuantValue::Q_8;
    } else if (fraction == division / 4) {
        quant_value = MidiOperations::QuantValue::Q_16;
    } else if (fraction == division / 8) {
        quant_value = MidiOperations::QuantValue::Q_32;
    } else if (fraction == division / 16) {
        quant_value = MidiOperations::QuantValue::Q_64;
    } else if (fraction == division / 32) {
        quant_value = MidiOperations::QuantValue::Q_128;
    } else if (fraction == division / 64) {
        quant_value = MidiOperations::QuantValue::Q_256;
    } else if (fraction == division / 128) {
        quant_value = MidiOperations::QuantValue::Q_512;
    } else if (fraction == division / 256) {
        quant_value = MidiOperations::QuantValue::Q_1024;
    } else {
        LOGD("Unknown quant fraction %d/%d in division %d/%d.", fraction.numerator(), fraction.denominator(), division.numerator(),
             division.denominator());
        quant_value = MidiOperations::QuantValue::Q_INVALID;
    }

    return quant_value;
}

MidiOperations::QuantValue default_quant_value_from_preferences() {
    // The engine has no MIDI import preferences: the upstream default, a 16th.
    int ticks = Constants::division / 4;
    const auto fraction = ReducedFraction::from_ticks(ticks);
    MidiOperations::QuantValue quant_value = fraction_to_quant_value(fraction);
    if (quant_value == MidiOperations::QuantValue::Q_INVALID) {
        LOGD("Unknown shortestNote value %d in preferences, defaulting to 16th.", ticks);
        quant_value = MidiOperations::QuantValue::Q_16;
    }
    return quant_value;
}

ReducedFraction shortest_quantized_note_in_range(const std::multimap<ReducedFraction, MidiChord>::const_iterator &beg,
                                                 const std::multimap<ReducedFraction, MidiChord>::const_iterator &end) {
    const auto division = ReducedFraction::from_ticks(Constants::division);
    auto min_duration = division;
    for (auto it = beg; it != end; ++it) {
        for (const auto &note : it->second.notes) {
            if (note.off_time - it->first < min_duration) {
                min_duration = note.off_time - it->first;
            }
        }
    }
    const auto min_allowed_duration = MChord::min_allowed_duration();
    auto shortest = division;
    for (; shortest > min_allowed_duration; shortest /= 2) {
        if (shortest <= min_duration) {
            break;
        }
    }
    return shortest;
}

ReducedFraction reduce_quant_if_dotted_note(const ReducedFraction &note_len, const ReducedFraction &raster) {
    auto new_raster = raster;
    const auto div = note_len / raster;
    const double ratio = div.to_double();
    if (ratio > 1.45 && ratio < 1.55) { // 1.5: dotted note that is larger than quantization value
        new_raster /= 2;                // reduce quantization error for dotted notes
    }
    return new_raster;
}

ReducedFraction quantize_value(const ReducedFraction &value, const ReducedFraction &quant) {
    const auto value_reduced = value.reduced();
    const auto raster_reduced = quant.reduced();
    int val_num = value_reduced.numerator() * raster_reduced.denominator();
    const int rast_num = raster_reduced.numerator() * value_reduced.denominator();
    const int common_den = value_reduced.denominator() * raster_reduced.denominator();
    val_num = ((val_num + rast_num / 2) / rast_num) * rast_num;
    return ReducedFraction(val_num, common_den).reduced();
}

ReducedFraction quant_for_len(const ReducedFraction &note_len, const ReducedFraction &basic_quant) {
    auto quant = basic_quant;
    while (quant > note_len && quant >= MChord::min_allowed_duration() * 2) {
        quant /= 2;
    }

    if (quant >= MChord::min_allowed_duration() * 2) {
        quant = reduce_quant_if_dotted_note(note_len, quant);
    }

    return quant;
}

ReducedFraction quant_for_tuplet(const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio) {
    const auto quant = tuplet_len / tuplet_ratio.numerator();

    assert(quant >= MChord::min_allowed_duration());

    if (quant >= MChord::min_allowed_duration() * 2) {
        return quant / 2;
    }

    return quant;
}

ReducedFraction find_min_quant(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant) {
    ReducedFraction min_quant(-1, 1);
    for (const auto &note : chord.second.notes) {
        const auto quant = quant_for_len(note.off_time - chord.first, basic_quant);
        if (min_quant == ReducedFraction(-1, 1) || quant < min_quant) {
            min_quant = quant;
        }
    }
    return min_quant;
}

ReducedFraction find_quantized_tuplet_chord_on_time(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                    const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio,
                                                    const ReducedFraction &range_start) {
    if (chord.first <= range_start) {
        return range_start;
    }
    const auto quant = quant_for_tuplet(tuplet_len, tuplet_ratio);
    return range_start + quantize_value(chord.first - range_start, quant);
}

ReducedFraction find_quantized_chord_on_time(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant) {
    const ReducedFraction quant = find_min_quant(chord, basic_quant);
    return quantize_value(chord.first, quant);
}

std::pair<ReducedFraction, ReducedFraction>
find_quantized_tuplet_note_off_time(const ReducedFraction &on_time, const ReducedFraction &off_time, const ReducedFraction &tuplet_len,
                                    const ReducedFraction &tuplet_ratio, const ReducedFraction &range_start) {
    if (off_time <= range_start) {
        return {range_start, tuplet_len};
    }

    ReducedFraction q_off_time;
    auto quant = quant_for_tuplet(tuplet_len, tuplet_ratio);

    while (true) {
        q_off_time = range_start + quantize_value(off_time - range_start, quant);
        if (q_off_time <= on_time) {
            if (quant >= MChord::min_allowed_duration() * 2) {
                quant /= 2;
                continue;
            }
            q_off_time = on_time + quant;
        }
        break;
    }
    return {q_off_time, quant};
}

std::pair<ReducedFraction, ReducedFraction> find_quantized_note_off_time(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                                         const ReducedFraction &off_time,
                                                                         const ReducedFraction &basic_quant) {
    ReducedFraction q_off_time;
    auto quant = quant_for_len(off_time - chord.first, basic_quant);

    while (true) {
        q_off_time = quantize_value(off_time, quant);
        if (q_off_time <= chord.first) {
            if (quant >= MChord::min_allowed_duration() * 2) {
                quant /= 2;
                continue;
            }
            q_off_time = chord.first + quant;
        }
        break;
    }
    return {q_off_time, quant};
}

ReducedFraction find_min_quantized_on_time(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant) {
    ReducedFraction min_on_time(-1, 1);
    for (const auto &note : chord.second.notes) {
        const auto quant = quant_for_len(note.off_time - chord.first, basic_quant);
        const auto on_time = quantize_value(chord.first, quant);
        if (min_on_time == ReducedFraction(-1, 1) || on_time < min_on_time) {
            min_on_time = on_time;
        }
    }
    return min_on_time;
}

ReducedFraction find_max_quantized_tuplet_off_time(const std::pair<const ReducedFraction, MidiChord> &chord,
                                                   const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio,
                                                   const ReducedFraction &range_start) {
    ReducedFraction max_off_time(0, 1);
    for (const auto &note : chord.second.notes) {
        if (note.off_time <= range_start) {
            continue;
        }
        const auto off_time = find_quantized_tuplet_note_off_time(chord.first, note.off_time, tuplet_len, tuplet_ratio, range_start).first;
        if (off_time > max_off_time) {
            max_off_time = off_time;
        }
    }
    return max_off_time;
}

ReducedFraction find_max_quantized_off_time(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant) {
    ReducedFraction max_off_time(0, 1);
    for (const auto &note : chord.second.notes) {
        const auto off_time = find_quantized_note_off_time(chord, note.off_time, basic_quant).first;
        if (off_time > max_off_time) {
            max_off_time = off_time;
        }
    }
    return max_off_time;
}

ReducedFraction find_on_time_tuplet_quant_error(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &tuplet_len,
                                                const ReducedFraction &tuplet_ratio, const ReducedFraction &range_start) {
    const auto q_on_time = find_quantized_tuplet_chord_on_time(chord, tuplet_len, tuplet_ratio, range_start);
    return (chord.first - q_on_time).abs_value();
}

ReducedFraction find_on_time_quant_error(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &basic_quant) {
    const auto q_on_time = find_quantized_chord_on_time(chord, basic_quant);
    return (chord.first - q_on_time).abs_value();
}

ReducedFraction find_off_time_tuplet_quant_error(const ReducedFraction &on_time, const ReducedFraction &off_time,
                                                 const ReducedFraction &tuplet_len, const ReducedFraction &tuplet_ratio,
                                                 const ReducedFraction &range_start) {
    const auto q_off_time = find_quantized_tuplet_note_off_time(on_time, off_time, tuplet_len, tuplet_ratio, range_start).first;
    return (off_time - q_off_time).abs_value();
}

ReducedFraction find_off_time_quant_error(const std::pair<const ReducedFraction, MidiChord> &chord, const ReducedFraction &off_time,
                                          const ReducedFraction &basic_quant) {
    const auto q_off_time = find_quantized_note_off_time(chord, off_time, basic_quant).first;
    return (off_time - q_off_time).abs_value();
}

ReducedFraction find_quant_for_range(const std::multimap<ReducedFraction, MidiChord>::const_iterator &beg,
                                     const std::multimap<ReducedFraction, MidiChord>::const_iterator &end,
                                     const ReducedFraction &basic_quant) {
    const auto shortest_len = shortest_quantized_note_in_range(beg, end);
    return quant_for_len(shortest_len, basic_quant);
}

//--------------------------------------------------------------------------------------------

bool is_human_performance(const std::multimap<ReducedFraction, MidiChord> &chords, const TimeSigMap *sigmap) {
    if (chords.empty()) {
        return false;
    }

    const auto basic_quant = ReducedFraction::from_ticks(Constants::division) / 4; // 1/16
    int matches = 0;
    int count = 0;

    std::set<ReducedFraction> used_on_times;

    for (const auto &chord : chords) {
        const auto quant = std::min(basic_quant, quant_for_len(MChord::max_note_len(chord), basic_quant));
        const auto on_time = quantize_value(chord.first, quant);
        int bar_index, beat, tick;
        sigmap->tick_values(on_time.ticks(), &bar_index, &beat, &tick);

        const auto bar_start = ReducedFraction::from_ticks(sigmap->bar2tick(bar_index, 0));
        const auto bar_fraction = ReducedFraction(sigmap->timesig(bar_start.ticks()).timesig());
        const auto beat_len = Meter::beat_length(bar_fraction);

        if (((on_time - bar_start) / beat_len).reduced().denominator() == 1 && used_on_times.find(on_time) == used_on_times.end()) {
            used_on_times.insert(on_time);
            ++count;
            const auto diff = (on_time - chord.first).abs_value();
            if (diff < MChord::min_allowed_duration()) {
                ++matches;
            }
        }
    }

    const double tol_value = 0.6;
    const double matched = matches * 1.0 / count;

    return matched < tol_value;
}

std::multimap<int, MTrack> get_track_with_all_chords(const std::multimap<int, MTrack> &tracks) {
    std::multimap<int, MTrack> single_track{{0, MTrack()}};
    auto &all_chords = single_track.begin()->second.chords;
    for (const auto &track : tracks) {
        const MTrack &t = track.second;
        for (const auto &chord : t.chords) {
            all_chords.insert(chord);
        }
    }
    return single_track;
}

void set_if_human_performance(const std::multimap<int, MTrack> &tracks, TimeSigMap *sigmap) {
    auto all_chords_track = get_track_with_all_chords(tracks);
    MChord::collect_chords(all_chords_track, {2, 1}, {1, 2});
    const MTrack &track = all_chords_track.begin()->second;
    const auto &all_chords = track.chords;
    if (all_chords.empty()) {
        return;
    }
    const bool is_human = is_human_performance(all_chords, sigmap);
    auto &opers = midi_import_operations.data()->track_opers;
    if (opers.is_human_performance.can_redefine_default_later()) {
        opers.is_human_performance.set_default_value(is_human);
    }

    if (is_human) {
        if (opers.quant_value.can_redefine_default_later()) {
            opers.quant_value.set_default_value(MidiOperations::QuantValue::Q_8);
        }
        if (opers.max_voice_count.can_redefine_default_later()) {
            opers.max_voice_count.set_default_value(MidiOperations::VoiceCount::V_2);
        }
        const double ticks_per_sec = MidiTempo::find_basic_tempo(tracks, true) * Constants::division;
        MidiBeat::find_beat_locations(all_chords, sigmap, ticks_per_sec); // and set time sig
    }
}

//--------------------------------------------------------------------------------------------

// remove small intersection with the next chord

void preserve_legato(ReducedFraction &off_time, bool is_off_time_in_tuplet,
                     const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                     const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant) {
    auto it = std::next(chord_it);
    while (it != chords.end() && it->second.voice != chord_it->second.voice) {
        ++it;
    }
    if (it != chords.end()) {
        const auto ioi = it->first - chord_it->first;
        const auto cross = off_time - it->first;

        if (it->second.is_in_tuplet && is_off_time_in_tuplet) {
            // while we don't split tuplet into voices we don't want to have
            // note intersections smaller than tuplet note length
            const auto &tuplet = it->second.tuplet->second;
            const auto tuplet_note_len = tuplet.len / tuplet.tuplet_number;
            if (cross > ReducedFraction(0, 1) && cross < tuplet_note_len) {
                off_time = it->first;
            }
        } else if (!it->second.is_in_tuplet && !is_off_time_in_tuplet) {
            if (cross > ReducedFraction(0, 1) && cross < ioi / 2 && cross < basic_quant / 2) {
                off_time = it->first;
            }
        }
    }
}

std::pair<ReducedFraction, ReducedFraction>
quantize_off_time_for_tuplet(const ReducedFraction &note_off_time, const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                             const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant,
                             const MidiTuplet::TupletData &tuplet) {
    assert(chord_it->first < note_off_time);

    const auto tuplet_ratio = MidiTuplet::tuplet_limits(tuplet.tuplet_number).ratio;
    const auto result = find_quantized_tuplet_note_off_time(chord_it->first, note_off_time, tuplet.len, tuplet_ratio, tuplet.on_time);
    auto off_time = result.first;
    auto quant = result.second;

    preserve_legato(off_time, true, chord_it, chords, basic_quant);

    // verify that offTime is still inside tuplet
    if (off_time < tuplet.on_time) {
        off_time = tuplet.on_time;
        quant = tuplet.len;
    } else if (off_time > tuplet.on_time + tuplet.len) {
        off_time = tuplet.on_time + tuplet.len;
        quant = tuplet.len;
    }

    return {off_time, quant};
}

std::pair<ReducedFraction, ReducedFraction>
quantize_off_time_for_non_tuplet(const ReducedFraction &note_off_time, const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                                 const std::multimap<ReducedFraction, MidiChord> &chords, const ReducedFraction &basic_quant) {
    assert(chord_it->first < note_off_time);

    const MidiChord &chord = chord_it->second;
    const auto result = find_quantized_note_off_time(*chord_it, note_off_time, basic_quant);
    auto off_time = result.first;
    auto quant = result.second;
    preserve_legato(off_time, false, chord_it, chords, basic_quant);

    // verify that offTime is still outside tuplets
    if (chord.is_in_tuplet) {
        const auto &tuplet = chord.tuplet->second;
        if (off_time < tuplet.on_time + tuplet.len) {
            off_time = tuplet.on_time + tuplet.len;
            quant = tuplet.len;
            return {off_time, quant};
        }
    }
    auto next = std::next(chord_it);
    while (true) {
        if (next == chords.end()) {
            break;
        }
        if (!next->second.is_in_tuplet || next->second.voice != chord.voice) {
            ++next;
            continue;
        }
        const bool is_in_same_tuplet = chord.is_in_tuplet && next->second.tuplet == chord.tuplet;
        const auto &tuplet = next->second.tuplet->second;
        if (is_in_same_tuplet || tuplet.on_time + tuplet.len <= off_time) {
            ++next;
            continue;
        }

        if (off_time > tuplet.on_time) {
            off_time = tuplet.on_time;
            quant = tuplet.len;
        }
        break;
    }

    return {off_time, quant};
}

struct QuantPos {
    ReducedFraction time;
    int metrical_level;
    double penalty;
    int prev_pos;
};

struct QuantData {
    std::multimap<ReducedFraction, MidiChord>::const_iterator chord;
    ReducedFraction quant;
    ReducedFraction quant_for_len;
    ReducedFraction chord_range_start;
    ReducedFraction chord_range_end;
    // if inter on time interval with previous chord
    // is less than min allowed duration
    // then chord can be merged with previous chord
    bool can_merge_with_prev = false;
    int metrical_level_for_len;
    std::vector<QuantPos> positions;
};

#ifndef NDEBUG

bool are_all_voices_same(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords) {
    auto it = chords.begin();
    const int voice = (*it)->second.voice;
    for (++it; it != chords.end(); ++it) {
        if ((*it)->second.voice != voice) {
            return false;
        }
    }
    return true;
}

bool are_chords_different(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords) {
    std::set<const std::pair<const ReducedFraction, MidiChord> *> chord_set;

    for (const auto &chord : chords) {
        const auto it = chord_set.find(&*chord);
        if (it == chord_set.end()) {
            chord_set.insert(&*chord);
        } else {
            return false;
        }
    }

    return true;
}

bool not_less_than_prev(const std::vector<QuantData>::iterator &it, const std::vector<QuantData> &data) {
    if (it != data.begin()) {
        const auto prev = std::prev(it);
        if (prev->chord_range_start > it->chord_range_start) {
            return false;
        }
    }
    return true;
}

bool is_tuplet_range_correct(const MidiTuplet::TupletData &tuplet, const ReducedFraction &range_start, const ReducedFraction &range_end) {
    return range_start == tuplet.on_time && range_end == tuplet.on_time + tuplet.len;
}

void check_off_time(const MidiNote &note, const std::multimap<ReducedFraction, MidiChord>::iterator &chord_it,
                    const std::multimap<ReducedFraction, MidiChord> &chords) {
    assert(note.off_time - chord_it->first >= MChord::min_allowed_duration());

    if (note.is_in_tuplet) {
        const auto &tuplet = note.tuplet->second;

        assert(note.off_time >= tuplet.on_time && note.off_time <= tuplet.on_time + tuplet.len);
    } else {
        const auto &chord = chord_it->second;

        auto next = std::next(chord_it);
        while (true) {
            if (next == chords.end()) {
                break;
            }
            if (!next->second.is_in_tuplet || next->second.voice != chord.voice) {
                ++next;
                continue;
            }
            const auto &tuplet = next->second.tuplet->second;
            if ((chord.is_in_tuplet && next->second.tuplet == chord.tuplet) || tuplet.on_time + tuplet.len <= note.off_time) {
                ++next;
                continue;
            }

            assert(note.off_time <= tuplet.on_time);
            break;
        }

        auto prev = chord_it;
        while (true) {
            if (prev->second.bar_index != chord.bar_index) {
                break;
            }
            if (prev->second.voice != chord.voice || !prev->second.is_in_tuplet ||
                (chord.is_in_tuplet && prev->second.tuplet == chord.tuplet)) {
                if (prev != chords.begin()) {
                    --prev;
                    continue;
                }
            } else {
                const auto &tuplet = prev->second.tuplet->second;

                assert(note.off_time >= tuplet.on_time + tuplet.len);
            }
            break;
        }
    }
}

bool are_on_time_values_different(const std::multimap<ReducedFraction, MidiChord> &chords) {
    std::set<std::tuple<ReducedFraction, int, int>> on_time_voices_bars;
    for (const auto &chord : chords) {
        const auto tuple = std::make_tuple(chord.first, chord.second.voice, chord.second.bar_index);
        if (on_time_voices_bars.find(tuple) == on_time_voices_bars.end()) {
            on_time_voices_bars.insert(tuple);
        } else {
            return false;
        }
    }
    return true;
}

bool are_tuplet_chords_consistent(const std::multimap<ReducedFraction, MidiChord> &chords) {
    std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator prev_tuplet;
    bool prev_tuplet_set = false;
    bool is_in_tuplet = false;
    const int limit = MidiVoice::voice_limit();

    for (int voice = 0; voice != limit; ++voice) {
        for (const auto &chord : chords) {
            const MidiChord &c = chord.second;
            if (c.voice != voice) {
                continue;
            }
            if (c.is_in_tuplet) {
                if (!is_in_tuplet && prev_tuplet_set && c.tuplet == prev_tuplet) {
                    LOGD() << "Inconsistent tuplets, bar (from 1):" << (c.bar_index + 1);
                    return false; // there is a non-tuplet chord inside tuplet
                }
                is_in_tuplet = true;
                prev_tuplet = c.tuplet;
                prev_tuplet_set = true;
            } else {
                is_in_tuplet = false;
            }
        }
    }
    return true;
}

bool are_tuplet_chords_consistent(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords) {
    auto it = chords.begin();
    const bool is_in_tuplet = (*it)->second.is_in_tuplet;
    for (it = std::next(it); it != chords.end(); ++it) {
        if (is_in_tuplet && (!(*it)->second.is_in_tuplet || (*it)->second.tuplet != (*chords.begin())->second.tuplet)) {
            return false;
        }
        if (!is_in_tuplet && (*it)->second.is_in_tuplet) {
            return false;
        }
    }
    return true;
}

bool are_chords_sorted_by_on_time(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords) {
    for (size_t i = 0; i != chords.size() - 1; ++i) {
        if (chords[i]->first >= chords[i + 1]->first) {
            return false;
        }
    }
    return true;
}

#endif

ReducedFraction quantize_to_large(const ReducedFraction &time, const ReducedFraction &quant) {
    const auto ratio = time / quant;
    auto quantized = quant * (ratio.numerator() / ratio.denominator());
    if (quantized < time) {
        quantized += quant;
    }
    return quantized;
}

ReducedFraction quantize_to_small(const ReducedFraction &time, const ReducedFraction &quant) {
    const auto ratio = time / quant;
    auto quantized = quant * (ratio.numerator() / ratio.denominator());
    if (quantized >= time) {
        quantized -= quant;
    }
    return quantized;
}

void find_metrical_levels(std::vector<QuantData> &data, const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords,
                          const ReducedFraction &tuplet_quant, const ReducedFraction &bar_start, const ReducedFraction &bar_fraction) {
    const auto divs_info = (tuplet_quant != ReducedFraction(-1, 1)) ?
                               Meter::division_info(bar_fraction, {(*chords.begin())->second.tuplet->second}) :
                               Meter::division_info(bar_fraction, {});

    for (QuantData &d : data) {
        for (auto t = d.chord_range_start; t <= d.chord_range_end; t += d.quant) {
            QuantPos p;
            p.time = t;
            p.metrical_level = Meter::level_of_tick(t - bar_start, divs_info);
            d.positions.push_back(p);
        }

        int min_level = std::numeric_limits<int>::max();
        while (true) {
            for (auto t = d.chord_range_start; t <= d.chord_range_end; t += d.quant) {
                if (((t - bar_start) / d.quant_for_len).reduced().denominator() != 1) {
                    continue;
                }
                int level = Meter::level_of_tick(t - bar_start, divs_info);
                if (level < min_level) {
                    min_level = level;
                }
            }
            if (min_level == std::numeric_limits<int>::max()) {
                d.quant_for_len /= 2;

                assert(d.quant_for_len >= MChord::min_allowed_duration());

                continue;
            }
            break;
        }
        d.metrical_level_for_len = min_level;
    }
}

void find_chord_range_ends(std::vector<QuantData> &data, [[maybe_unused]] const ReducedFraction &range_start,
                           const ReducedFraction &range_end, const ReducedFraction &bar_start, const ReducedFraction &beat_len) {
    for (auto it = data.rbegin(); it != data.rend(); ++it) {
        QuantData &d = *it;
        d.chord_range_end = bar_start + quantize_to_small(range_end - bar_start, d.quant);

        assert(d.chord->first + beat_len >= range_start);

        if (d.chord->first + beat_len < range_end) {
            d.chord_range_end = bar_start + quantize_to_small(d.chord->first + beat_len - bar_start, d.quant);
        }
        if (it != data.rbegin()) {
            const auto prev = std::prev(it); // next in terms of time
            if (prev->chord_range_end < d.chord_range_end) {
                d.chord_range_end = bar_start + quantize_to_small(prev->chord_range_end - bar_start, d.quant);
            }
            if (!prev->can_merge_with_prev && d.chord_range_end == prev->chord_range_end) {
                d.chord_range_end -= d.quant;
            }
        }
        if (d.chord_range_end < d.chord_range_start) {
            d.chord_range_end = d.chord_range_start;
        }

        assert(d.chord_range_end < range_end);
        assert(d.chord_range_start <= d.chord_range_end);
        assert(((d.chord_range_end - bar_start) / d.quant).reduced().denominator() == 1);
        assert(((d.chord_range_end - d.chord_range_start) / d.quant).reduced().denominator() == 1);
    }
}

void find_chord_range_starts(std::vector<QuantData> &data, const ReducedFraction &range_start, const ReducedFraction &range_end,
                             const ReducedFraction &bar_start, const ReducedFraction &beat_len) {
    for (auto it = data.begin(); it != data.end(); ++it) {
        QuantData &d = *it;
        while (true) {
            d.chord_range_start = bar_start + quantize_to_large(range_start - bar_start, d.quant);

            assert(d.chord->first - beat_len <= range_end);

            if (d.chord->first - beat_len > range_start) {
                d.chord_range_start = bar_start + quantize_to_large(d.chord->first - beat_len - bar_start, d.quant);
            }

            if (it != data.begin()) {
                const auto prev = std::prev(it);
                if (prev->chord_range_start > d.chord_range_start) {
                    d.chord_range_start = bar_start + quantize_to_large(prev->chord_range_start - bar_start, d.quant);
                }
                if (!d.can_merge_with_prev && d.chord_range_start == prev->chord_range_start) {
                    d.chord_range_start += d.quant;
                }
            }

            if (d.chord_range_start >= range_end) {
                if (d.quant >= MChord::min_allowed_duration() * 2) {
                    d.quant /= 2;
                } else {
                    d.can_merge_with_prev = true;
                }
                continue;
            }
            break;
        }
#ifndef NDEBUG
        assert(not_less_than_prev(it, data));
        assert(d.chord_range_start >= range_start);
        assert(d.chord_range_start < range_end);
        assert(((d.chord_range_start - bar_start) / d.quant).reduced().denominator() == 1);
#endif
    }
}

void find_quants(std::vector<QuantData> &data, const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords,
                 const ReducedFraction &range_start, const ReducedFraction &range_end, const ReducedFraction &basic_quant,
                 const ReducedFraction &tuplet_quant, const ReducedFraction &bar_fraction) {
    for (auto it = chords.begin(); it != chords.end(); ++it) {
        const auto chord_it = *it;
        QuantData d;
        d.chord = chord_it;
        auto len = MChord::min_note_len(*chord_it);
        if (range_end - range_start < len) {
            len = range_end - range_start;
        }
        if (it != chords.begin()) {
            const auto prev_chord_it = *std::prev(it);
            if (chord_it->first - prev_chord_it->first < len) {
                len = chord_it->first - prev_chord_it->first;
            }
            if (len < MChord::min_allowed_duration()) {
                d.can_merge_with_prev = true;
            }
        }
        if (tuplet_quant != ReducedFraction(-1, 1)) {
            const MidiTuplet::TupletData &tuplet = (*chords.begin())->second.tuplet->second;
            d.quant = tuplet_quant;
            d.quant_for_len = tuplet.len / tuplet.tuplet_number;
        } else {
            d.quant = quant_for_len(len, basic_quant);
            auto max_quant = basic_quant;
            while (max_quant < bar_fraction) {
                max_quant *= 2;
            }
            d.quant_for_len = quant_for_len(std::min(MChord::min_note_len(*chord_it), range_end - range_start), max_quant);
        }

        assert(d.quant <= range_end - range_start);

        data.push_back(d);
    }
}

ReducedFraction find_tuplet_quant(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords) {
    ReducedFraction tuplet_quant(-1, 1);
    if ((*chords.begin())->second.is_in_tuplet) {
        const MidiTuplet::TupletData &tuplet = (*chords.begin())->second.tuplet->second;
        const auto tuplet_ratio = MidiTuplet::tuplet_limits(tuplet.tuplet_number).ratio;
        tuplet_quant = quant_for_tuplet(tuplet.len, tuplet_ratio);
    }

    return tuplet_quant;
}

std::vector<QuantData> find_quant_data(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords,
                                       const ReducedFraction &range_start, const ReducedFraction &range_end,
                                       const ReducedFraction &basic_quant, const ReducedFraction &bar_start,
                                       const ReducedFraction &bar_fraction) {
    assert(!chords.empty());

    std::vector<QuantData> data;
    const auto tuplet_quant = find_tuplet_quant(chords);
    const auto beat_len = Meter::beat_length(bar_fraction);

    find_quants(data, chords, range_start, range_end, basic_quant, tuplet_quant, bar_fraction);
    find_chord_range_starts(data, range_start, range_end, bar_start, beat_len);
    find_chord_range_ends(data, range_start, range_end, bar_start, beat_len);
    find_metrical_levels(data, chords, tuplet_quant, bar_start, bar_fraction);

    return data;
}

struct QuantInfo {
    ReducedFraction on_time;
    double penalty = 0.0;
    std::multimap<ReducedFraction, MidiChord>::const_iterator chord;
};

int find_last_chord_position(const std::vector<QuantData> &quant_data) {
    int pos_index = -1;
    double min_penalty = std::numeric_limits<double>::max();
    const auto &last_positions = quant_data[quant_data.size() - 1].positions;
    for (size_t i = 0; i != last_positions.size(); ++i) {
        if (last_positions[i].penalty < min_penalty) {
            min_penalty = last_positions[i].penalty;
            pos_index = static_cast<int>(i);
        }
    }

    assert(pos_index != -1);

    return pos_index;
}

void apply_dynamic_programming(std::vector<QuantData> &quant_data) {
    const auto &opers = midi_import_operations.data()->track_opers;
    const bool is_human = opers.is_human_performance.value();
    const double merge_penalty_coeff = 5.0;

    for (size_t chord_index = 0; chord_index != quant_data.size(); ++chord_index) {
        QuantData &d = quant_data[chord_index];
        for (size_t pos = 0; pos != d.positions.size(); ++pos) {
            QuantPos &p = d.positions[pos];

            const auto time_penalty = (d.chord->first - p.time).abs_value().to_double();
            const double level_diff = std::abs(d.metrical_level_for_len - p.metrical_level);

            if (is_human) {
                if (p.metrical_level <= d.metrical_level_for_len) {
                    p.penalty = time_penalty + level_diff * d.quant_for_len.to_double();
                } else {
                    p.penalty = time_penalty / (1 + level_diff);
                }
            } else {
                if (p.metrical_level <= d.metrical_level_for_len) {
                    p.penalty = time_penalty * (1 + level_diff);
                } else {
                    p.penalty = time_penalty;
                }
            }

            if (chord_index == 0) {
                continue;
            }

            const QuantData &d_prev = quant_data[chord_index - 1];
            double min_penalty = std::numeric_limits<double>::max();
            int min_pos = -1;

            for (size_t pos_prev = 0; pos_prev != d_prev.positions.size(); ++pos_prev) {
                const QuantPos &p_prev = d_prev.positions[pos_prev];
                if (p_prev.time > p.time) {
                    continue;
                }

                double penalty = p_prev.penalty;
                if (p_prev.time == p.time) {
                    if (!d.can_merge_with_prev) {
                        continue;
                    }
                    penalty += d.quant.to_double() * merge_penalty_coeff;
                }

                if (penalty < min_penalty) {
                    min_penalty = penalty;
                    min_pos = static_cast<int>(pos_prev);
                }
            }

            assert(min_pos != -1);

            p.penalty += min_penalty;
            p.prev_pos = min_pos;
        }
    }
}

void quantize_on_times_in_range(const std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords,
                                std::map<const std::pair<const ReducedFraction, MidiChord> *, QuantInfo> &found_on_times,
                                const ReducedFraction &range_start, const ReducedFraction &range_end, const ReducedFraction &basic_quant,
                                const ReducedFraction &bar_start, const ReducedFraction &bar_fraction) {
#ifndef NDEBUG
    assert(!chords.empty());
    assert(are_all_voices_same(chords));
    assert(are_chords_different(chords));
    assert(are_tuplet_chords_consistent(chords));
    assert(are_chords_sorted_by_on_time(chords));

    assert(range_start != ReducedFraction(-1, 1) && range_end != ReducedFraction(-1, 1) && range_start < range_end);

    assert((chords.front()->second.is_in_tuplet) ? is_tuplet_range_correct(chords.front()->second.tuplet->second, range_start, range_end) :
                                                   true);
#endif
    std::vector<QuantData> quant_data = find_quant_data(chords, range_start, range_end, basic_quant, bar_start, bar_fraction);
    apply_dynamic_programming(quant_data);

    // backward dynamic programming step - collect optimal chord positions
    int pos_index = find_last_chord_position(quant_data);

    assert(quant_data.size() == chords.size());

    for (size_t chord_index = quant_data.size() - 1;; --chord_index) {
        const QuantPos &p = quant_data[chord_index].positions[pos_index];
        const auto on_time = p.time;
        const auto chord_it = chords[chord_index];

        const auto found = found_on_times.find(&*chord_it);
        if (found == found_on_times.end()) {
            QuantInfo info;
            info.chord = chord_it;
            info.on_time = on_time;
            info.penalty = p.penalty;
            found_on_times.insert({&*chord_it, info});
        } else if (p.penalty < found->second.penalty) {
            found->second.on_time = on_time;
            found->second.penalty = p.penalty;
        }

        if (chord_index == 0) {
            break;
        }
        pos_index = p.prev_pos;
    }
}

// input chords - sorted by onTime value

void apply_tuplet_staccato(std::multimap<ReducedFraction, MidiChord> &chords) {
    for (auto chord_it = chords.begin(); chord_it != chords.end(); ++chord_it) {
        for (MidiNote &note : chord_it->second.notes) {
            if (note.is_in_tuplet && note.staccato) {
                const MidiTuplet::TupletData &tuplet = note.tuplet->second;
                // decrease tuplet error by enlarging staccato notes:
                // make note.len = tuplet note length
                const auto tuplet_note_len = tuplet.len / tuplet.tuplet_number;
                note.off_time = chord_it->first + tuplet_note_len;
            }
        }
    }
}

void quantize_off_times(std::multimap<ReducedFraction, MidiChord> &quantized_chords, const ReducedFraction &basic_quant) {
    for (auto chord_it = quantized_chords.begin(); chord_it != quantized_chords.end(); ++chord_it) {
        MidiChord &chord = chord_it->second;
        // quantize off times
        for (auto &note : chord.notes) {
            // check if note is not in tuplet anymore
            if (note.is_in_tuplet) {
                if (chord_it->first >= note.tuplet->first + note.tuplet->second.len) {
                    note.is_in_tuplet = false;
                }
            }

            const auto result =
                (note.is_in_tuplet) ?
                    quantize_off_time_for_tuplet(note.off_time, chord_it, quantized_chords, basic_quant, note.tuplet->second) :
                    quantize_off_time_for_non_tuplet(note.off_time, chord_it, quantized_chords, basic_quant);
            note.off_time = result.first;
            note.off_time_quant = result.second;
#ifndef NDEBUG
            check_off_time(note, chord_it, quantized_chords);
#endif
        }
    }
}

void add_chords_from_prev_range(std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> &chords_to_quant,
                                const std::multimap<ReducedFraction, MidiChord> &chords, bool currently_in_tuplet, int current_bar_index,
                                int voice, const ReducedFraction &bar_start, const ReducedFraction &range_start,
                                const ReducedFraction &basic_quant) {
    if (currently_in_tuplet) {
        return;
    }

    auto it = chords_to_quant.front();
    if (it == chords.begin()) {
        return;
    }
    --it;

    if (range_start == bar_start) { // new bar
        while (it->second.bar_index >= current_bar_index - 1) {
            if (it->second.voice == voice && it->second.bar_index == current_bar_index) {
                assert(!it->second.is_in_tuplet);

                chords_to_quant.push_front(it);
            }
            if (it == chords.begin()) {
                break;
            }
            --it;
        }
    } else {
        const auto tol = basic_quant / 2;
        while (it->first > range_start - tol) {
            if (it->second.voice == voice && !it->second.is_in_tuplet) {
                chords_to_quant.push_front(it);
            }
            if (it == chords.begin()) {
                break;
            }
            --it;
        }
    }
}

void quantize_on_times(const std::multimap<ReducedFraction, MidiChord> &chords,
                       std::map<const std::pair<const ReducedFraction, MidiChord> *, QuantInfo> &found_on_times,
                       const ReducedFraction &basic_quant, const TimeSigMap *sigmap) {
    int max_voice = 0;
    for (int voice = 0; voice <= max_voice; ++voice) {
        int current_bar_index = -1;
        ReducedFraction range_start(-1, 1);
        ReducedFraction range_end(-1, 1);
        ReducedFraction bar_fraction(-1, 1);
        ReducedFraction bar_start(-1, 1);
        bool currently_in_tuplet = false;
        std::deque<std::multimap<ReducedFraction, MidiChord>::const_iterator> chords_to_quant;

        for (auto chord_it = chords.begin(); chord_it != chords.end(); ++chord_it) {
            assert(MChord::min_note_len(*chord_it) >= MChord::min_allowed_duration());

            if (chord_it->second.voice > max_voice) {
                max_voice = chord_it->second.voice;
            }
            if (chord_it->second.voice != voice) {
                continue;
            }

            if (chords_to_quant.empty()) {
                range_start = range_end;
                currently_in_tuplet = chord_it->second.is_in_tuplet;
                if (current_bar_index != chord_it->second.bar_index) {
                    current_bar_index = chord_it->second.bar_index;
                    bar_start = ReducedFraction::from_ticks(sigmap->bar2tick(current_bar_index, 0));
                    bar_fraction = ReducedFraction(sigmap->timesig(bar_start.ticks()).timesig());
                    if (!currently_in_tuplet) {
                        range_start = bar_start;
                    }
                }
                if (currently_in_tuplet) {
                    const auto &tuplet = chord_it->second.tuplet->second;
                    range_start = tuplet.on_time;
                    range_end = tuplet.on_time + tuplet.len;
                }
            }

            chords_to_quant.push_back(chord_it);

            auto next_chord = std::next(chord_it);
            while (next_chord != chords.end() && next_chord->second.voice != voice) {
                ++next_chord;
            }
            if (next_chord == chords.end() || next_chord->second.bar_index != current_bar_index ||
                next_chord->second.is_in_tuplet != currently_in_tuplet ||
                (next_chord->second.is_in_tuplet && currently_in_tuplet && next_chord->second.tuplet != chord_it->second.tuplet)) {
                if (!currently_in_tuplet) {
                    if (next_chord != chords.end()) {
                        if (next_chord->second.bar_index != current_bar_index) {
                            range_end = ReducedFraction::from_ticks(sigmap->bar2tick(current_bar_index + 1, 0));
                        } else if (next_chord->second.is_in_tuplet) {
                            range_end = next_chord->second.tuplet->second.on_time;
                        }
                    } else {
                        range_end = ReducedFraction::from_ticks(sigmap->bar2tick(current_bar_index + 1, 0));
                    }
                }

                add_chords_from_prev_range(chords_to_quant, chords, currently_in_tuplet, current_bar_index, voice, bar_start, range_start,
                                           basic_quant);

                quantize_on_times_in_range(chords_to_quant, found_on_times, range_start, range_end, basic_quant, bar_start, bar_fraction);
                chords_to_quant.clear();
            }
        }
    }
}

// if on time after quantization become >= note off time
// then shift note off time preserving old note length

void move_off_times(const ReducedFraction &old_on_time, const ReducedFraction &new_on_time, std::vector<MidiNote> &notes) {
    for (auto &note : notes) {
        if (new_on_time >= note.off_time) {
            assert(new_on_time > old_on_time);
            assert(note.off_time > old_on_time);

            note.off_time += new_on_time - old_on_time;
        }
    }
}

std::multimap<ReducedFraction, MidiChord>
find_quantized_chords(const std::map<const std::pair<const ReducedFraction, MidiChord> *, QuantInfo> &found_on_times) {
    std::multimap<ReducedFraction, MidiChord> quantized_chords;
    for (const auto &f : found_on_times) {
        const QuantInfo &i = f.second;
        const MidiChord &chord = i.chord->second;

        auto found = quantized_chords.end();
        const auto range = quantized_chords.equal_range(i.on_time);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second.voice == chord.voice) {
                found = it;
                break;
            }
        }
        if (found != quantized_chords.end()) {
            MidiChord &fc = found->second;
            fc.notes.insert(fc.notes.end(), chord.notes.begin(), chord.notes.end()); // merge chords with equal on times
            move_off_times(i.chord->first, i.on_time, fc.notes);
            if (chord.is_in_tuplet) {
                if (!fc.is_in_tuplet) {
                    fc.is_in_tuplet = true;
                    fc.tuplet = chord.tuplet;
                }
#ifndef NDEBUG
                else {
                    assert(fc.tuplet == chord.tuplet);
                }
#endif
            }
        } else {
            MidiChord midi_chord(chord); // copy chord
            move_off_times(i.chord->first, i.on_time, midi_chord.notes);
            quantized_chords.insert({i.on_time, midi_chord});
        }
    }

    return quantized_chords;
}

void remove_useless_tuplet_references(std::multimap<ReducedFraction, MidiChord> &chords) {
    for (auto &chord : chords) {
        if (chord.second.is_in_tuplet) {
            const auto &tuplet = chord.second.tuplet->second;
            if (chord.first >= tuplet.on_time + tuplet.len) {
                chord.second.is_in_tuplet = false;
            }
        }
        for (auto &note : chord.second.notes) {
            if (note.is_in_tuplet) {
                const auto &tuplet = note.tuplet->second;
                if (note.off_time <= tuplet.on_time) {
                    note.is_in_tuplet = false;
                }
            }
        }
    }
}

void quantize_chords(std::multimap<ReducedFraction, MidiChord> &chords, const TimeSigMap *sigmap, const ReducedFraction &basic_quant) {
#ifndef NDEBUG
    assert(MidiTuplet::are_tuplet_references_valid(chords));
    assert(are_on_time_values_different(chords));
    assert(are_tuplet_chords_consistent(chords));
    assert(MChord::are_notes_long_enough(chords));
    assert(MChord::are_bar_indexes_successive(chords));
#endif
    apply_tuplet_staccato(chords); // apply staccato for tuplet off times
    std::map<const std::pair<const ReducedFraction, MidiChord> *, QuantInfo> found_on_times;
    quantize_on_times(chords, found_on_times, basic_quant, sigmap);
    auto quantized_chords = find_quantized_chords(found_on_times);
#ifndef NDEBUG
    assert(MidiTuplet::are_tuplet_references_valid(quantized_chords));
#endif
    quantize_off_times(quantized_chords, basic_quant);
    std::swap(chords, quantized_chords);

    remove_useless_tuplet_references(chords);
}
}} // namespace mu::iex::midi::Quantize
