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

#include "../dom/segment.h"

namespace mu::engraving {
void validate_element_add(EngravingItem &item);
void prepare_element_removal(EngravingItem &item);
void apply_element_add(EngravingItem &item);
void revert_element_add(EngravingItem &item);
void apply_element_remove(EngravingItem &item);
void revert_element_remove(EngravingItem &item);
void apply_element_replace(EngravingItem &before, EngravingItem &after);
void apply_element_reparent(EngravingItem &item, EngravingItem &parent, track_idx_t track);
void apply_segment_reparent(Segment &segment, Measure &parent, Fraction tick);
} // namespace mu::engraving
