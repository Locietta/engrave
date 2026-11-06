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
#pragma once

#include <vector>

#include "global/types/string.h"

#include "internal/midishared/generalmidi.h"

namespace mu::engraving {
class Score;
} // namespace mu::engraving

namespace mu::iex::midi {
class MTrack;

namespace MidiInstr {
muse::String instrument_name(GM1Program, bool is_drum_track);
muse::String ms_instr_name(int track_index);
muse::String concatenate_with_comma(const muse::String &left, const muse::String &right);
bool is_grand_staff(const MTrack &t1, const MTrack &t2);
void set_grand_staff_program(std::vector<MTrack> &tracks);
void find_instruments_for_all_tracks(const std::vector<MTrack> &tracks, bool force_reload = false);
void create_instruments(engraving::Score *score, std::vector<MTrack> &tracks);

extern void instrument_templates_changed();
} // namespace MidiInstr
} // namespace mu::iex::midi
