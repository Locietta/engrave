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

#include "fxresolver.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::fx;

std::vector<IFxProcessorPtr> FxResolver::resolve_master_fx_list(const AudioFxChain &fx_chain, const OutputSpec &output_spec) {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    std::vector<IFxProcessorPtr> result;

    for (const auto &resolver : resolvers_) {
        AudioFxChain fx_chain_by_type;

        for (const auto &fx : fx_chain) {
            if (resolver.first == fx.second.type() || fx.second.type() == AudioFxType::UNDEFINED) {
                fx_chain_by_type.insert(fx);
            }
        }

        if (fx_chain_by_type.empty()) {
            continue;
        }

        std::vector<IFxProcessorPtr> fx_list = resolver.second->resolve_master_fx_list(std::move(fx_chain_by_type), output_spec);
        result.insert(result.end(), fx_list.begin(), fx_list.end());
    }

    return result;
}

std::vector<IFxProcessorPtr> FxResolver::resolve_fx_list(const TrackId track_id, const AudioFxChain &fx_chain,
                                                         const OutputSpec &output_spec) {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    std::vector<IFxProcessorPtr> result;

    for (const auto &resolver : resolvers_) {
        AudioFxChain fx_chain_by_type;

        for (const auto &fx : fx_chain) {
            if (resolver.first == fx.second.type() || fx.second.type() == AudioFxType::UNDEFINED) {
                fx_chain_by_type.insert(fx);
            }
        }

        if (fx_chain_by_type.empty()) {
            continue;
        }

        std::vector<IFxProcessorPtr> fx_list = resolver.second->resolve_fx_list(track_id, std::move(fx_chain_by_type), output_spec);
        result.insert(result.end(), fx_list.begin(), fx_list.end());
    }

    return result;
}

AudioResourceMetaList FxResolver::resolve_available_resources() const {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    AudioResourceMetaList result;

    for (const auto &pair : resolvers_) {
        const AudioResourceMetaList &resolved_resources = pair.second->resolve_resources();
        result.insert(result.end(), resolved_resources.begin(), resolved_resources.end());
    }

    return result;
}

void FxResolver::register_resolver(const AudioFxType type, IResolverPtr resolver) {

    std::lock_guard lock(mutex_);

    resolvers_.insert_or_assign(type, std::move(resolver));
}

void FxResolver::clear_all_fx() {

    std::lock_guard lock(mutex_);

    for (auto it = resolvers_.begin(); it != resolvers_.end(); ++it) {
        it->second->clear_all_fx();
    }
}
