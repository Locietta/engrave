/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "benddatacollector.h"

#include "../utils.h"
#include "bendinfoconverter.h"
#include "log.h"
#include <engraving/dom/chord.h>
#include <engraving/dom/note.h>
#include <engraving/dom/score.h>

using namespace mu::engraving;

namespace mu::iex::guitarpro {
static void fill_bend_data_for_note(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info, int note_index_in_chord,
                                    bool is_bend_single);

static void fill_slight_bend_data(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info, int note_index_in_chord);
static void fill_prebend_data(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info, int note_index_in_chord);
static void fill_normal_bend_data(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info);

BendDataCollector::BendDataCollector(mu::engraving::Score *score) : score_(score) {}

void BendDataCollector::store_bend_data(const mu::engraving::Note *note, const mu::engraving::PitchValues &pitch_values) {
#ifdef SPLIT_CHORD_DURATIONS
    {
        if (!bend_data_collector_split_chord_) {
            bend_data_collector_split_chord_ = std::make_unique<BendDataCollectorSplitChord>();
        }

        bend_data_collector_split_chord_->store_bend_data(note, pitch_values);
    }
#else
    if (!pitch_values.empty()) {
        bend_info_for_note_[note->track()][note->tick()][note] = BendInfoConverter::fill_bend_info(note, pitch_values);
    }
#endif
}

BendDataContext BendDataCollector::collect_bend_data_context() {
    BendDataContext bend_data_ctx;

#ifdef SPLIT_CHORD_DURATIONS
    bend_data_ctx.split_chord_ctx =
        std::make_unique<BendDataContextSplitChord>(bend_data_collector_split_chord_->collect_bend_data_context());
#else
    regroup_bend_data_by_tied_chords();
    fill_bend_data_context(bend_data_ctx);
#endif

    return bend_data_ctx;
}

static void fill_bend_data_for_note(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info, int note_index_in_chord,
                                    bool is_bend_single) {
    const Note *note = imported_info.note;
    if (!note) {
        return;
    }

    if (imported_info.type == BendType::SLIGHT_BEND && is_bend_single) {
        // even though it's slight bend (1/4), we import it as normal bend, if it's followed by other bend (or hold)
        fill_slight_bend_data(bend_data_ctx, imported_info, note_index_in_chord);
        return;
    }

    if (imported_info.type == BendType::PREBEND) {
        fill_prebend_data(bend_data_ctx, imported_info, note_index_in_chord);
    }

    fill_normal_bend_data(bend_data_ctx, imported_info);
}

void fill_slight_bend_data(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info, int note_index_in_chord) {
    const Note *note = imported_info.note;
    const Chord *chord = note->chord();
    Fraction tick = chord->tick();

    segment_data_map_t &slight_bend_chord_data = bend_data_ctx.slight_bend_data[note->track()][tick];
    SegmentData slight_segment_data;
    slight_segment_data.quarter_tones = 1;

    const auto &seg = imported_info.segments.front();

    const int distance_to_bend_start = imported_info.time_offset_from_start;
    const int bend_length = seg.middle_time - seg.start_time;
    const int current_segment_length = seg.end_time - seg.start_time + distance_to_bend_start;

    slight_segment_data.start_factor = (double) distance_to_bend_start / current_segment_length;
    slight_segment_data.end_factor = (double) (distance_to_bend_start + bend_length) / current_segment_length;

    slight_bend_chord_data[note_index_in_chord] = std::move(slight_segment_data);
}

static void fill_prebend_data(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info, int note_index_in_chord) {
    const Note *note = imported_info.note;
    Fraction tick = note->tick();
    segment_data_map_t &prebend_chord_data = bend_data_ctx.prebend_data[note->track()][tick];

    SegmentData prebend_note_data;
    prebend_note_data.quarter_tones = imported_info.pitch_offset_from_start / ::mu::iex::guitarpro::gp_pitch_per_quartertone;

    prebend_chord_data[note_index_in_chord] = std::move(prebend_note_data);
}

static void fill_normal_bend_data(BendDataContext &bend_data_ctx, const ImportedBendInfo &imported_info) {
    if (imported_info.segments.empty()) {
        return;
    }

    const Note *note = imported_info.note;
    const Chord *chord = note->chord();
    track_idx_t track = chord->track();
    Fraction tick = chord->tick();

    for (size_t i = 0; i < imported_info.segments.size(); i++) {
        const auto &seg = imported_info.segments[i];
        const int offset_from_start = (i == 0) ? imported_info.time_offset_from_start : 0;

        SegmentData data;
        data.quarter_tones = seg.end_pitch / ::mu::iex::guitarpro::gp_pitch_per_quartertone;

        const int distance_to_bend_start = offset_from_start;
        const int bend_length = seg.middle_time - seg.start_time;
        const int current_segment_length = seg.end_time - seg.start_time + offset_from_start;

        data.start_factor = (double) distance_to_bend_start / current_segment_length;
        data.end_factor = (double) (distance_to_bend_start + bend_length) / current_segment_length;
        size_t note_idx = muse::index_of(note->chord()->notes(), note);

        if (imported_info.connection_type == ConnectionToNextNoteType::MAIN_NOTE_CONNECTS) {
            bend_data_ctx.tied_notes_bends_data[track][tick][note_idx] = std::move(data);
        } else if (imported_info.connection_type == ConnectionToNextNoteType::LAST_GRACE_CONNECTS &&
                   i == imported_info.segments.size() - 1) {
            LastGraceNoteData last_note_data;
            last_note_data.should_move_tie = true;
            last_note_data.end_factor = data.end_factor;
            bend_data_ctx.grace_after_bend_data[track][tick][note_idx].last_note_data = last_note_data;
        } else {
            bend_data_ctx.grace_after_bend_data[track][tick][note_idx].data.push_back(std::move(data));
        }
    }
}

void BendDataCollector::regroup_bend_data_by_tied_chords() {
    for (auto &[track, track_info] : bend_info_for_note_) {
        for (auto &[main_tick, tick_info] : track_info) {
            const Chord *main_chord = utils::get_located_chord(score_, main_tick, track);
            auto &chunk = regrouped_data_by_tied_chords_[track][main_tick];
            {
                auto &info_for_first_chord = chunk[main_tick];
                info_for_first_chord.chord = main_chord;

                for (const auto &[note, info] : tick_info) {
                    info_for_first_chord.data_by_note[note] = std::move(tick_info[note]);
                }
            }

            // filling the tied notes
            for (const auto &[note, info] : tick_info) {
                const Note *current_note = note;
                const Tie *tie_for = current_note->tie_for();
                while (tie_for) {
                    current_note = tie_for->end_note();

                    IF_ASSERT_FAILED(current_note) {
                        LOGE() << "invalid tie encountered while importing guitar bend";
                        break;
                    }

                    if (muse::contains(track_info, current_note->tick()) &&
                        muse::contains(track_info.at(current_note->tick()), current_note)) {
                        auto &note_info = track_info.at(current_note->tick()).at(current_note);
                        if (note_info.type == BendType::PREBEND) {
                            note_info.type = BendType::HOLD;
                            // rewriting first note's info about should it connect to next or not
                            auto &first_chord_data = regrouped_data_by_tied_chords_[track][main_tick].begin()->second;
                            size_t note_index = std::min(muse::index_of(current_note->chord()->notes(), current_note),
                                                         first_chord_data.chord->notes().size() - 1);
                            first_chord_data.data_by_note[first_chord_data.chord->notes()[note_index]].connection_type =
                                ConnectionToNextNoteType::MAIN_NOTE_CONNECTS;
                        } else {
                            break;
                        }
                    }

                    const Chord *current_chord = current_note->chord();
                    auto &info_for_current_chord = chunk[current_chord->tick()];

                    info_for_current_chord.chord = current_chord;
                    info_for_current_chord.data_by_note[current_note] = std::move(tick_info[current_note]);

                    tie_for = current_note->tie_for();
                }
            }
        }
    }

    bend_info_for_note_.clear();
}

void BendDataCollector::fill_bend_data_context(BendDataContext &bend_data_ctx) {
    for (auto &[track, track_info] : regrouped_data_by_tied_chords_) {
        for (auto &[tick, chunk] : track_info) {
            size_t tied_bend_notes_amount = chunk.size();

            auto &first_chord_data = chunk.begin()->second;
            const Chord *first_chord = first_chord_data.chord;

            for (size_t note_idx = 0; note_idx < first_chord->notes().size(); note_idx++) {
                const Note *main_note = first_chord->notes()[note_idx];
                if (muse::contains(first_chord_data.data_by_note, main_note)) {
                    auto &data_for_first_note = first_chord_data.data_by_note.at(main_note);
                    if (tied_bend_notes_amount >= data_for_first_note.segments.size() && tied_bend_notes_amount > 1) {
                        move_segments_to_tied_notes(chunk, data_for_first_note, note_idx);
                    } else if (main_note->tie_for()) {
                        // last grace note should connect to next tied note instead of original note of chord
                        auto &last_chord_data = chunk.rbegin()->second;
                        size_t note_index =
                            std::min(muse::index_of(main_note->chord()->notes(), main_note), last_chord_data.chord->notes().size() - 1);
                        last_chord_data.data_by_note[last_chord_data.chord->notes()[note_index]].connection_type =
                            (data_for_first_note.segments.size() > 1 ? ConnectionToNextNoteType::LAST_GRACE_CONNECTS :
                                                                       ConnectionToNextNoteType::MAIN_NOTE_CONNECTS);
                    }
                }
            }

            size_t chunk_size = chunk.size();
            for (auto &[chunk_tick, chord_info] : chunk) {
                const auto &notes_vec = chord_info.chord->notes();
                const auto &notes_data = chord_info.data_by_note;
                for (size_t i = 0; i < notes_vec.size(); ++i) {
                    Note *n = notes_vec[i];
                    if (auto it = notes_data.find(n); it != notes_data.end()) {
                        fill_bend_data_for_note(bend_data_ctx, it->second, static_cast<int>(i), chunk_size == 1);
                    }
                }
            }
        }
    }
}

void BendDataCollector::move_segments_to_tied_notes(tied_chords_bend_data_chunk_t &data_chunk, ImportedBendInfo &data_for_first_note,
                                                    size_t note_idx) {
    data_for_first_note.connection_type = ConnectionToNextNoteType::MAIN_NOTE_CONNECTS;
    const size_t segments_size = data_for_first_note.segments.size();
    size_t new_segments_size = segments_size;
    const Note *note = data_for_first_note.note;
    size_t diff = data_chunk.size() - segments_size;

    IF_ASSERT_FAILED(data_chunk.size() >= segments_size) { return; }

    auto data_chunk_it = data_chunk.begin();

    for (size_t i = 1; new_segments_size > 1; i++, new_segments_size--) {
        const bool chord_exists = i < data_chunk.size();
        data_chunk_it = std::next(data_chunk_it);
        auto &next_chord_data = data_chunk_it->second;

        if (!chord_exists || note_idx >= next_chord_data.chord->notes().size()) {
            LOGE() << "bend import error: bends data for tied notes is wrong for track " << note->track() << ", tick "
                   << note->tick().ticks();
            return;
        }

        const Note *next_note = next_chord_data.chord->notes()[note_idx];
        auto &data_for_next_note = next_chord_data.data_by_note[next_note];
        data_for_next_note.note = next_note;
        data_for_next_note.segments.push_back(std::move(data_for_first_note.segments[i]));
        data_for_next_note.connection_type = ConnectionToNextNoteType::MAIN_NOTE_CONNECTS;
    }

    if (new_segments_size < segments_size) {
        data_for_first_note.segments.resize(new_segments_size);
    }

    // in that case last note should be grace-after
    if (diff == 0) {
        auto &next_chord_data = data_chunk.rbegin()->second;
        for (const Note *last_chord_note : next_chord_data.chord->notes()) {
            if (muse::contains(next_chord_data.data_by_note, last_chord_note)) {
                next_chord_data.data_by_note[last_chord_note].connection_type = ConnectionToNextNoteType::NONE;
            }
        }
    }
}
} // namespace mu::iex::guitarpro
