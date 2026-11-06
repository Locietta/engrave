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

#pragma once

#include <memory>

#include "audio/common/audiotypes.h"

#include "isynthesizer.h"

namespace muse::audio::synth {
struct ISynthResolver {
    virtual ~ISynthResolver() = default;

    struct IResolver {
        virtual ~IResolver() = default;

        virtual ISynthesizerPtr resolve_synth(const TrackId track_id, const AudioInputParams &params,
                                              const OutputSpec &output_spec) const = 0;
        virtual bool has_compatible_resources(const PlaybackSetupData &setup) const = 0;
        virtual AudioResourceMetaList resolve_resources() const = 0;
        virtual SoundPresetList resolve_sound_presets(const AudioResourceMeta &resource_meta) const = 0;
        virtual void refresh() = 0;
        virtual void clear_sources() = 0;
    };

    using IResolverPtr = std::shared_ptr<IResolver>;

    virtual void init(const AudioInputParams &default_input_params, const audio::OutputSpec &default_output_spec) = 0;

    virtual ISynthesizerPtr resolve_synth(const TrackId track_id, const AudioInputParams &params, const OutputSpec &spec,
                                          const PlaybackSetupData &setup_data) const = 0;
    virtual ISynthesizerPtr resolve_default_synth(const TrackId track_id) const = 0;
    virtual AudioInputParams resolve_default_input_params() const = 0;
    virtual AudioResourceMetaList resolve_available_resources() const = 0;
    virtual SoundPresetList resolve_available_sound_presets(const AudioResourceMeta &resource_meta) const = 0;
    virtual void register_resolver(const AudioSourceType type, IResolverPtr resolver) = 0;
    virtual void clear_sources() = 0;
};

using ISynthResolverPtr = std::shared_ptr<ISynthResolver>;
} // namespace muse::audio::synth
