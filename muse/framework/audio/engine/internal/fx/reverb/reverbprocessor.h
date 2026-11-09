/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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

#ifndef MUSE_AUDIO_REVERBPROCESSOR_H
#define MUSE_AUDIO_REVERBPROCESSOR_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "audio/common/audiotypes.h"

namespace muse::audio::fx {
struct ReverbProcessor {
    ReverbProcessor(const audio::AudioFxParams &params);
    ~ReverbProcessor();

    void init(const OutputSpec &spec);

    AudioFxType type() const;
    std::string name() const;
    const AudioFxParams &params() const;
    void set_output_spec(const OutputSpec &spec);

    bool active() const;
    void set_active(bool active);

    void set_mode(const ProcessMode mode);

    bool should_process_during_silence() const;

    void process(float *buffer, samples_t sample_count, samples_t playback_position_samples = 0);

private:
    enum Params {
        QUALITY,
        VELVET_IN,
        VELVET_OUT,

        DRY_LEVEL,
        LATE_LEVEL,
        ER_LEVEL,
        E_RTO_LATE,

        PRE_DELAY_MS,
        STEREO_SPREAD,
        MOD_DELAY_FREQ,
        MOD_DELAY_AMP,
        MOD_TYPE, // currently 0 = sine with regular phase offset | 1 = all channel synced sine

        REVERB_TIME_MS,
        LATE_ROOM_SCALE,
        TIME_LOW,
        TIME_MID,
        TIME_HIGH,
        FEEDBACK_TOP,
        XOVER_LOW_MID,
        XOVER_MID_HIGH,

        LOW_CUT_FREQ,
        HIGH_CUT_FREQ,
        PEAK_FREQ,
        PEAK_GAIN,
        PEAK_Q,

        NUM_PARAMS,
    };

    struct ParameterInfo {
        std::string name;
        std::pair<float, float> range;
    };

    void get_parameter_info(int32_t index, ParameterInfo &info);

    float get_parameter(int32_t index);

    void set_parameter(int32_t index, float new_value);

    bool set_format(audioch_t input_audio_channels_count, double input_sample_rate, int32_t maximum_block_size);

    void delete_signal_buffers();

    void reset();

    // Base Processor data
    struct Processor {
        struct Parameter {
            std::string name;
            std::pair<float, float> value_range = {0.f, 1.f};
            float current_value = 0.f;
        };

        std::vector<Parameter> param;

        void allocate_parameters(int num);
        void setup_parameter(int index, const std::string &name, std::pair<float, float> value_range, float initial_value);
        bool set_format(audioch_t input_audio_channels_count, double input_sample_rate, int32_t maximum_block_size);

        audioch_t audio_channels_count_value = 0;
        double sample_rate_value = 0.0;
        double sample_t = 0.0;
        int block_size = 0;
    };

    Processor processor_;

    // Specific effect data
    template <int num_lines>
    void process_lines(float **signal_ptr, int32_t num_samples);
    static constexpr int max_num_delays = 24;

    void calculate_tail_params();
    void calculate_mod_params();

    struct Impl;
    Impl *d = nullptr; // aligned pointer

    float late_gain_ = 0.1f;
    float late_gain_correction_ = 1.f;
    float er_to_late_gain_ = 0.f;

    int delays_ = 16;

    AudioFxParams params_;

    float **signal_buffers_ = nullptr;
};
} // namespace muse::audio::fx

#endif // MUSE_AUDIO_REVERBPROCESSOR_H
