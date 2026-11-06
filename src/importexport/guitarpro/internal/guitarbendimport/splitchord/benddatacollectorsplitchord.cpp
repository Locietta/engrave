/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#include "log.h"
#include "benddatacollectorsplitchord.h"

#include <engraving/dom/chord.h>
#include <engraving/dom/note.h>
#include <engraving/dom/score.h>
#include <engraving/dom/tie.h>
#include <engraving/dom/tuplet.h>

#include "../bendinfoconverter.h"
#include "bendchorddurationsplitter.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
static void fill_chord_durations_from_bend_diagram(BendDataContextSplitChord &bend_data_ctx, Fraction total_duration,
                                                   const ImportedBendInfo &imported_info);

static void fill_bend_data_for_note(BendDataContextSplitChord &bend_data_ctx, const ImportedBendInfo &imported_info,
                                    int note_index_in_chord);

void BendDataCollectorSplitChord::store_bend_data(const mu::engraving::Note *note, const mu::engraving::PitchValues &pitch_values) {
    if (!pitch_values.empty()) {
        bend_info_for_note_[note->track()][note->tick()][note] = BendInfoConverter::fill_bend_info(note, pitch_values);
    }
}

BendDataContextSplitChord BendDataCollectorSplitChord::collect_bend_data_context() {
    BendDataContextSplitChord bend_data_ctx;

    fill_bends_durations(bend_data_ctx);
    fill_bend_data(bend_data_ctx);

    return bend_data_ctx;
}

static void fill_bend_data_for_note(BendDataContextSplitChord &bend_data_ctx, const ImportedBendInfo &imported_info,
                                    int note_index_in_chord) {
    if (imported_info.segments.empty()) {
        return;
    }

    const Note *note = imported_info.note;
    const Chord *chord = note->chord();
    track_idx_t track = chord->track();

    const Fraction chord_start_tick = chord->tick();
    const auto &durations = bend_data_ctx.bend_chord_durations;
    Fraction ratio = Fraction(1, 1);

    if (const Tuplet *tuplet = chord->tuplet()) {
        ratio = tuplet->ratio();
    }

    if (durations.find(chord->track()) == durations.end()) {
        LOGE() << "bend import error : no information about chord duration for track " << track;
        return;
    }

    const auto &track_durations = durations.at(track);
    if (track_durations.find(chord_start_tick.ticks()) == track_durations.end()) {
        LOGE() << "bend import error : no information about chord duration for track " << track << " tick " << chord_start_tick.ticks();
        return;
    }

    const auto &tick_durations = track_durations.at(chord_start_tick.ticks());

    if (tick_durations.empty()) {
        LOGE() << "bend import error : chord durations are empty for track " << track << " tick " << chord_start_tick.ticks();
        return;
    }

    IF_ASSERT_FAILED(tick_durations.size() > 1) {
        LOGE() << "not possible to split durations of chord";
        return;
    }

    Fraction current_tick = chord_start_tick;
    for (size_t i = 0; i < tick_durations.size() - 1; i++) {
        if (i >= imported_info.segments.size()) {
            break;
        }

        Fraction tick_duration = tick_durations[i] / ratio;

        const auto &seg = imported_info.segments[i];

        BendChordData &bend_chord_data = bend_data_ctx.bend_data_by_end_tick[note->track()][(current_tick + tick_duration).ticks()];
        bend_chord_data.start_tick = current_tick;
        SegmentData bend_note_data;
        bend_note_data.quarter_tones = seg.end_pitch / ::mu::iex::guitarpro::gp_pitch_per_quartertone;
        bend_chord_data.note_data_by_idx[note_index_in_chord] = std::move(bend_note_data);

        current_tick += tick_duration;
    }
}

void BendDataCollectorSplitChord::fill_bend_data(BendDataContextSplitChord &bend_data_ctx) {
    for (auto &[track, track_info] : bend_info_for_note_) {
        for (auto &[tick, tick_info] : track_info) {
            for (auto &[note, data_by_note] : tick_info) {
                Chord *chord = note->chord();
                int idx = static_cast<int>(muse::index_of(chord->notes(), note));
                fill_bend_data_for_note(bend_data_ctx, data_by_note, idx);
            }
        }
    }
}

