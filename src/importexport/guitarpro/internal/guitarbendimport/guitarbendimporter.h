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
#pragma once

#include "bendbuilder.h"
#include "benddatacollector.h"
#include "divedatacollector.h"

namespace mu::engraving {
struct Note;
struct Chord;
struct Score;
} // namespace mu::engraving

namespace mu::iex::guitarpro {
struct GuitarBendImporter {

    GuitarBendImporter(mu::engraving::Score *score);

    void collect_bend(mu::engraving::Note *note, const mu::engraving::PitchValues &pitch_values);
    void collect_dive(const mu::engraving::Chord *chord, const mu::engraving::PitchValues &pitch_values);
    void add_elements_to_score();

private:

    BendDataCollector bend_collector_;
    DiveDataCollector dive_collector_;
    BendBuilder bend_builder_;
    mu::engraving::Score *score_ = nullptr;
};
} // namespace mu::iex::guitarpro
