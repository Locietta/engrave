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
#include "bendbuilder.h"

#include <algorithm>

#include <engraving/dom/chord.h>
#include <engraving/dom/guitarbend.h>
#include <engraving/dom/factory.h>
#include <engraving/dom/measure.h>
#include <engraving/dom/note.h>
#include <engraving/dom/score.h>
#include <engraving/dom/part.h>
#include <engraving/dom/staff.h>
#include <engraving/dom/stringdata.h>
#include <engraving/dom/tie.h>
#include <engraving/dom/tuplet.h>

#include "../utils.h"

#include "global/log.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
static std::vector<Chord *> create_grace_chords(Chord *chord, const grace_segment_map_t &bend_info);
static void reindex_grace_notes(Chord *chord);
static void create_slight_bends(const BendDataContext &bend_data_ctx, Score *score);
static void create_pre_bends(const BendDataContext &bend_data_ctx, Score *score);
static void create_pre_dives(const DiveDataContext &dive_data_ctx, Score *score);
static void create_grace_after_notes(const GraceAfterTrackMap &data, GuitarBendType type, Score *score);
static void create_tied_spans(const TiedNotesTrackMap &data, GuitarBendType type, Score *score);

void BendBuilder::add_elements_to_score(Score *score, const BendDataContext &bend_ctx, const DiveDataContext &dive_ctx) {
    create_pre_bends(bend_ctx, score);
    create_slight_bends(bend_ctx, score);
    create_grace_after_notes(bend_ctx.grace_after_bend_data, GuitarBendType::BEND, score);
    create_tied_spans(bend_ctx.tied_notes_bends_data, GuitarBendType::BEND, score);
    create_pre_dives(dive_ctx, score);
    create_tied_spans(dive_ctx.tied_notes_dives_data, GuitarBendType::DIVE, score);
    create_grace_after_notes(dive_ctx.grace_after_dive_data, GuitarBendType::DIVE, score);

    if (bend_ctx.split_chord_ctx) {
        bend_data_processor_split_chord_ = std::make_unique<BendDataProcessorSplitChord>(score);
        bend_data_processor_split_chord_->process_bends(*bend_ctx.split_chord_ctx);
    }
}

static void create_slight_bends(const BendDataContext &bend_data_ctx, mu::engraving::Score *score) {
    for (const auto &[track, track_info] : bend_data_ctx.slight_bend_data) {
        for (const auto &[tick, tick_info] : track_info) {
            Chord *chord = utils::get_located_chord(score, tick, track);
            for (size_t note_index = 0; note_index < chord->notes().size(); note_index++) {
                if (!muse::contains(tick_info, note_index)) {
                    continue;
                }

                Note *note = chord->notes()[note_index];
                const auto &note_bend_data = tick_info.at(note_index);
                GuitarBend *bend = chord->score()->add_guitar_bend(GuitarBendType::SLIGHT_BEND, note);
                IF_ASSERT_FAILED(bend) {
                    LOGE() << "bend wasn't created for track " << chord->track() << ", tick " << chord->tick().ticks();
                    continue;
                }

                bend->set_start_time_factor(note_bend_data.start_factor);
                bend->set_end_time_factor(note_bend_data.end_factor);
            }
        }
    }
}

