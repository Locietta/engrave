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
#include "importmidi_simplify.h"

#include <algorithm>
#include <vector>

#include "../midishared/midifile.h"
#include "engraving/dom/durationtype.h"
#include "engraving/dom/sig.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_quant.h"
#include "importmidi_tuplet.h"
#include "importmidi_tuplet_voice.h"
#include "importmidi_voice.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace Simplify {
bool has_complex_beamed_durations(const std::vector<std::pair<ReducedFraction, TDuration>> &list) {
    for (const auto &d : list) {
        if (d.second == DurationType::V_16TH || d.second == DurationType::V_32ND || d.second == DurationType::V_64TH ||
            d.second == DurationType::V_128TH || d.second == DurationType::V_256TH || d.second == DurationType::V_512TH ||
            d.second == DurationType::V_1024TH) {
            return true;
        }
    }
    return false;
}

void lengthen_note(MidiNote &note, int voice, const ReducedFraction &note_on_time, const ReducedFraction &duration_start,
                   const ReducedFraction &end_time, const ReducedFraction &bar_start, const ReducedFraction &bar_fraction,
                   const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets, bool is_drum_track) {
    if (end_time <= note.off_time) {
        return;
    }

    const auto &opers = midi_import_operations.data()->track_opers;
    const int current_track = midi_import_operations.current_track();

    const bool use_dots = opers.use_dots.value(current_track);
    const auto tuplets_for_duration =
        MidiTuplet::find_tuplets_in_bar_for_duration(voice, bar_start, note.off_time, end_time - note.off_time, tuplets);

    assert(note.off_time_quant != ReducedFraction(-1, 1));

    const auto orig_note_durations = Meter::to_duration_list(duration_start - bar_start, note.off_time - bar_start, bar_fraction,
                                                             tuplets_for_duration, Meter::DurationType::NOTE, use_dots, false);

    const auto orig_rest_durations = Meter::to_duration_list(note.off_time - bar_start, end_time - bar_start, bar_fraction,
                                                             tuplets_for_duration, Meter::DurationType::REST, use_dots, false);

    // double - because can be + 0.5 for dots
    double min_note_duration_count = MidiDuration::duration_count(orig_note_durations);
    double min_rest_duration_count = MidiDuration::duration_count(orig_rest_durations);

    ReducedFraction best_off_time(-1, 1);

    for (ReducedFraction off_time = note.off_time + note.off_time_quant; off_time <= end_time; off_time += note.off_time_quant) {
        double note_duration_count = 0;
        double rest_duration_count = 0;

        const auto note_durations = Meter::to_duration_list(duration_start - bar_start, off_time - bar_start, bar_fraction,
                                                            tuplets_for_duration, Meter::DurationType::NOTE, use_dots, false);

        note_duration_count += MidiDuration::duration_count(note_durations);

        if (off_time < end_time) {
            const auto rest_durations = Meter::to_duration_list(off_time - bar_start, end_time - bar_start, bar_fraction,
                                                                tuplets_for_duration, Meter::DurationType::REST, use_dots, false);

            rest_duration_count += MidiDuration::duration_count(rest_durations);
        }

        if (note_duration_count + rest_duration_count < min_note_duration_count + min_rest_duration_count) {
            if (opers.is_human_performance.value() || is_drum_track || note_duration_count <= 1.5) {
                min_note_duration_count = note_duration_count;
                min_rest_duration_count = rest_duration_count;
                best_off_time = off_time;
            }
        }
    }

    if (best_off_time == ReducedFraction(-1, 1)) {
        return;
    }

    // check for staccato:
    //    don't apply staccato if note is tied
    //    (case noteOnTime != durationStart - another bar, for example)

    bool has_loss_of_accuracy = false;
    if (!is_drum_track) {
        const double added_part = ((best_off_time - note.off_time) / (best_off_time - duration_start)).to_double();
        const double staccato_tol = 0.3;

        if (added_part >= staccato_tol) {
            if (note_on_time == duration_start && min_note_duration_count <= 1.5) {
                note.staccato = true;
            } else {
                has_loss_of_accuracy = true;
            }
        }
    }

    // if the difference is only in one note/rest and there is some loss of accuracy -
    // discard change because it silently reduces duration accuracy
    // without significant improvement of readability

    if (!is_drum_track && !opers.is_human_performance.value() && has_loss_of_accuracy &&
        (orig_note_durations.size() + orig_rest_durations.size()) - (min_note_duration_count + min_rest_duration_count) <= 1 &&
        !has_complex_beamed_durations(orig_note_durations) && !has_complex_beamed_durations(orig_rest_durations)) {
        return;
    }

    note.off_time = best_off_time;
}

// for drum tracks note duration can be arbitrary
// so start with short duration to check different cases,
// to find the most simple one

