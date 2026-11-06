/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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
#include "guitarbendimporter.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
GuitarBendImporter::GuitarBendImporter(Score *score) : bend_collector_(score), score_(score) {}

void GuitarBendImporter::collect_bend(Note *note, const mu::engraving::PitchValues &pitch_values) {
    bend_collector_.store_bend_data(note, pitch_values);
}

void GuitarBendImporter::collect_dive(const mu::engraving::Chord *chord, const mu::engraving::PitchValues &pitch_values) {
    dive_collector_.collect_dive_data(chord, pitch_values);
}

void GuitarBendImporter::add_elements_to_score() {
    bend_builder_.add_elements_to_score(score_, bend_collector_.collect_bend_data_context(), dive_collector_.context());
}
} // namespace mu::iex::guitarpro
