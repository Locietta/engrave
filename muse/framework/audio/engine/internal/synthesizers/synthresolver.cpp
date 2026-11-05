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

#include "synthresolver.h"

#include "log.h"

using namespace muse::audio;
using namespace muse::audio::synth;

void SynthResolver::init(const AudioInputParams &defaultInputParams, const OutputSpec &defaultOutputSpec) {

    IF_ASSERT_FAILED(defaultInputParams.isValid() && defaultOutputSpec.isValid()) { return; }

    defaultInputParams_ = defaultInputParams;
    defaultOutputSpec_ = defaultOutputSpec;
}

ISynthesizerPtr SynthResolver::resolveSynth(const TrackId trackId, const AudioInputParams &params, const audio::OutputSpec &spec,
                                            const PlaybackSetupData &setupData) const {

    TRACEFUNC;

    if (!params.isValid()) {
        LOGE() << "invalid audio source params for trackId: " << trackId;
        return nullptr;
    }

    std::lock_guard lock(mutex_);

    auto search = resolvers_.find(params.type());

    if (search == resolvers_.end()) {
        return nullptr;
    }

    const IResolverPtr &resolver = search->second;

    if (!resolver->hasCompatibleResources(setupData)) {
        return nullptr;
    }

    return resolver->resolveSynth(trackId, params, spec);
}

ISynthesizerPtr SynthResolver::resolveDefaultSynth(const TrackId trackId) const {

    return resolveSynth(trackId, defaultInputParams_, defaultOutputSpec_, {});
}

AudioInputParams SynthResolver::resolveDefaultInputParams() const { return defaultInputParams_; }

AudioResourceMetaList SynthResolver::resolveAvailableResources() const {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    AudioResourceMetaList result;

    for (const auto &pair : resolvers_) {
        const AudioResourceMetaList &resolvedResources = pair.second->resolveResources();
        result.insert(result.end(), resolvedResources.begin(), resolvedResources.end());
    }

    return result;
}

SoundPresetList SynthResolver::resolveAvailableSoundPresets(const AudioResourceMeta &resourceMeta) const {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    auto search = resolvers_.find(audio::sourceTypeFromResourceType(resourceMeta.type));
    if (search == resolvers_.end()) {
        return SoundPresetList();
    }

    return search->second->resolveSoundPresets(resourceMeta);
}

void SynthResolver::registerResolver(const AudioSourceType type, IResolverPtr resolver) {

    std::lock_guard lock(mutex_);

    resolvers_.insert_or_assign(type, std::move(resolver));
}

void SynthResolver::clearSources() {

    std::lock_guard lock(mutex_);

    for (auto it = resolvers_.begin(); it != resolvers_.end(); ++it) {
        it->second->clearSources();
    }
}
