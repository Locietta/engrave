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

#pragma once

#include "mpe/events.h"

namespace mu::engraving {
struct Note;
struct NominalNoteCtx;

struct GlissandosRenderer {
    static muse::mpe::duration_t discrete_glissando_step_duration(const Note *note, const muse::mpe::duration_t note_duration);

    static void render_discrete_glissando(const Note *note, NominalNoteCtx &ctx, muse::mpe::PlaybackEventList &result);
    static void render_continuous_glissando(const Note *note, NominalNoteCtx &ctx, muse::mpe::PlaybackEventList &result);
};
} // namespace mu::engraving
