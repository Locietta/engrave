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

#ifndef MUSE_AUDIO_FLUIDTRACK_H
#define MUSE_AUDIO_FLUIDTRACK_H

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <amc/flatset.hpp>

#include "../../../offlinerendersettings.h"
#include "../../midieventsink.h"
#include "audio/common/timeposition.h"
#include "fluidsequencer.h"
#include "global/io/path.h"

namespace muse::audio::engine {
struct AudioResources;
} // namespace muse::audio::engine

namespace muse::audio::synth {

struct Fluid;

struct FluidTrack {
#ifdef ENGRAVE_PLAYBACK_HARNESS
    static int live_instances();
#endif
    // The observer preserves legacy staff remapping. Resources outlive this per-track owner.
    FluidTrack(const audio::AudioSourceParams &params, const engine::OfflineRenderSettings &config, engine::MidiEventSink midi_event_sink,
               engine::AudioResources &resources);

    ~FluidTrack();
    FluidTrack(const FluidTrack &) = delete;
    FluidTrack &operator=(const FluidTrack &) = delete;
    void setup(const mpe::TrackEvents &events);

    ProcessMode mode() const { return mode_; }

    const OutputSpec &output_spec() const { return output_spec_; }

    const AudioInputParams &params() const { return params_; }

    Ret init(const OutputSpec &spec);

    Ret add_sound_fonts(const std::vector<io::PathT> &sfonts);
    void set_preset(const std::optional<midi::Program> &preset);

    std::string name() const;
    AudioSourceType type() const;

    void setup_sound(const mpe::PlaybackSetupData &setup_data);
    void setup_events(const mpe::TrackEvents &events);

    void flush_sound(); // all channels

    TimePosition playback_position() const;
    void set_playback_position(const TimePosition &position);

    samples_t process(float *buffer, samples_t samples_per_channel);

    void set_mode(const ProcessMode mode);
    void set_output_spec(const OutputSpec &spec);

    bool is_valid() const;

private:
    struct KeyTuning {
        std::vector<int> keys;
        std::vector<double> pitches;

        void add(int key, double tuning) {
            keys.push_back(key);
            pitches.push_back((key * 100.0) + tuning);
        }

        int size() const { return static_cast<int>(keys.size()); }

        void reset() {
            keys.clear();
            pitches.clear();
        }

        bool is_empty() const { return keys.empty() && pitches.empty(); }
    };

    void create_fluid_instance();

    void do_flush_sound();

    bool process_sequence(std::span<const engine::TimedMidiEvent> sequence, samples_t samples, float *buffer);
    bool handle_event(const midi::Event &event);

    void update_expression_levels();

    int set_controller_value(int channel, int ctrl, int value);
    int set_pitch_bend(int channel, int pitch_bend);

    msecs_t samples_to_msecs(samples_t samples, samples_t rate) const;
    samples_t micro_secs_to_samples(msecs_t duration, samples_t rate) const;
    ProcessMode mode_ = ProcessMode::UNDEFINED;
    mpe::PlaybackSetupData setup_data_;
    AudioInputParams params_;
    engine::OfflineRenderSettings config_;
    engine::AudioResources &resources_;
    OutputSpec output_spec_;

    std::unique_ptr<Fluid> fluid_;
    engine::MidiEventSink midi_event_sink_ = nullptr;

    FluidSequencer sequencer_;
    amc::FlatSet<io::PathT> sfont_paths_;
    std::optional<midi::Program> preset_;

    KeyTuning tuning_;

    bool flush_sound_requested_ = false;
};

} // namespace muse::audio::synth

#endif //MUSE_AUDIO_FLUIDTRACK_H
