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

#include <engraving/types/types.h>

namespace mu::engraving {
struct Note;
struct Chord;
struct Score;
} // namespace mu::engraving

namespace mu::iex::guitarpro::utils {
int harmonic_overtone(mu::engraving::Note *note, float harmonic_value, int harmonic_type);
std::vector<int> standard_tuning_for(int midi_program, int strings_count);
bool is_standard_tuning(int midi_program, const std::vector<int> &tuning);
mu::engraving::Chord *get_located_chord(mu::engraving::Score *score, mu::engraving::Fraction tick_fr, mu::engraving::track_idx_t track);
void create_ghost_note_paren_groups(mu::engraving::Chord *ch);
void add_play_count_texts(mu::engraving::Score *score);
} // namespace mu::iex::guitarpro::utils
