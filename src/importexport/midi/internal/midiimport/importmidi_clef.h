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
#ifndef IMPORTMIDI_CLEF_H
#define IMPORTMIDI_CLEF_H

#include "engraving/types/types.h"

namespace mu::engraving {
struct Staff;
struct InstrumentTemplate;
} // namespace mu::engraving

namespace mu::iex::midi { namespace MidiClef {
bool has_g_fclefs(const engraving::InstrumentTemplate *templ);
void create_clefs(engraving::Staff *staff, int index_of_operation, bool is_drum_track);
engraving::ClefType clef_type_from_average_pitch(int average_pitch);
}} // namespace mu::iex::midi::MidiClef

#endif // IMPORTMIDI_CLEF_H
