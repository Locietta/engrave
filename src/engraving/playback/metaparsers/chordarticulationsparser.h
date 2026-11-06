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

#ifndef CHORDARTICULATIONSPARSER_H
#define CHORDARTICULATIONSPARSER_H

#include "metaparserbase.h"

namespace mu::engraving {
struct Chord;

struct ChordArticulationsParser : MetaParserBase<ChordArticulationsParser> {
    static void build_chord_articulation_map(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);

protected:
    friend MetaParserBase;

    static void do_parse(const EngravingItem *item, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);

private:
    static void parse_spanners(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_articulation_symbols(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_annotations(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_tremolo(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_arpeggio(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_grace_notes(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_chord_line(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_tapping(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
    static void parse_breath(const Chord *chord, const RenderingContext &ctx, muse::mpe::ArticulationMap &result);
};
} // namespace mu::engraving

#endif // CHORDARTICULATIONSPARSER_H
