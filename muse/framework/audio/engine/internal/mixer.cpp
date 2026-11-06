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
#include "offlinecancellation.h"
#include "mixer.h"

#include <sstream>

#include "audio/common/audioerrors.h"

#include "nodes/signalnode.h"

#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::engine;

static bool is_chain_silent(const TrackChainPtr &chain) {
    if (auto signal = chain->signal()) {
        return signal->is_silent();
    }
    return false;
}

Mixer::~Mixer() {}

void Mixer::init() {}

Ret Mixer::add_track(TrackChainPtr track_chain, const AuxSendsParams &aux_sends) {
    const size_t out_buffer_size = output_spec_.samples_per_channel * output_spec_.audio_channel_count;

    TrackData track_data;
    track_data.track_id = track_chain->track_id();
    track_data.chain = track_chain;
    track_data.buffer.resize(out_buffer_size);

    tracks_.emplace_back(std::move(track_data));

    set_aux_sends(track_data.track_id, aux_sends);

    return make_ok();
}

Ret Mixer::add_aux_track(TrackChainPtr track_chain) {
    const size_t out_buffer_size = output_spec_.samples_per_channel * output_spec_.audio_channel_count;

    TrackData track_data;
    track_data.track_id = track_chain->track_id();
    track_data.chain = track_chain;
    track_data.buffer.resize(out_buffer_size);

    aux_tracks_.emplace_back(std::move(track_data));

    return make_ok();
}

Ret Mixer::remove_track(const TrackId track_id) {

    bool removed = muse::remove_if(tracks_, [track_id](const TrackData &track) { return track.track_id == track_id; });

    if (removed) {
        aux_sends_.erase(track_id);
    }

    if (!removed) {
        removed = muse::remove_if(aux_tracks_, [track_id](const TrackData &track) { return track.track_id == track_id; });
    }

    return removed ? make_ret(Ret::Code::OK) : make_ret(Err::INVALID_TRACK_ID);
}

void Mixer::set_aux_sends(const TrackId track_id, const AuxSendsParams &aux_sends) { aux_sends_[track_id] = aux_sends; }

void Mixer::on_output_spec_changed(const OutputSpec &spec) {
    for (auto &t : tracks_) {
        t.chain->set_output_spec(spec);
    }

    for (auto &t : aux_tracks_) {
        t.chain->set_output_spec(spec);
    }
}

void Mixer::on_mode_changed(const ProcessMode mode) {

    for (auto &t : tracks_) {
        t.chain->set_mode(mode);
    }

    for (auto &t : aux_tracks_) {
        t.chain->set_mode(mode);
    }
}

void Mixer::process(float *out_buffer, samples_t samples_per_channel) {

    if (!enabled_) {
        return;
    }

    const size_t out_buffer_size = samples_per_channel * output_spec_.audio_channel_count;

    process_track_channels(out_buffer_size, samples_per_channel);
    if (OfflineCancellation::requested()) return;

    prepare_aux_buffers(out_buffer_size);

    for (auto &t : tracks_) {
        if (OfflineCancellation::requested()) return;
        if (!t.processed) {
            continue;
        }

        //! NOTE If the signal is silent, do not write to the output buffer
        //! or the aux buffers
        if (is_chain_silent(t.chain)) {
            continue;
        }

        mix_output_from_channel(out_buffer, t.buffer.data(), out_buffer_size);
        write_track_to_aux_buffers(t.buffer.data(), out_buffer_size, aux_sends_[t.track_id]);
    }

    process_aux_channels(out_buffer, samples_per_channel);
}

