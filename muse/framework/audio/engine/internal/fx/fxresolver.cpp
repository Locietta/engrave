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

std::vector<IFxProcessorPtr> FxResolver::resolveMasterFxList(const AudioFxChain &fxChain, const OutputSpec &outputSpec) {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    std::vector<IFxProcessorPtr> result;

    for (const auto &resolver : resolvers_) {
        AudioFxChain fxChainByType;

        for (const auto &fx : fxChain) {
            if (resolver.first == fx.second.type() || fx.second.type() == AudioFxType::Undefined) {
                fxChainByType.insert(fx);
            }
        }

        if (fxChainByType.empty()) {
            continue;
        }

        std::vector<IFxProcessorPtr> fxList = resolver.second->resolveMasterFxList(std::move(fxChainByType), outputSpec);
        result.insert(result.end(), fxList.begin(), fxList.end());
    }

    return result;
}

std::vector<IFxProcessorPtr> FxResolver::resolveFxList(const TrackId trackId, const AudioFxChain &fxChain, const OutputSpec &outputSpec) {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    std::vector<IFxProcessorPtr> result;

    for (const auto &resolver : resolvers_) {
        AudioFxChain fxChainByType;

        for (const auto &fx : fxChain) {
            if (resolver.first == fx.second.type() || fx.second.type() == AudioFxType::Undefined) {
                fxChainByType.insert(fx);
            }
        }

        if (fxChainByType.empty()) {
            continue;
        }

        std::vector<IFxProcessorPtr> fxList = resolver.second->resolveFxList(trackId, std::move(fxChainByType), outputSpec);
        result.insert(result.end(), fxList.begin(), fxList.end());
    }

    return result;
}

AudioResourceMetaList FxResolver::resolveAvailableResources() const {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    AudioResourceMetaList result;

    for (const auto &pair : resolvers_) {
        const AudioResourceMetaList &resolvedResources = pair.second->resolveResources();
        result.insert(result.end(), resolvedResources.begin(), resolvedResources.end());
    }

    return result;
}

void FxResolver::registerResolver(const AudioFxType type, IResolverPtr resolver) {

    std::lock_guard lock(mutex_);

    resolvers_.insert_or_assign(type, std::move(resolver));
}

void FxResolver::clearAllFx() {

    std::lock_guard lock(mutex_);

    for (auto it = resolvers_.begin(); it != resolvers_.end(); ++it) {
        it->second->clearAllFx();
    }
}
