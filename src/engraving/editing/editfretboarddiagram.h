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

#include "../dom/fret.h"
#include "engraving/editing/cmd.h"

namespace mu::engraving {
void apply_fret_data_change(FretDiagram *d, const String &harmony_name);

void apply_fret_dot(FretDiagram *d, int input_string, int input_fret, bool input_add = false,
                    FretDotType input_dtype = FretDotType::NORMAL);

void apply_fret_marker(FretDiagram *d, int input_string, FretMarkerType input_mtype);

void apply_fret_barre(FretDiagram *d, int input_string, int input_fret, bool input_add = false);

void apply_fret_clear(FretDiagram *d);

} // namespace mu::engraving
