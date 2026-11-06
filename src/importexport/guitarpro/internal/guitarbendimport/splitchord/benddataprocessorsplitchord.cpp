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
#include "benddataprocessorsplitchord.h"

#include "benddatacontextsplitchord.h"
#include <engraving/dom/chord.h>
#include <engraving/dom/guitarbend.h>
#include <engraving/dom/factory.h>
#include <engraving/dom/measure.h>
#include <engraving/dom/note.h>
#include <engraving/dom/score.h>
#include <engraving/dom/tie.h>
#include <engraving/dom/tuplet.h>

using namespace mu::engraving;

namespace mu::iex::guitarpro {
static void create_split_duration_bends(const BendDataContextSplitChord &bend_data_ctx, mu::engraving::Score *score);
static void create_split_duration_bends_for_chord(const BendDataContextSplitChord &bend_data_ctx, mu::engraving::Chord *chord);
static void remove_reduntant_chords(const BendDataContextSplitChord &bend_data_ctx, const mu::engraving::Score *score);

BendDataProcessorSplitChord::BendDataProcessorSplitChord(mu::engraving::Score *score) : score_(score) {}

void BendDataProcessorSplitChord::process_bends(const BendDataContextSplitChord &bend_data_ctx) {
    create_split_duration_bends(bend_data_ctx, score_);
    remove_reduntant_chords(bend_data_ctx, score_);
}

static void create_split_duration_bends(const BendDataContextSplitChord &bend_data_ctx, mu::engraving::Score *score) {
    for (const auto &[track, track_info] : bend_data_ctx.bend_chord_durations) {
        for (const auto &[main_tick, chords_durations] : track_info) {
            if (chords_durations.empty()) {
                LOGE() << "bend import error : no chord duration data for track " << track << ", tick " << main_tick;
                continue;
            }

            Fraction main_tick_fr = Fraction::from_ticks(main_tick);
            const Measure *main_chord_measure = score->tick2measure(main_tick_fr);

            if (!main_chord_measure) {
                LOGE() << "bend import error : no valid measure for track " << track << ", tick " << main_tick;
                return;
            }

            Chord *main_chord = main_chord_measure->find_chord(main_tick_fr, track);
            if (!main_chord) {
                LOGE() << "bend import error : no valid chord for track " << track << ", tick " << main_tick;
                return;
            }

            Fraction new_main_chord_duration = chords_durations.front();
            main_chord->set_ticks(new_main_chord_duration);
            main_chord->set_duration_type(TDuration(new_main_chord_duration));
            Fraction current_tick = main_chord->tick() + main_chord->ticks();
            Fraction current_actual_tick = main_chord->end_tick();
            create_split_duration_bends_for_chord(bend_data_ctx, main_chord);

            Tuplet *tuplet = main_chord->tuplet();
            for (size_t i = 1; i < chords_durations.size(); i++) {
                Measure *current_measure = score->tick2measure(current_tick);
                if (!current_measure) {
                    LOGE() << "bend import error : no valid measure for track " << track << ", tick " << current_tick.ticks();
                    return;
                }

                Segment *cur_segment = current_measure->find_segment(SegmentType::CHORD_REST, current_actual_tick);
                Chord *current_chord = nullptr;
                Fraction current_chord_duration = chords_durations[i];
                Fraction actual_duration = (tuplet ? current_chord_duration / tuplet->ratio() : current_chord_duration);

                if (cur_segment) {
                    current_chord = current_measure->find_chord(current_tick, track);
                    if (!current_chord) {
                        LOGE() << "bend import error : no valid chord for track " << track << ", tick " << current_tick.ticks();
                        return;
                    }

                    current_chord->set_ticks(current_chord_duration);
                    current_chord->set_duration_type(current_chord_duration);
                } else {
                    cur_segment = current_measure->get_segment(SegmentType::CHORD_REST, current_actual_tick);
                    current_chord = Factory::create_chord(score->dummy());
                    current_chord->set_track(track);
                    current_chord->set_ticks(current_chord_duration);
                    current_chord->set_duration_type(current_chord_duration);
                    if (tuplet) {
                        tuplet->add(current_chord);
                    }

                    cur_segment->add(current_chord);

                    for (Note *note : main_chord->notes()) {
                        Note *new_chord_note = Factory::create_note(current_chord);
                        new_chord_note->set_track(track);
                        current_chord->add(new_chord_note);
                        new_chord_note->set_pitch(note->pitch());
                        new_chord_note->set_tpc_from_pitch();
                    }
                }

                create_split_duration_bends_for_chord(bend_data_ctx, current_chord);
                current_tick += current_chord_duration;
                current_actual_tick += actual_duration;
            }
        }
    }
}

static void create_split_duration_bends_for_chord(const BendDataContextSplitChord &bend_data_ctx, mu::engraving::Chord *chord) {
    Score *score = chord->score();
    int chord_ticks = chord->tick().ticks();

    if (!muse::contains(bend_data_ctx.bend_data_by_end_tick, chord->track())) {
        return;
    }

    const auto &current_track_data = bend_data_ctx.bend_data_by_end_tick.at(chord->track());
    if (!muse::contains(current_track_data, chord_ticks)) {
        return;
    }

    const BendChordData &bend_chord_data = current_track_data.at(chord_ticks);
    const Measure *start_measure = score->tick2measure(bend_chord_data.start_tick);
    IF_ASSERT_FAILED(start_measure) {
        LOGE() << "bend import error : no valid measure for track " << chord->track() << ", tick " << bend_chord_data.start_tick.ticks();
        return;
    }

    Chord *start_chord = start_measure->find_chord(bend_chord_data.start_tick, chord->track());
    IF_ASSERT_FAILED(start_chord) {
        LOGE() << "bend import error : no valid chord for track " << chord->track() << ", tick " << bend_chord_data.start_tick.ticks();
        return;
    }

    std::vector<Note *> start_chord_notes = start_chord->notes();
    std::vector<Note *> end_chord_notes = chord->notes();

    IF_ASSERT_FAILED(start_chord_notes.size() == end_chord_notes.size()) {
        LOGE() << "bend import error: start and end chord sizes don't match for track " << chord->track() << ", tick "
               << bend_chord_data.start_tick.ticks();
        return;
    }

    std::sort(start_chord_notes.begin(), start_chord_notes.end(), [](Note *l, Note *r) { return l->pitch() < r->pitch(); });

    std::sort(end_chord_notes.begin(), end_chord_notes.end(), [](Note *l, Note *r) { return l->pitch() < r->pitch(); });
    for (size_t note_index = 0; note_index < end_chord_notes.size(); note_index++) {
        Note *start_note = start_chord_notes[note_index];
        Note *note = end_chord_notes[note_index];

        if (bend_chord_data.note_data_by_idx.find(static_cast<int>(note_index)) == bend_chord_data.note_data_by_idx.end()) {
            Tie *tie = Factory::create_tie(score->dummy());
            start_note->add(tie);
            tie->set_end_note(note);
            note->set_tie_back(tie);
            continue;
        }

        const auto &bend_note_data = bend_chord_data.note_data_by_idx.at(static_cast<int>(note_index));
        const int pitch = bend_note_data.quarter_tones / 2;

        IF_ASSERT_FAILED(start_chord != chord) {
            LOGE() << "bend import error : start and end chords are the same for track " << chord->track() << ", tick "
                   << bend_chord_data.start_tick.ticks();
            return;
        }

        GuitarBend *bend = score->add_guitar_bend(GuitarBendType::BEND, start_note, note);
        IF_ASSERT_FAILED(bend) {
            LOGE() << "bend wasn't created for track " << chord->track() << ", tick " << start_chord->tick().ticks();
            return;
        }

        int quarter_off = bend_note_data.quarter_tones % 2;
        bend->set_end_note_pitch(bend->start_note_of_chain()->pitch() + pitch, quarter_off);
        bend->set_start_time_factor(bend_note_data.start_factor);
        bend->set_end_time_factor(bend_note_data.end_factor);

        int new_pitch = note->pitch();
        Note *tied_note = nullptr;

        Tie *tie_for = start_note->tie_for();
        if (tie_for) {
            tied_note = tie_for->end_note();
            start_note->remove(tie_for);
        }

        while (tied_note) {
            tied_note->set_pitch(new_pitch);
            tied_note->set_tpc_from_pitch();
            Tie *tie = tied_note->tie_for();
            if (!tie) {
                break;
            }

            tied_note = tie->end_note();
        }
    }
}

static void remove_reduntant_chords(const BendDataContextSplitChord &bend_data_ctx, const mu::engraving::Score *score) {
    for (const auto &[track, track_info] : bend_data_ctx.reduntant_chord_ticks) {
        for (const Fraction &tick : track_info) {
            const Measure *current_measure = score->tick2measure(tick);
            IF_ASSERT_FAILED(current_measure) {
                LOGE() << "bend import error : couldn't remove invalid chord, no valid measure for track " << track << ", tick "
                       << tick.ticks();
                continue;
            }

            Segment *cur_segment = current_measure->find_segment(SegmentType::CHORD_REST, tick);
            Chord *current_chord = current_measure->find_chord(tick, track);

            IF_ASSERT_FAILED(cur_segment && current_chord) {
                LOGE() << "bend import error : couldn't remove invalid chord, segment or chord was null for track " << track << ", tick "
                       << tick.ticks();
                continue;
            }

            cur_segment->remove(current_chord);
        }
    }
}
} // namespace mu::iex::guitarpro