void Mixer::process_track_channels(size_t out_buffer_size, size_t samples_per_channel) {
    auto process_channel = [out_buffer_size, samples_per_channel](TrackData &track_data) {
        IF_ASSERT_FAILED(track_data.chain) { return; }

        if (track_data.buffer.size() < out_buffer_size) {
            track_data.buffer.resize(out_buffer_size);
        }

        std::fill(track_data.buffer.begin(), track_data.buffer.begin() + out_buffer_size, 0.f);
        track_data.chain->process(track_data.buffer.data(), samples_per_channel);
        track_data.processed = true;
    };

    bool filter_tracks = (mode_ == ProcessMode::IDLE) && !tracks_to_process_when_idle_.empty();

    for (auto &t : tracks_) {
        if (OfflineCancellation::requested()) return;
        t.processed = false;

        if (filter_tracks && !muse::contains(tracks_to_process_when_idle_, t.track_id)) {
            continue;
        }

        process_channel(t);
    }
}

void Mixer::set_non_muted_track_count(size_t count) { non_muted_track_count_ = count; }

void Mixer::set_tracks_to_process_when_idle(const std::unordered_set<TrackId> &track_ids) { tracks_to_process_when_idle_ = track_ids; }

void Mixer::mix_output_from_channel(float *out_buffer, const float *in_buffer, size_t buffer_size) const {
    IF_ASSERT_FAILED(out_buffer && in_buffer) { return; }

    for (size_t i = 0; i < buffer_size; ++i) {
        out_buffer[i] += in_buffer[i];
    }
}

void Mixer::prepare_aux_buffers(size_t out_buffer_size) {
    for (auto &aux : aux_tracks_) {
        aux.processed = false;
        if (!aux.chain->fx_chain()) {
            continue;
        }
        aux.buffer.resize(out_buffer_size);
        std::fill(aux.buffer.begin(), aux.buffer.begin() + out_buffer_size, 0.f);
    }
}

void Mixer::write_track_to_aux_buffers(const float *track_buffer, size_t out_buffer_size, const AuxSendsParams &aux_sends) {
    for (aux_channel_idx_t aux_idx = 0; aux_idx < aux_sends.size(); ++aux_idx) {
        if (aux_idx >= aux_tracks_.size()) {
            break;
        }

        TrackData &aux = aux_tracks_.at(aux_idx);
        if (!aux.chain->fx_chain()) {
            continue;
        }

        const AuxSendParams &aux_send = aux_sends.at(aux_idx);
        if (!aux_send.active || muse::is_zero(aux_send.signal_amount)) {
            continue;
        }

        float *aux_buffer = aux.buffer.data();
        float signal_amount = aux_send.signal_amount;

        for (size_t i = 0; i < out_buffer_size; ++i) {
            aux_buffer[i] += track_buffer[i] * signal_amount;
        }

        aux.processed = true;
    }
}

void Mixer::process_aux_channels(float *buffer, samples_t samples_per_channel) {
    const size_t out_buffer_size = samples_per_channel * output_spec_.audio_channel_count;

    for (TrackData &aux : aux_tracks_) {
        if (OfflineCancellation::requested()) return;
        //! NOTE Process when the aux received a signal this block (aux.processed) and/or if
        //! it's not yet silent (e.g. reverb is still ringing out)
        if (!aux.processed && is_chain_silent(aux.chain)) {
            continue;
        }

        float *aux_buffer = aux.buffer.data();
        aux.chain->process(aux_buffer, samples_per_channel);

        //! NOTE If the signal is silent, do not write to the output buffer
        if (!is_chain_silent(aux.chain)) {
            mix_output_from_channel(buffer, aux_buffer, out_buffer_size);
        }
    }
}

std::string Mixer::dump() const {
    std::stringstream ss;
    ss << "\n";
    ss << name() << ":";

    int indent = 2;

    ss << "\n";
    ss << std::string(indent, ' ') << "tracks: " << tracks_.size();
    for (const auto &track : tracks_) {
        ss << "\n";
        ss << std::string(indent, ' ') << "<--[" << track.track_id << "] " << track.chain->dump();
    }

    ss << "\n";
    ss << std::string(indent, ' ') << "auxs: " << aux_tracks_.size();
    for (const auto &aux : aux_tracks_) {
        ss << "\n";
        ss << std::string(indent, ' ') << "<--[" << aux.track_id << "] " << aux.chain->dump();
    }

    return ss.str();
}
