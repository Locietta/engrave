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

#include "chordlinemetaparser.h"

#include "dom/chordline.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

void ChordLineMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item) { return; }

    if (!item->is_chord_line()) {
        return;
    }

    const ChordLine *chord_line = to_chord_line(item);

    ArticulationType type = chord_line_articulation_type(chord_line->chord_line_type());

    if (type == ArticulationType::UNDEFINED) {
        return;
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    append_articulation_data(mpe::ArticulationMeta(type, pattern, ctx.nominal_timestamp, ctx.nominal_duration), result);
}

ArticulationType ChordLineMetaParser::chord_line_articulation_type(const ChordLineType chord_line_type) {
    static const std::unordered_map<ChordLineType, ArticulationType> articulation_types = {{ChordLineType::DOIT, ArticulationType::DOIT},
                                                                                           {ChordLineType::FALL, ArticulationType::FALL},
                                                                                           {ChordLineType::PLOP, ArticulationType::PLOP},
                                                                                           {ChordLineType::SCOOP, ArticulationType::SCOOP}};

    auto search = articulation_types.find(chord_line_type);

    if (search != articulation_types.cend()) {
        return search->second;
    }

    return ArticulationType::UNDEFINED;
}
