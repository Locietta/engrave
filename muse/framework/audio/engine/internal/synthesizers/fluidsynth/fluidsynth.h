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

#ifndef MUSE_AUDIO_FLUIDSYNTH_H
#define MUSE_AUDIO_FLUIDSYNTH_H

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "../abstractsynthesizer.h"
#include "fluidsequencer.h"
#include "global/io/path.h"
#include "midi/imidioutport.h"

namespace muse::audio::synth {
// Release cached fonts after the engine's synthesizers have been destroyed.
void release_sound_font_cache();
struct Fluid;

struct FluidSynth : AbstractSynthesizer {
    // midiOutPort, when given, receives every event the synthesizer handles; it
    // and config must outlive the synthesizer.
    FluidSynth(const audio::AudioSourceParams &params, const engine::IAudioEngineConfiguration &config, midi::IMidiOutPort *midi_out_port);

    Ret init(const OutputSpec &spec);

    Ret add_sound_fonts(const std::vector<io::PathT> &sfonts);
    void set_preset(const std::optional<midi::Program> &preset);

    std::string name() const override;
    AudioSourceType type() const override;

    void setup_sound(const mpe::PlaybackSetupData &setup_data) override;
    void setup_events(const mpe::TrackEvents &events) override;

    void flush_sound() override; // all channels

    TimePosition playback_position() const override;
    void set_playback_position(const TimePosition &position) override;

    samples_t process(float *buffer, samples_t samples_per_channel) override;

    void set_mode(const ProcessMode mode) override;
    void set_output_spec(const OutputSpec &spec) override;

    bool is_valid() const override;

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

    OutputSpec output_spec_;

    std::shared_ptr<Fluid> fluid_;
    midi::IMidiOutPort *midi_out_port_ = nullptr;

    FluidSequencer sequencer_;
    std::set<io::PathT> sfont_paths_;
    std::optional<midi::Program> preset_;

    KeyTuning tuning_;

    bool flush_sound_requested_ = false;
};

using FluidSynthPtr = std::shared_ptr<FluidSynth>;
} // namespace muse::audio::synth

#endif //MUSE_AUDIO_FLUIDSYNTH_H
