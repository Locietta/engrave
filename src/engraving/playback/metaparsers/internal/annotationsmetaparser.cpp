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

#include "annotationsmetaparser.h"

#include "dom/dynamic.h"

using namespace mu::engraving;
using namespace muse;
using namespace muse::mpe;

void AnnotationsMetaParser::do_parse(const EngravingItem *item, const RenderingContext &ctx, mpe::ArticulationMap &result) {
    IF_ASSERT_FAILED(item) { return; }

    mpe::ArticulationType type = mpe::ArticulationType::UNDEFINED;

    if (item->is_dynamic()) {
        const Dynamic *dynamic = to_dynamic(item);
        const DynamicType dynamic_type = dynamic->dynamic_type();

        if (dynamic_type == DynamicType::S || dynamic_type == DynamicType::SF || dynamic_type == DynamicType::SFF ||
            dynamic_type == DynamicType::SFFF || dynamic_type == DynamicType::SFZ || dynamic_type == DynamicType::SFFZ ||
            dynamic_type == DynamicType::SFFFZ || dynamic_type == DynamicType::SFP || dynamic_type == DynamicType::SFPP) {
            type = mpe::ArticulationType::SUBITO;
        }
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
