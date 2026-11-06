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

#ifndef MU_ENGRAVING_ORNAMENTSRENDERER_H
#define MU_ENGRAVING_ORNAMENTSRENDERER_H

#include "renderbase.h"

namespace mu::engraving {
struct DisclosurePattern;

struct OrnamentsRenderer : RenderBase<OrnamentsRenderer> {
    static const muse::mpe::ArticulationTypeSet &supported_types();

    static void do_render(const EngravingItem *item, const muse::mpe::ArticulationType preferred_type, const RenderingContext &ctx,
                          muse::mpe::PlaybackEventList &result);

private:
    static void apply_tied_notes_duration(const Note *note, const muse::mpe::ArticulationType ornament_type, RenderingContext &ctx);

    static void convert(const muse::mpe::ArticulationType type, const DisclosurePattern &pattern, NominalNoteCtx &&note_ctx,
                        muse::mpe::PlaybackEventList &result);

    static int alterations_number_by_tempo(const double beats_per_seconds, const int principal_note_duration_ticks,
                                           const DisclosurePattern &pattern);

    static void create_events(const muse::mpe::ArticulationType type, NominalNoteCtx &note_ctx, const int alterations_count,
                              const int available_duration_ticks, const int overall_duration_ticks,
                              const std::vector<muse::mpe::pitch_level_t> &pitch_offsets, muse::mpe::PlaybackEventList &result);
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_ORNAMENTSRENDERER_H
