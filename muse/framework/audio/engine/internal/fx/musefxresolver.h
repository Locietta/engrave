/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_MUSEFXRESOLVER_H
#define MUSE_AUDIO_MUSEFXRESOLVER_H

#include "abstractfxresolver.h"

namespace muse::audio::fx {
struct MuseFxResolver : AbstractFxResolver {
    AudioResourceMetaList resolve_resources() const override;

private:
    IFxProcessorPtr create_master_fx(const AudioFxParams &fx_params, const OutputSpec &output_spec) const override;
    IFxProcessorPtr create_track_fx(const TrackId track_id, const AudioFxParams &fx_params, const OutputSpec &output_spec) const override;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_MUSEFXRESOLVER_H
