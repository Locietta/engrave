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

#include "musefxresolver.h"

#include "reverb/reverbprocessor.h"

#include "audio/common/audioutils.h"

using namespace muse::audio;
using namespace muse::audio::fx;

namespace muse::audio::fx {
IFxProcessorPtr create_fx_processor(const AudioFxParams &fx_params, const OutputSpec &output_spec) {
    if (fx_params.resource_meta.id == ::muse::audio::muse_reverb_id) {
        std::shared_ptr<ReverbProcessor> p = std::make_shared<ReverbProcessor>(fx_params);
        p->init(output_spec);
        return p;
    }

    return nullptr;
}
} // namespace muse::audio::fx

AudioResourceMetaList MuseFxResolver::resolve_resources() const {
    AudioResourceMetaList result;
    result.emplace_back(make_reverb_meta());

    return result;
}

IFxProcessorPtr MuseFxResolver::create_master_fx(const AudioFxParams &fx_params, const OutputSpec &output_spec) const {
    return create_fx_processor(fx_params, output_spec);
}

IFxProcessorPtr MuseFxResolver::create_track_fx(const TrackId, const AudioFxParams &fx_params, const OutputSpec &output_spec) const {
    return create_fx_processor(fx_params, output_spec);
}
