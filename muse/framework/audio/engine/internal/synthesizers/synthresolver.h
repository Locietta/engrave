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

#ifndef MUSE_AUDIO_SYNTHRESOLVER_H
#define MUSE_AUDIO_SYNTHRESOLVER_H

#include <map>
#include <mutex>

#include "../../isynthresolver.h"

namespace muse::audio::synth {
class SynthResolver : public ISynthResolver {
public:
    void init(const AudioInputParams &default_input_params, const audio::OutputSpec &default_output_spec) override;

    ISynthesizerPtr resolve_synth(const TrackId track_id, const AudioInputParams &params, const OutputSpec &spec,
                                  const PlaybackSetupData &setup_data) const override;
    ISynthesizerPtr resolve_default_synth(const TrackId track_id) const override;
    AudioInputParams resolve_default_input_params() const override;
    AudioResourceMetaList resolve_available_resources() const override;
    SoundPresetList resolve_available_sound_presets(const AudioResourceMeta &resource_meta) const override;

    void register_resolver(const AudioSourceType type, IResolverPtr resolver) override;

    void clear_sources() override;

private:
    using SynthPair = std::pair<audio::AudioResourceId, ISynthesizerPtr>;

    mutable std::mutex mutex_;

    std::map<AudioSourceType, IResolverPtr> resolvers_;
    AudioInputParams default_input_params_;
    OutputSpec default_output_spec_;
};
} // namespace muse::audio::synth

#endif // MUSE_AUDIO_SYNTHRESOLVER_H
