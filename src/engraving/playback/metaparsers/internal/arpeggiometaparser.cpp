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

#include "arpeggiometaparser.h"

#include "dom/arpeggio.h"

using namespace mu::engraving;
using namespace muse;

void ArpeggioMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item->is_arpeggio()) { return; }

    const Arpeggio *arpeggio = to_arpeggio(item);

    if (!arpeggio->play_arpeggio()) {
        return;
    }

    mpe::ArticulationType type = mpe::ArticulationType::UNDEFINED;

    switch (arpeggio->arpeggio_type()) {
        case ArpeggioType::NORMAL: type = mpe::ArticulationType::ARPEGGIO; break;
        case ArpeggioType::UP: type = mpe::ArticulationType::ARPEGGIO_UP; break;
        case ArpeggioType::DOWN: type = mpe::ArticulationType::ARPEGGIO_DOWN; break;
        case ArpeggioType::DOWN_STRAIGHT: type = mpe::ArticulationType::ARPEGGIO_STRAIGHT_DOWN; break;
        case ArpeggioType::UP_STRAIGHT: type = mpe::ArticulationType::ARPEGGIO_STRAIGHT_UP; break;
        default: break;
    }

    if (type == mpe::ArticulationType::UNDEFINED) {
        return;
    }

    const mpe::ArticulationPattern &pattern = ctx.profile->pattern(type);
    if (pattern.empty()) {
        return;
    }

    mpe::ArticulationMeta articulation_meta;
    articulation_meta.type = type;
    articulation_meta.pattern = pattern;
    articulation_meta.timestamp = ctx.nominal_timestamp;
    articulation_meta.overall_duration = ctx.nominal_duration * arpeggio->stretch();

    append_articulation_data(std::move(articulation_meta), result);
}
