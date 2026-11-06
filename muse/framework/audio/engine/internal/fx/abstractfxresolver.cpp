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

#include "abstractfxresolver.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::fx;

std::vector<IFxProcessorPtr> AbstractFxResolver::resolve_fx_list(const TrackId track_id, const AudioFxChain &fx_chain,
                                                                 const OutputSpec &output_spec) {
    if (fx_chain.empty()) {
        LOGE() << "invalid fx chain for trackId: " << track_id;
        return {};
    }

    FxMap &fx_map = tracks_fx_map_[track_id];
    update_track_fx_map(fx_map, track_id, fx_chain, output_spec);

    return muse::values(fx_map);
}

std::vector<IFxProcessorPtr> AbstractFxResolver::resolve_master_fx_list(const AudioFxChain &fx_chain, const OutputSpec &output_spec) {
    if (fx_chain.empty()) {
        LOGE() << "invalid master fx params";
        return {};
    }

    update_master_fx_map(fx_chain, output_spec);

    return muse::values(master_fx_map_);
}

void AbstractFxResolver::refresh() {}

void AbstractFxResolver::clear_all_fx() {
    tracks_fx_map_.clear();
    master_fx_map_.clear();
}

void AbstractFxResolver::update_track_fx_map(FxMap &fx_map, const audio::TrackId track_id, const AudioFxChain &new_fx_chain,
                                             const OutputSpec &output_spec) {
    AudioFxChain current_fx_chain;
    for (const auto &pair : fx_map) {
        current_fx_chain.emplace(pair.first, pair.second->params());
    }

    audio::AudioFxChain fx_to_remove;
    fx_chain_to_remove(current_fx_chain, new_fx_chain, fx_to_remove);

    for (const auto &pair : fx_to_remove) {
        remove_track_fx(track_id, pair.second.resource_meta.id, pair.second.chain_order);

        current_fx_chain.erase(pair.first);
        fx_map.erase(pair.first);
    }

    audio::AudioFxChain fx_to_create;
    fx_chain_to_create(current_fx_chain, new_fx_chain, fx_to_create);

    for (const auto &pair : fx_to_create) {
        IFxProcessorPtr fx_ptr = create_track_fx(track_id, pair.second, output_spec);
        if (fx_ptr) {
            fx_map.emplace(pair.first, std::move(fx_ptr));
        }
    }
}

void AbstractFxResolver::update_master_fx_map(const AudioFxChain &new_fx_chain, const OutputSpec &output_spec) {
    AudioFxChain current_fx_chain;
    for (const auto &pair : master_fx_map_) {
        current_fx_chain.emplace(pair.first, pair.second->params());
    }

    audio::AudioFxChain fx_to_remove;
    fx_chain_to_remove(current_fx_chain, new_fx_chain, fx_to_remove);

    for (const auto &pair : fx_to_remove) {
        remove_master_fx(pair.second.resource_meta.id, pair.second.chain_order);

        current_fx_chain.erase(pair.first);
        master_fx_map_.erase(pair.first);
    }

    audio::AudioFxChain fx_to_create;
    fx_chain_to_create(current_fx_chain, new_fx_chain, fx_to_create);

    for (const auto &pair : fx_to_create) {
        IFxProcessorPtr fx = create_master_fx(pair.second, output_spec);
        if (fx) {
            master_fx_map_.emplace(pair.first, fx);
        }
    }
}

void AbstractFxResolver::remove_master_fx(const AudioResourceId &, AudioFxChainOrder) {}

void AbstractFxResolver::remove_track_fx(const audio::TrackId, const AudioResourceId &, AudioFxChainOrder) {}

void AbstractFxResolver::fx_chain_to_remove(const AudioFxChain &current_fx_chain, const AudioFxChain &new_fx_chain,
                                            AudioFxChain &result_chain) {
    for (auto it = current_fx_chain.cbegin(); it != current_fx_chain.cend(); ++it) {
        auto new_it = new_fx_chain.find(it->first);

        if (new_it == new_fx_chain.cend()) {
            result_chain.insert({it->first, it->second});
            continue;
        }

        if (it->second.resource_meta != new_it->second.resource_meta) {
            result_chain.insert({it->first, it->second});
        }
    }
}

void AbstractFxResolver::fx_chain_to_create(const AudioFxChain &current_fx_chain, const AudioFxChain &new_fx_chain,
                                            AudioFxChain &result_chain) {
    for (auto it = new_fx_chain.cbegin(); it != new_fx_chain.cend(); ++it) {
        if (current_fx_chain.find(it->first) != current_fx_chain.cend()) {
            continue;
        }

        if (it->second.is_valid()) {
            result_chain.insert({it->first, it->second});
        }
    }
}