void shorten_drum_note(MidiNote &note, const std::multimap<ReducedFraction, MidiChord>::iterator &it,
                       const std::multimap<ReducedFraction, MidiChord> &chords) {
    if (it->second.is_in_tuplet) { // if note on time is in tuplet -
                                   // set note off time to be inside that tuplet
        const auto &tuplet = it->second.tuplet->second;
        const auto len = tuplet.len / tuplet.tuplet_number;
        const auto new_off_time = std::min(it->first + len, tuplet.on_time + tuplet.len);
        if (note.off_time > new_off_time) {
            note.off_time = new_off_time;
            note.is_in_tuplet = true;
            note.tuplet = it->second.tuplet;
            note.off_time_quant = len;
        }
    } else { // if note on time is outside tuplets -
             // set note off time to be outside all tuplets
        auto next = std::next(it);
        while (next != chords.end() && next->second.voice != it->second.voice) {
            ++next;
        }
        if (next != chords.end()) {
            const auto len = ReducedFraction::from_ticks(Constants::division) / 8; // 1/32
            auto new_off_time = it->first + len;
            if (next->second.is_in_tuplet) {
                const auto &tuplet = next->second.tuplet->second;
                if (tuplet.on_time < new_off_time) {
                    new_off_time = tuplet.on_time;
                }
            }
            if (new_off_time < note.off_time) {
                note.off_time = new_off_time;
                note.is_in_tuplet = false;
                note.off_time_quant = len;
            }
        }
    }
}

void minimize_number_of_rests(std::multimap<ReducedFraction, MidiChord> &chords, const TimeSigMap *sigmap,
                              const std::multimap<ReducedFraction, MidiTuplet::TupletData> &tuplets, bool is_drum_track) {
    for (auto it = chords.begin(); it != chords.end(); ++it) {
        for (MidiNote &note : it->second.notes) {
            if (is_drum_track) {
                shorten_drum_note(note, it, chords);
            }

            const auto bar_start = MidiBar::find_bar_start(note.off_time, sigmap);
            const auto bar_fraction = ReducedFraction(sigmap->timesig(bar_start.ticks()).timesig());
            auto duration_start = (it->first > bar_start) ? it->first : bar_start;

            if (it->second.is_in_tuplet) {
                const auto &tuplet = it->second.tuplet->second;
                if (note.off_time >= tuplet.on_time + tuplet.len) {
                    duration_start = tuplet.on_time + tuplet.len;
                }
            }
            auto end_time = (bar_start == note.off_time) ? bar_start : bar_start + bar_fraction;
            if (note.is_in_tuplet) {
                const auto &tuplet = note.tuplet->second;
                if (note.off_time == tuplet.on_time + tuplet.len) {
                    continue;
                }
                if (is_drum_track) {
                    end_time = tuplet.on_time + tuplet.len;
                } else {
                    end_time = bar_start + Quantize::quantize_to_large(note.off_time - bar_start, tuplet.len / tuplet.tuplet_number);
                }
            }

            const auto beat_len = Meter::beat_length(bar_fraction);
            const auto beat_time = bar_start + Quantize::quantize_to_large(note.off_time - bar_start, beat_len);
            if (end_time > beat_time) {
                end_time = beat_time;
            }

            auto next = std::next(it);
            while (next != chords.end() && (next->second.voice != it->second.voice || next->first < note.off_time)) {
                ++next;
            }
            if (next != chords.end()) {
                if (next->first < end_time) {
                    end_time = next->first;
                }
                if (next->second.is_in_tuplet && !note.is_in_tuplet) {
                    const auto &tuplet = next->second.tuplet->second;
                    if (tuplet.on_time < end_time) {
                        end_time = tuplet.on_time;
                    }
                }
            }

            lengthen_note(note, it->second.voice, it->first, duration_start, end_time, bar_start, bar_fraction, tuplets, is_drum_track);
        }
    }
}

void simplify_durations(std::multimap<int, MTrack> &tracks, const TimeSigMap *sigmap, bool simplify_drum_tracks) {
    auto &opers = midi_import_operations;

    for (auto &track : tracks) {
        MTrack &mtrack = track.second;
        if (mtrack.mtrack->drum_track() != simplify_drum_tracks) {
            continue;
        }
        auto &chords = track.second.chords;
        if (chords.empty()) {
            continue;
        }

        if (opers.data()->track_opers.simplify_durations.value(mtrack.index_of_operation)) {
            MidiOperations::CurrentTrackSetter set_current_track{opers, mtrack.index_of_operation};
#ifndef NDEBUG
            assert(MidiTuplet::are_tuplet_ranges_ok(chords, mtrack.tuplets));
#endif

            minimize_number_of_rests(chords, sigmap, mtrack.tuplets, mtrack.mtrack->drum_track());
            // empty tuplets may appear after simplification
            MidiTuplet::remove_empty_tuplets(mtrack);
#ifndef NDEBUG
            assert(MidiTuplet::are_tuplet_ranges_ok(chords, mtrack.tuplets));
#endif
        }
    }
}

void simplify_durations_for_drums(std::multimap<int, MTrack> &tracks, const TimeSigMap *sigmap) {
    simplify_durations(tracks, sigmap, true);
}

void simplify_durations_not_drums(std::multimap<int, MTrack> &tracks, const TimeSigMap *sigmap) {
    simplify_durations(tracks, sigmap, false);
}
}} // namespace mu::iex::midi::Simplify
