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

#ifndef MU_ENGRAVING_TREMOLORENDERER_H
#define MU_ENGRAVING_TREMOLORENDERER_H

#include "renderbase.h"

namespace mu::engraving {
class TremoloRenderer : public RenderBase<TremoloRenderer> {
public:
    static const muse::mpe::ArticulationTypeSet &supported_types();

    static void do_render(const EngravingItem *item, const muse::mpe::ArticulationType preferred_type, const RenderingContext &ctx,
                          muse::mpe::PlaybackEventList &result);

private:
    using TremoloTimeCache = std::unordered_map<const Note *, muse::mpe::TimestampAndDuration>;

    static int step_duration_ticks(const Chord *chord, int tremolo_lines);
    static void build_and_append_events(const Chord *chord, const muse::mpe::ArticulationType type, const int step_duration_ticks,
                                        const int start_tick, const RenderingContext &ctx, TremoloTimeCache &tremolo_cache,
                                        muse::mpe::PlaybackEventList &result);
    static const muse::mpe::TimestampAndDuration &tremolo_time_and_duration(const Note *note, const RenderingContext &ctx,
                                                                            TremoloTimeCache &cache);
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_TREMOLORENDERER_H
