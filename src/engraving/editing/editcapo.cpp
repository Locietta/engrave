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

#include "editcapo.h"

#include "dom/measure.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/staff.h"
#include "dom/stringdata.h"

namespace mu::engraving {
// static
void EditCapo::apply_capo_transpose(int start_tick, int end_tick, UpdateCtx &ctx) {
    staff_idx_t staff_idx = ctx.staff->idx();
    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;

    for (MeasureBase *mb = ctx.staff->score()->measures()->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        for (Segment &seg : to_measure(mb)->segments()) {
            if (!seg.is_chord_rest_type()) {
                continue;
            }
            for (track_idx_t track = start_track; track < end_track; ++track) {
                EngravingItem *e = seg.element(track);
                if (!e || !e->is_chord()) {
                    continue;
                }
                if (e->tick().ticks() < start_tick || (-1 != end_tick && e->tick().ticks() >= end_tick)) {
                    continue;
                }

                Chord *chord = to_chord(e);
                chord->sort_notes();
                ctx.string_data = chord->origin_part()->string_data(chord->tick(), staff_idx);
                // Prefer not change strings for intervals and chords
                ctx.possible_fret_conflict =
                    chord->notes().size() > 1 || std::any_of(chord->notes().begin(), chord->notes().end(),
                                                             [&](const Note *n) { return n->fret() < ctx.params.fret_position; });

                for (auto note : chord->notes()) {
                    if (ctx.update_ignored_strings) {
                        update_string(note, ctx);
                    } else {
                        update(note, ctx);
                    }
                    if (const GuitarBend *bend = note->bend_for(); bend) {
                        if (GuitarBendType::BEND == bend->bend_type()) {
                            Note *start_note = bend->start_note();
                            Note *end_note = bend->end_note();
                            const StringData *string_data = ctx.string_data;
                            const int start_string = start_note->string();
                            const int start_fret =
                                string_data->fret(start_note->pitch(), start_string, start_note->staff(), start_note->tick());
                            if (start_fret != start_note->fret()) {
                                start_note->set_fret(start_fret);
                            }

                            if (end_note->fret() != start_fret) {
                                end_note->set_string(start_string);
                                end_note->set_fret(start_fret);
                            }
                            GuitarBend::fix_notes_fretting_for_standard_bend(bend->start_note(), bend->end_note());
                        }
                    }
                }

                for (Chord *g : chord->grace_notes()) {
                    for (Note *n : g->notes()) {
                        if (ctx.update_ignored_strings) {
                            update_string(n, ctx);
                        } else {
                            update(n, ctx);
                        }
                    }
                }
            }
        }
    }
}

// static
void EditCapo::update_notation_for_capo_change(const CapoParams &old_params, const CapoParams &new_params, const Staff *staff,
                                               int start_tick, int end_tick) {
    UpdateCtx ctx;
    ctx.staff = staff;
    ctx.params = new_params;
    // These two have to be set in applyCapoTranspose for every chord!
    // ctx.stringData = nullptr;
    // ctx.possibleFretConflict = false;

    Score *score = staff->score();
    Fraction layout_end_tick = end_tick != -1 ? Fraction::from_ticks(end_tick) : Fraction::max();
    score->set_layout(layout_end_tick, staff->idx());

    const bool mode_changed = old_params.transpose_mode != new_params.transpose_mode;
    const bool fret_changed = old_params.fret_position != new_params.fret_position;
    const bool strings_changed = old_params.ignored_strings != new_params.ignored_strings;
    const bool active_changed = old_params.active != new_params.active;

    if (mode_changed) {
        handle_mode_change(old_params, new_params, start_tick, end_tick, ctx);
        return;
    }
    if (fret_changed) {
        handle_fret_change(old_params, new_params, start_tick, end_tick, ctx);
        return;
    }
    if (strings_changed) {
        handle_string_changed(new_params, start_tick, end_tick, ctx);
        return;
    }
    if (active_changed) {
        handle_active_changed(new_params, start_tick, end_tick, ctx);
        return;
    }
}

// static
void EditCapo::update(Note *note, const UpdateCtx &ctx) {
    if (muse::contains(ctx.params.ignored_strings, (string_idx_t) note->string())) {
        note->set_pitch(ctx.string_data->get_pitch(note->string(), note->fret(), ctx.staff));
    } else {
        const int pitch = note->pitch() + ctx.note_offset;
        note->set_pitch(pitch);
    }
    note->set_tpc_from_pitch();
}

// static
void EditCapo::update_string(Note *note, const UpdateCtx &ctx) {
    const int pitch = note->pitch();
    const int string = note->string();
    const int fret = note->fret();
    if (CapoParams::TransposeMode::PLAYBACK_ONLY != ctx.params.transpose_mode) {
        if (muse::contains(ctx.params.ignored_strings, (string_idx_t) note->string())) {
            // Already handled
            if (pitch == ctx.string_data->get_pitch(string, fret, 0)) {
                return;
            }
            if (CapoParams::TransposeMode::STANDARD_ONLY == ctx.params.transpose_mode) {
                note->set_pitch(ctx.string_data->get_pitch(note->string(), note->fret(), ctx.staff));
            }
            note->set_tpc_from_pitch();
            return;
        }
        if (pitch != ctx.string_data->get_pitch(string, fret, -ctx.params.fret_position)) {
            if (CapoParams::TransposeMode::STANDARD_ONLY == ctx.params.transpose_mode) {
                note->set_pitch(ctx.string_data->get_pitch(note->string(), note->fret(), -ctx.params.fret_position));
            }
        }
    }
    note->set_tpc_from_pitch();
}

// static
void EditCapo::handle_mode_change(const CapoParams &old_params, const CapoParams &new_params, int start_tick, int end_tick,
                                  UpdateCtx &ctx) {
    switch (old_params.transpose_mode) {
        case CapoParams::TransposeMode::PLAYBACK_ONLY:
            if (new_params.transpose_mode == CapoParams::TransposeMode::STANDARD_ONLY) {
                ctx.note_offset = new_params.fret_position;
            }
            break;
        case CapoParams::TransposeMode::STANDARD_ONLY:
            if (new_params.transpose_mode == CapoParams::TransposeMode::PLAYBACK_ONLY) {
                ctx.note_offset = -old_params.fret_position;
            } else if (new_params.transpose_mode == CapoParams::TransposeMode::TAB_ONLY) {
                ctx.note_offset = -old_params.fret_position;
            }
            break;
        case CapoParams::TransposeMode::TAB_ONLY:
            if (new_params.transpose_mode == CapoParams::TransposeMode::STANDARD_ONLY) {
                ctx.note_offset = new_params.fret_position;
            }
            break;
    }
    apply_capo_transpose(start_tick, end_tick, ctx);
}

// static
void EditCapo::handle_fret_change(const CapoParams &old_params, const CapoParams &new_params, int start_tick, int end_tick,
                                  UpdateCtx &ctx) {
    if (CapoParams::TransposeMode::STANDARD_ONLY == new_params.transpose_mode) {
        ctx.note_offset = new_params.fret_position - old_params.fret_position;
        apply_capo_transpose(start_tick, end_tick, ctx);
    }
}

// static
void EditCapo::handle_string_changed(const CapoParams &new_params, int start_tick, int end_tick, UpdateCtx &ctx) {
    ctx.update_ignored_strings = true;

    switch (new_params.transpose_mode) {
        case CapoParams::TransposeMode::STANDARD_ONLY:
            ctx.note_offset = new_params.fret_position;
            apply_capo_transpose(start_tick, end_tick, ctx);
            break;
        case CapoParams::TransposeMode::TAB_ONLY:
        case CapoParams::TransposeMode::PLAYBACK_ONLY: break;
    }
}

// static
void EditCapo::handle_active_changed(const CapoParams &new_params, int start_tick, int end_tick, UpdateCtx &ctx) {
    if (!new_params.active) {
        // Treat as whatever mode to playback-only transition
        switch (new_params.transpose_mode) {
            case CapoParams::TransposeMode::STANDARD_ONLY: ctx.note_offset = -new_params.fret_position; break;
            case CapoParams::TransposeMode::TAB_ONLY:
            case CapoParams::TransposeMode::PLAYBACK_ONLY: break;
        }
    } else {
        // Otherwise, as playback-only mode to previous one
        switch (new_params.transpose_mode) {
            case CapoParams::TransposeMode::STANDARD_ONLY: ctx.note_offset = new_params.fret_position; break;
            case CapoParams::TransposeMode::TAB_ONLY:
            case CapoParams::TransposeMode::PLAYBACK_ONLY: break;
        }
    }
    apply_capo_transpose(start_tick, end_tick, ctx);
}
} // namespace mu::engraving
