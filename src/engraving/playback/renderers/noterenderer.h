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

#pragma once

#include "playback/renderingcontext.h"

namespace mu::engraving {
struct Note;

struct NoteRenderer {
    static bool should_render(const Note *note, const RenderingContext &ctx, const muse::mpe::ArticulationMap &articulations);
    static void render(const Note *note, const RenderingContext &ctx, muse::mpe::PlaybackEventList &result);

private:
    static void render_partial_tie(const Note *outgoing_note, NominalNoteCtx &outgoing_note_ctx);
    static void render_normal_tie(const Note *first_note, NominalNoteCtx &first_note_ctx);
    static void add_tied_note(const NominalNoteCtx &tied_note_ctx, NominalNoteCtx &first_note_ctx);
    static void update_articulation_boundaries(const muse::mpe::timestamp_t note_timestamp, const muse::mpe::duration_t note_duration,
                                               muse::mpe::ArticulationMap &articulations);
    static void apply_swing_if_need(const Note *note, NominalNoteCtx &note_ctx);

    static NominalNoteCtx build_nominal_note_ctx(const Note *note, const RenderingContext &ctx);
};
} // namespace mu::engraving
