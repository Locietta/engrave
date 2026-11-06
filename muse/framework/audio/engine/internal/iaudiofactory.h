/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "global/types/retval.h"
#include "mpe/events.h"
#include "../isynthesizer.h"
#include "nodes/audiosourcenode.h"
#include "nodes/fxchain.h"

namespace muse::audio::engine {
class IAudioFactory {
public:
    virtual ~IAudioFactory() = default;

    // Available resources
    virtual AudioResourceMetaList available_input_resources() const = 0;
    virtual SoundPresetList available_sound_presets(const AudioResourceMeta &resource_meta) const = 0;
    virtual AudioResourceMetaList available_output_resources() const = 0;

    // Make audio sources
    virtual RetVal<synth::ISynthesizerPtr> make_synth(const TrackId track_id, const AudioInputParams &params,
                                                      const PlaybackSetupData &setup_data) const = 0;

    virtual RetVal<synth::ISynthesizerPtr> make_default_synth(const TrackId track_id) const = 0;

    //! NOTE For internal purposes,
    // created synth instances are registered in an internal registry (see VST).
    // This method clears this registry.
    virtual void clear_synth_sources() = 0;

    virtual RetVal<AudioSourceNodePtr> make_event_source(const TrackId track_id, const mpe::PlaybackData &playback_data,
                                                         const AudioInputParams &params) const = 0;
    // Make FX
    virtual FxChainPtr make_master_fx_chain(const AudioFxChain &fx_chain) const = 0;
    virtual FxChainPtr make_track_fx_chain(const TrackId track_id, const AudioFxChain &fx_chain) const = 0;

    //! NOTE For internal purposes,
    // created effect instances are registered in an internal registry (see VST).
    // This method clears this registry.
    virtual void clear_all_fx() = 0;
};
} // namespace muse::audio::engine