void BendDataCollectorSplitChord::fill_bends_durations(BendDataContextSplitChord &bend_data_ctx) {
    std::unordered_map<mu::engraving::track_idx_t, std::map<Fraction, std::vector<const Chord *>>> tied_notes_after_bend;
    for (const auto &[track, track_info] : bend_info_for_note_) {
        for (const auto &[main_tick, tick_info] : track_info) {
            // now using the top note of chord to fill durations
            const Note *current_note = tick_info.begin()->first;
            auto &current_tick_tied_chords = tied_notes_after_bend[track][main_tick];
            current_tick_tied_chords.push_back(current_note->chord());
            const Tie *tie_for = current_note->tie_for();

            bool delete_last_tied_chord = true;
            while (tie_for) {
                current_note = tie_for->end_note();

                IF_ASSERT_FAILED(current_note) {
                    LOGE() << "invalid tie encountered while importing guitar bend";
                    break;
                }

                if (track_info.find(current_note->tick()) != track_info.end()) {
                    delete_last_tied_chord = false;
                    bend_data_ctx.chord_ticks_for_tie_back[track].insert(current_note->tick());
                    break;
                }

                tied_notes_after_bend[track][main_tick].push_back(current_note->chord());
                tie_for = current_note->tie_for();
            }

            for (size_t i = 1; i < current_tick_tied_chords.size(); i++) {
                if (i != current_tick_tied_chords.size() - 1 || delete_last_tied_chord) {
                    bend_data_ctx.reduntant_chord_ticks[track].insert(current_tick_tied_chords[i]->tick());
                }
            }
        }
    }

    auto is_power_of_two = [](int n) { return n > 0 && (n & (n - 1)) == 0; };

    for (const auto &[track, track_info] : tied_notes_after_bend) {
        const auto &imported_info_for_track = bend_info_for_note_.at(track);
        for (const auto &[main_tick, chords] : track_info) {
            const auto &imported_info_for_tick = imported_info_for_track.at(main_tick);
            const Chord *last_chord = chords.back();
            Fraction total_duration = last_chord->end_tick() - main_tick;
            if (Tuplet *tuplet = last_chord->tuplet()) {
                total_duration *= tuplet->ratio();
            }

            total_duration.reduce();
            IF_ASSERT_FAILED(!total_duration.negative() && is_power_of_two(total_duration.denominator())) {
                LOGE() << "bend import error: duration is wrong for track " << track << ", tick " << main_tick << "("
                       << total_duration.numerator() << " / " << total_duration.denominator() << ")";
                continue;
            }

            if (!imported_info_for_tick.empty()) {
                fill_chord_durations_from_bend_diagram(bend_data_ctx, total_duration, imported_info_for_tick.begin()->second);
            }
        }
    }

    for (const auto &[track, track_info] : bend_data_ctx.bend_chord_durations) {
        if (bend_data_ctx.reduntant_chord_ticks.find(track) == bend_data_ctx.reduntant_chord_ticks.end()) {
            continue;
        }

        for (const auto &[main_tick, durations] : track_info) {
            Fraction current_tick = Fraction::from_ticks(main_tick);
            bend_data_ctx.reduntant_chord_ticks.at(track).erase(current_tick);

            for (const Fraction &duration : durations) {
                current_tick += duration;
                bend_data_ctx.reduntant_chord_ticks.at(track).erase(current_tick);
            }
        }
    }
}

static int get_max_denominator_for_split(const Fraction &duration) {
    int denominator = 1;

    while (Fraction(1, denominator * 2) > duration) {
        denominator *= 2;
    }

    return denominator * 2;
}

static std::vector<Fraction> splitted_durations(const ImportedBendInfo &imported_info, Fraction total_duration) {
    const auto &bend_segments = imported_info.segments;

    std::vector<Fraction> proportions;

    const int max_denominator = get_max_denominator_for_split(total_duration / 4);

    for (size_t i = 0; i < bend_segments.size(); i++) {
        const auto &seg = bend_segments[i];
        if (seg.end_time != seg.start_time) {
            Fraction target_proportion(seg.end_time - seg.start_time, PitchValue::max_time);
            target_proportion.reduce();
            proportions.push_back(target_proportion);
        }
    }

    Fraction proportions_sum = std::accumulate(proportions.begin(), proportions.end(), Fraction(0, 1));

    if (proportions_sum < Fraction(1, 1)) {
        Fraction last_proportion = Fraction(1, 1) - proportions_sum;
        proportions.push_back(last_proportion);
    }

    return BendChordDurationSplitter::find_valid_note_split(total_duration, proportions, max_denominator);
}

static void split_bend_chord_durations(BendDataContextSplitChord &bend_data_ctx, Fraction total_duration,
                                       const ImportedBendInfo &imported_info) {
    const Chord *chord = imported_info.note->chord();
    bend_data_ctx.bend_chord_durations[chord->track()][chord->tick().ticks()] = splitted_durations(imported_info, total_duration);
}

static void fill_chord_durations_from_bend_diagram(BendDataContextSplitChord &bend_data_ctx, Fraction total_duration,
                                                   const ImportedBendInfo &imported_info) {
    if (imported_info.segments.empty()) {
        return;
    }

    const Note *note = imported_info.note;
    IF_ASSERT_FAILED(note) {
        LOGE() << "couldn't fill chord durations: note is NULL";
        return;
    }

    split_bend_chord_durations(bend_data_ctx, total_duration, imported_info);
}
} // namespace mu::iex::guitarpro
