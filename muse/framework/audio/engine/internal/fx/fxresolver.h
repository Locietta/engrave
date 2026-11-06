/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
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

#ifndef MUSE_AUDIO_FXRESOLVER_H
#define MUSE_AUDIO_FXRESOLVER_H

#include <map>
#include <mutex>

#include "../../ifxresolver.h"

namespace muse::audio::fx {
class FxResolver : public IFxResolver {
public:
    std::vector<IFxProcessorPtr> resolve_master_fx_list(const AudioFxChain &fx_chain, const OutputSpec &output_spec) override;
    std::vector<IFxProcessorPtr> resolve_fx_list(const TrackId track_id, const AudioFxChain &fx_chain,
                                                 const OutputSpec &output_spec) override;
    AudioResourceMetaList resolve_available_resources() const override;
    void register_resolver(const AudioFxType type, IResolverPtr resolver) override;
    void clear_all_fx() override;

private:
    std::map<AudioFxType, IResolverPtr> resolvers_;
    mutable std::mutex mutex_;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_FXRESOLVER_H