static void create_pre_bends(const BendDataContext &bend_data_ctx, mu::engraving::Score *score) {
    for (const auto &[track, track_info] : bend_data_ctx.prebend_data) {
        for (const auto &[tick, tick_info] : track_info) {
            Chord *chord = utils::get_located_chord(score, tick, track);
            for (size_t note_index = 0; note_index < chord->notes().size(); note_index++) {
                if (!muse::contains(tick_info, note_index)) {
                    continue;
                }

                Note *note = chord->notes()[note_index];
                // NOTE: addGuitarBend will create grace note and transpose it down. We need the pitch of current note instead to properly fret the note.
                note->transpose_diatonic(1, true, false);
                const auto &note_bend_data = tick_info.at(note_index);
                GuitarBend *bend = chord->score()->add_guitar_bend(GuitarBendType::PRE_BEND, note);
                IF_ASSERT_FAILED(bend) {
                    LOGE() << "prebend wasn't created for track " << chord->track() << ", tick " << chord->tick().ticks();
                    continue;
                }

                // NOTE: returning pitch.
                note->transpose_diatonic(-1, true, false);
                bend->set_start_time_factor(note_bend_data.start_factor);
                bend->set_end_time_factor(note_bend_data.end_factor);

                const int pitch = note_bend_data.quarter_tones / 2;
                note->set_pitch(note->pitch() + pitch);
                note->set_tpc_from_pitch();
                int quarter_off = note_bend_data.quarter_tones % 2;
                bend->set_end_note_pitch(note->pitch(), quarter_off);
                Note *start_note = bend->start_note();
                if (!start_note) {
                    return;
                }

                start_note->set_pitch(note->pitch() - pitch);
                start_note->set_tpc_from_pitch();
                if (note->display_fret() == Note::DisplayFretOption::HIDE) {
                    start_note->set_display_fret(Note::DisplayFretOption::HIDE);
                }

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
    }
}

static void reindex_grace_notes(Chord *chord) {
    int gi = 0;
    for (Chord *c : chord->grace_notes()) {
        c->set_grace_index(gi++);
    }
}

static Chord *create_grace_after_chord(Chord *parent) {
    Chord *grace_chord = Factory::create_chord(parent->score()->dummy());
    grace_chord->set_track(parent->track());
    grace_chord->set_note_type(NoteType::GRACE8_AFTER);
    grace_chord->set_no_stem(true);
    grace_chord->set_beam_mode(BeamMode::NONE);

    TDuration dur;
    dur.set_val(mu::engraving::Constants::division / 2);
    grace_chord->set_duration_type(dur);
    grace_chord->set_ticks(dur.fraction());
    parent->add(grace_chord);

    return grace_chord;
}

static std::vector<Chord *> create_grace_chords(Chord *chord, const grace_segment_map_t &bend_info) {
    std::vector<Chord *> grace_chords;

    size_t max_grace_amount = 0;
    for (size_t note_index = 0; note_index < chord->notes().size(); note_index++) {
        if (!muse::contains(bend_info, note_index)) {
            continue;
        }
        max_grace_amount = std::max(max_grace_amount, bend_info.at(note_index).data.size());
    }

    for (size_t i = 0; i < max_grace_amount; i++) {
        grace_chords.push_back(create_grace_after_chord(chord));
    }

    return grace_chords;
}

static void create_grace_after_notes(const GraceAfterTrackMap &data, GuitarBendType type, Score *score) {
    for (const auto &[track, track_info] : data) {
        for (const auto &[tick, tick_entry] : track_info) {
            Chord *chord = utils::get_located_chord(score, tick, track);
            std::vector<Chord *> grace_chords = create_grace_chords(chord, tick_entry);

            for (size_t note_index = 0; note_index < chord->notes().size(); note_index++) {
                if (!muse::contains(tick_entry, note_index)) {
                    continue;
                }

                Note *main_note = chord->notes()[note_index];
                const auto &grace_data = tick_entry.at(note_index);
                const auto &last_grace_data = grace_data.last_note_data;

                const bool is_dive = (type == GuitarBendType::DIVE);

                Note *current_note = main_note;
                for (size_t grace_index = 0; grace_index < grace_data.data.size(); grace_index++) {
                    const auto &note_data = grace_data.data[grace_index];
                    Chord *grace_chord = grace_chords[grace_index];

                    Note *grace_note = Factory::create_note(grace_chord);
                    const int grace_pitch = current_note->pitch() + note_data.quarter_tones / 2;
                    grace_note->set_pitch(grace_pitch);
                    grace_note->set_tpc_from_pitch();
                    grace_chord->add(grace_note);

                    if (is_dive) {
                        grace_note->set_string(main_note->string());
                        const Staff *staff = main_note->staff();
                        const StringData *sd = main_note->part()->string_data(main_note->tick(), staff->idx());
                        const int offset = staff->pitch_offset(main_note->tick());
                        const int fret = sd->fret(grace_pitch + offset, main_note->string(), staff);
                        if (fret >= 0) {
                            grace_note->set_fret(fret);
                        }
                    }

                    GuitarBend *bend = score->add_guitar_bend(type, current_note, grace_note);
                    IF_ASSERT_FAILED(bend) {
                        LOGE() << "grace-after spanner not created for track " << track << ", tick " << tick.ticks();
                        break;
                    }

                    // Bend quarterTones are absolute from chain start, dive quarterTones are deltas
                    const int end_pitch = is_dive ? current_note->pitch() + note_data.quarter_tones / 2 :
                                                    bend->start_note_of_chain()->pitch() + note_data.quarter_tones / 2;

                    bend->set_end_note_pitch(end_pitch, note_data.quarter_tones % 2);
                    bend->set_start_time_factor(note_data.start_factor);
                    bend->set_end_time_factor(note_data.end_factor);

                    current_note = grace_note;
                }

                reindex_grace_notes(main_note->chord());

                if (last_grace_data.should_move_tie) {
                    Tie *tie_for = main_note->tie_for();
                    if (tie_for && tie_for->end_note()) {
                        Note *tied_note = tie_for->end_note();
                        main_note->remove(tie_for);
                        GuitarBend *tie_bend = score->add_guitar_bend(type, current_note, tied_note);
                        if (tie_bend) {
                            tie_bend->set_end_time_factor(last_grace_data.end_factor);
                        }
                    }
                }
            }
        }
    }
}

static void create_pre_dives(const DiveDataContext &dive_data_ctx, Score *score) {
    for (const auto &[track, track_info] : dive_data_ctx.pre_dive_data) {
        for (const auto &[tick, tick_info] : track_info) {
            Chord *chord = utils::get_located_chord(score, tick, track);
            if (!chord) {
                continue;
            }

            for (size_t note_index = 0; note_index < chord->notes().size(); note_index++) {
                if (!muse::contains(tick_info, note_index)) {
                    continue;
                }

                Note *note = chord->notes()[note_index];
                const SegmentData &pd = tick_info.at(note_index);

                const int written_pitch = note->pitch();
                const int original_string = note->string();
                const int original_fret = note->fret();
                const int attack_pitch = written_pitch - pd.quarter_tones / 2;

                note->transpose_diatonic(-1, true, false);
                GuitarBend *pre_dive = score->add_guitar_bend(GuitarBendType::PRE_DIVE, note);
                note->transpose_diatonic(1, true, false);

                IF_ASSERT_FAILED(pre_dive) {
                    LOGE() << "pre-dive not created for track " << track << ", tick " << tick.ticks();
                    continue;
                }

                Note *ghost_note = pre_dive->start_note();
                if (ghost_note) {
                    ghost_note->set_pitch(written_pitch);
                    ghost_note->set_tpc_from_pitch();
                    ghost_note->set_string(original_string);
                    ghost_note->set_fret(original_fret);
                }

                note->set_pitch(attack_pitch);
                note->set_tpc_from_pitch();
                note->set_string(original_string);
                note->set_fret(original_fret);
            }
        }
    }
}

static void create_tied_spans(const TiedNotesTrackMap &data, GuitarBendType type, Score *score) {
    const bool is_dive = (type == GuitarBendType::DIVE);

    for (const auto &[track, track_info] : data) {
        for (const auto &[tick, tick_entry] : track_info) {
            Chord *chord = utils::get_located_chord(score, tick, track);
            if (!chord) {
                continue;
            }

            for (size_t note_index = 0; note_index < chord->notes().size(); note_index++) {
                if (!muse::contains(tick_entry, note_index)) {
                    continue;
                }

                const auto &note_info = tick_entry.at(note_index);
                Note *start_note = chord->notes()[note_index];
                Note *end_note = start_note->tie_for() ? start_note->tie_for()->end_note() : nullptr;
                if (!end_note) {
                    LOGE() << "bend/dive import error: not found tied note for track " << track << ", tick " << tick.ticks();
                    continue;
                }

                Tie *tie = start_note->tie_for();
                if (tie) {
                    start_note->remove(tie);
                }

                GuitarBend *bend = score->add_guitar_bend(type, start_note, end_note);
                IF_ASSERT_FAILED(bend) {
                    LOGE() << "bend/dive wasn't created for track " << chord->track() << ", tick " << chord->tick().ticks();
                    continue;
                }

                if (is_dive && start_note->tie_back()) {
                    start_note->set_pitch(start_note->tie_back()->start_note()->pitch());
                    start_note->set_tpc_from_pitch();
                }

                int root_pitch = is_dive ? start_note->pitch() : bend->start_note_of_chain()->pitch();

                bend->set_end_note_pitch(root_pitch + note_info.quarter_tones / 2, note_info.quarter_tones % 2);
                bend->set_start_time_factor(note_info.start_factor);
                bend->set_end_time_factor(note_info.end_factor);
                end_note->set_parentheses_mode(ParenthesesMode::BOTH);

                Tie *end_tie = end_note->tie_for();
                while (end_tie) {
                    Note *next_note = end_tie->end_note();
                    IF_ASSERT_FAILED(next_note) {
                        LOGE() << "bend/dive import error: not found tied note for track " << track << ", tick " << tick.ticks();
                        break;
                    }
                    next_note->set_pitch(end_note->pitch());
                    next_note->set_tpc_from_pitch();
                    next_note->set_parentheses_mode(ParenthesesMode::BOTH);
                    end_tie = next_note->tie_for();
                }
            }
        }
    }
}
} // namespace mu::iex::guitarpro
