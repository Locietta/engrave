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

#pragma once

#include <map>

#include "../../ifxresolver.h"
#include "../../ifxprocessor.h"

namespace muse::audio::fx {
class AbstractFxResolver : public IFxResolver::IResolver {
public:
    std::vector<IFxProcessorPtr> resolve_fx_list(const TrackId track_id, const AudioFxChain &fx_chain,
                                                 const OutputSpec &output_spec) override;
    std::vector<IFxProcessorPtr> resolve_master_fx_list(const AudioFxChain &fx_chain, const OutputSpec &output_spec) override;

    void refresh() override;
    void clear_all_fx() override;

protected:
    virtual IFxProcessorPtr create_master_fx(const AudioFxParams &fx_params, const OutputSpec &output_spec) const = 0;
    virtual IFxProcessorPtr create_track_fx(const TrackId track_id, const AudioFxParams &fx_params,
                                            const OutputSpec &output_spec) const = 0;

    virtual void remove_master_fx(const AudioResourceId &resoure_id, AudioFxChainOrder order);
    virtual void remove_track_fx(const TrackId track_id, const AudioResourceId &resoure_id, AudioFxChainOrder order);

private:
    using FxMap = std::map<AudioFxChainOrder, IFxProcessorPtr>;

    void update_master_fx_map(const AudioFxChain &new_fx_chain, const OutputSpec &output_spec);
    void update_track_fx_map(FxMap &fx_map, const TrackId track_id, const AudioFxChain &new_fx_chain, const audio::OutputSpec &output_spec);

    void fx_chain_to_remove(const AudioFxChain &current_fx_chain, const AudioFxChain &new_fx_chain, AudioFxChain &result_chain);
    void fx_chain_to_create(const AudioFxChain &current_fx_chain, const AudioFxChain &new_fx_chain, AudioFxChain &result_chain);

    std::map<TrackId, FxMap> tracks_fx_map_;
    FxMap master_fx_map_;
};
} // namespace muse::audio::fx
