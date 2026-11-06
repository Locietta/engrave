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

#ifndef MU_ENGRAVING_PLAYBACKSETUPDATARESOLVER_H
#define MU_ENGRAVING_PLAYBACKSETUPDATARESOLVER_H

#include "mpe/events.h"

namespace mu::engraving {
struct Instrument;

struct PlaybackSetupDataResolver {
    void resolve_setup_data(const Instrument *instrument, muse::mpe::PlaybackSetupData &result) const;
    void resolve_chord_symbols_setup_data(const Instrument *instrument, muse::mpe::PlaybackSetupData &result) const;
    void resolve_metronome_setup_data(muse::mpe::PlaybackSetupData &result) const;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_PLAYBACKSETUPDATARESOLVER_H
