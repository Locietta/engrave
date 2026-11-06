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

#pragma once

#include "renderbase.h"

namespace mu::engraving {
struct ArpeggioRenderer : RenderBase<ArpeggioRenderer> {
    static const muse::mpe::ArticulationTypeSet &supported_types();

    static void do_render(const EngravingItem *item, const muse::mpe::ArticulationType preferred_type, const RenderingContext &ctx,
                          muse::mpe::PlaybackEventList &result);
};
} // namespace mu::engraving
