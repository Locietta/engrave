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

#include "global/types/retval.h"
#include "nodes/trackchain.h"

namespace muse::audio {
struct ContextMixerTag {
    static constexpr const char *name = "ContextMixer";
};
} // namespace muse::audio

namespace muse::audio::engine {
struct Mixer : AudioNode<ContextMixerTag> {
    ~Mixer() override;

    void init();

    Ret add_track(TrackChainPtr track_chain, const AuxSendsParams &aux_sends);
    Ret add_aux_track(TrackChainPtr track_chain);
    Ret remove_track(const TrackId track_id);

    void set_aux_sends(const TrackId track_id, const AuxSendsParams &aux_sends);

    void set_tracks_to_process_when_idle(const std::unordered_set<TrackId> &track_ids);
    void set_non_muted_track_count(size_t count);

    void process(float *buffer, samples_t samples_per_channel) override;

    std::string dump() const override;

private:

    void on_output_spec_changed(const OutputSpec &spec) override;
    void on_mode_changed(const ProcessMode mode) override;

    void do_self_process(float *, samples_t) override {}

    void process_track_channels(size_t out_buffer_size, size_t samples_per_channel);
    void mix_output_from_channel(float *out_buffer, const float *in_buffer, size_t buffer_size) const;
    void prepare_aux_buffers(size_t out_buffer_size);
    void write_track_to_aux_buffers(const float *track_buffer, size_t out_buffer_size, const AuxSendsParams &aux_sends);
    void process_aux_channels(float *buffer, samples_t samples_per_channel);

    struct TrackData {
        TrackId track_id;
        TrackChainPtr chain;
        std::vector<float> buffer;
        bool processed = false;
    };

    std::vector<TrackData> tracks_;
    std::vector<TrackData> aux_tracks_;
    std::map<TrackId, AuxSendsParams> aux_sends_;

    size_t non_muted_track_count_ = 0;
    std::unordered_set<TrackId> tracks_to_process_when_idle_;
};

using MixerPtr = std::shared_ptr<Mixer>;
} // namespace muse::audio::engine
