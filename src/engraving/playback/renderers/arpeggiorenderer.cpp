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

#include "arpeggiorenderer.h"

#include "dom/chord.h"
#include "dom/arpeggio.h"

#include "noterenderer.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

static bool is_direction_up(const mpe::ArticulationType type) {
    switch (type) {
        case mpe::ArticulationType::ARPEGGIO_DOWN:
        case mpe::ArticulationType::ARPEGGIO_STRAIGHT_DOWN: return false;
        case mpe::ArticulationType::ARPEGGIO:
        case mpe::ArticulationType::ARPEGGIO_UP:
        case mpe::ArticulationType::ARPEGGIO_STRAIGHT_UP: return true;
        default: return false;
    }
}

static usecs_t timestamp_offset_step(const RenderingContext &ctx, int step_count) {
    constexpr int max_timestamp_offset_step = 60000;
    const int offset_step = ctx.nominal_duration / step_count;

    return std::min(offset_step, max_timestamp_offset_step);
}

static std::map<int /*pitch*/, const Note *> arpeggio_notes(const Chord *chord) {
    std::map<int, const Note *> result;
    for (const Note *note : chord->notes()) {
        result.emplace(note->pitch(), note);
    }

    return result;
}

const ArticulationTypeSet &ArpeggioRenderer::supported_types() {
    static const mpe::ArticulationTypeSet types = {mpe::ArticulationType::ARPEGGIO, mpe::ArticulationType::ARPEGGIO_DOWN,
                                                   mpe::ArticulationType::ARPEGGIO_STRAIGHT_DOWN,
                                                   mpe::ArticulationType::ARPEGGIO_STRAIGHT_UP, mpe::ArticulationType::ARPEGGIO_UP};

    return types;
}

void ArpeggioRenderer::do_render(const EngravingItem *item, const mpe::ArticulationType preferred_type, const RenderingContext &ctx,
                                 mpe::PlaybackEventList &result) {
    const Chord *chord = to_chord(item);
    IF_ASSERT_FAILED(chord) { return; }

    const Arpeggio *arpeggio = chord->arpeggio();
    IF_ASSERT_FAILED(arpeggio) { return; }

    const int steps_count = static_cast<int>(chord->notes().size());
    const usecs_t offset_step = timestamp_offset_step(ctx, steps_count);
    const double stretch = arpeggio->stretch();

    auto render_event = [&](const Note *note, const int step_number) {
        const timestamp_t offset = offset_step * step_number * stretch;

        RenderingContext note_ctx(ctx);
        note_ctx.nominal_timestamp += offset;
        note_ctx.nominal_duration -= offset;

        NoteRenderer::render(note, note_ctx, result);
    };

    const std::map<int, const Note *> notes = arpeggio_notes(chord);

    if (is_direction_up(preferred_type)) {
        for (auto it = notes.begin(); it != notes.end(); ++it) {
            render_event(it->second, std::distance(notes.begin(), it));
        }
    } else {
        for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
            render_event(it->second, std::distance(notes.rbegin(), it));
        }
    }
}
