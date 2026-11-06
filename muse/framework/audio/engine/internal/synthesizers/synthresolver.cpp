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

void SynthResolver::init(const AudioInputParams &default_input_params, const OutputSpec &default_output_spec) {

    IF_ASSERT_FAILED(default_input_params.is_valid() && default_output_spec.is_valid()) { return; }

    default_input_params_ = default_input_params;
    default_output_spec_ = default_output_spec;
}

ISynthesizerPtr SynthResolver::resolve_synth(const TrackId track_id, const AudioInputParams &params, const audio::OutputSpec &spec,
                                             const PlaybackSetupData &setup_data) const {

    TRACEFUNC;

    if (!params.is_valid()) {
        LOGE() << "invalid audio source params for trackId: " << track_id;
        return nullptr;
    }

    std::lock_guard lock(mutex_);

    auto search = resolvers_.find(params.type());

    if (search == resolvers_.end()) {
        return nullptr;
    }

    const IResolverPtr &resolver = search->second;

    if (!resolver->has_compatible_resources(setup_data)) {
        return nullptr;
    }

    return resolver->resolve_synth(track_id, params, spec);
}

ISynthesizerPtr SynthResolver::resolve_default_synth(const TrackId track_id) const {

    return resolve_synth(track_id, default_input_params_, default_output_spec_, {});
}

AudioInputParams SynthResolver::resolve_default_input_params() const { return default_input_params_; }

AudioResourceMetaList SynthResolver::resolve_available_resources() const {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    AudioResourceMetaList result;

    for (const auto &pair : resolvers_) {
        const AudioResourceMetaList &resolved_resources = pair.second->resolve_resources();
        result.insert(result.end(), resolved_resources.begin(), resolved_resources.end());
    }

    return result;
}

SoundPresetList SynthResolver::resolve_available_sound_presets(const AudioResourceMeta &resource_meta) const {

    TRACEFUNC;

    std::lock_guard lock(mutex_);

    auto search = resolvers_.find(audio::source_type_from_resource_type(resource_meta.type));
    if (search == resolvers_.end()) {
        return SoundPresetList();
    }

    return search->second->resolve_sound_presets(resource_meta);
}

void SynthResolver::register_resolver(const AudioSourceType type, IResolverPtr resolver) {

    std::lock_guard lock(mutex_);

    resolvers_.insert_or_assign(type, std::move(resolver));
}

void SynthResolver::clear_sources() {

    std::lock_guard lock(mutex_);

    for (auto it = resolvers_.begin(); it != resolvers_.end(); ++it) {
        it->second->clear_sources();
    }
}
