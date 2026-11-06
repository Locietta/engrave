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

#include "gracenotesmetaparser.h"

#include "dom/chord.h"

using namespace mu::engraving;
using namespace muse;

void GraceNotesMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->is_chord()) { return; }

    mpe::ArticulationType type = mpe::ArticulationType::UNDEFINED;

    const Chord *grace_chord = to_chord(item);

    if (!grace_chord->is_chord_playable()) {
        return;
    }

    switch (grace_chord->note_type()) {
        case NoteType::ACCIACCATURA: type = mpe::ArticulationType::ACCIACCATURA; break;

        case NoteType::APPOGGIATURA:
        case NoteType::GRACE4:
        case NoteType::GRACE16:
        case NoteType::GRACE32: type = mpe::ArticulationType::PRE_APPOGGIATURA; break;

        case NoteType::GRACE8_AFTER:
        case NoteType::GRACE16_AFTER:
        case NoteType::GRACE32_AFTER: type = mpe::ArticulationType::POST_APPOGGIATURA; break;
        default: break;
    }

    if (type == mpe::ArticulationType::UNDEFINED) {
        return;
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    append_articulation_data(mpe::ArticulationMeta(type, pattern, ctx.nominal_timestamp, ctx.nominal_duration), result);
}
