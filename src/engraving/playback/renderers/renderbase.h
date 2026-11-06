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

#ifndef MU_ENGRAVING_RENDERBASE_H
#define MU_ENGRAVING_RENDERBASE_H

#include "global/asserts.h"

#include "playback/renderingcontext.h"

namespace mu::engraving {
template <class T>
struct RenderBase {
    static bool is_able_to_render(const muse::mpe::ArticulationType &type) {
        const muse::mpe::ArticulationTypeSet &supported_types = T::supported_types();

        return supported_types.find(type) != supported_types.cend();
    }

    static void render(const EngravingItem *item, const muse::mpe::ArticulationType preferred_type, const RenderingContext &context,
                       muse::mpe::PlaybackEventList &result) {
        IF_ASSERT_FAILED(item) { return; }

        T::do_render(item, preferred_type, context, result);
    }
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_RENDERBASE_H
