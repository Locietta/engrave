/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#include "vorbis_decode.h"

#include "../../engine/internal/offlinecancellation.h"

extern "C" {
// Owner-thread cooperative hook. Never throws through the C sample loader.
int engrave_fluid_cancelled() { return muse::audio::OfflineCancellation::requested() ? 1 : 0; }

int vorbis_decode_memory(const unsigned char *mem, unsigned int len, short **output, unsigned int *channels, unsigned int *sample_rate) {
    const auto decoder = muse::audio::fluid::FluidVorbisDecoder::decoder;
    return decoder ? decoder(mem, len, output, channels, sample_rate) : -1;
}
}

namespace muse::audio::fluid {
FluidVorbisDecoder::DecodeMemoryFn FluidVorbisDecoder::decoder = nullptr;
} // namespace muse::audio::fluid
