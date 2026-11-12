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

#include "engraving/types/types.h"
#include "metaparserbase.h"

namespace mu::engraving {
struct Note;

struct NoteArticulationsParser : MetaParserBase<NoteArticulationsParser> {
    static void build_note_articulation_map(const Note *note, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);

    static void parse_playing_technique(const RenderingContext &ctx, const track_idx_t track_idx, muse::mpe::ArticulationMap &result,
                                        bool sustain_allowed = true);
    static void parse_ghost_note(const Note *note, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_note_head(const Note *note, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_symbols(const Note *note, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_laissez_vibrer(const Note *note, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_spanners(const Note *note, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);

protected:
    friend MetaParserBase;

    static void do_parse(const EngravingItem *item, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);

private:
    static muse::mpe::ArticulationType articulation_type_by_notehead_group(const NoteHeadGroup notehead_group);
    static void append_articulations(const muse::mpe::ArticulationTypeSet &types, const RenderingContext &ctx,
                                     muse::mpe::ArticulationMap &result);
};
} // namespace mu::engraving
