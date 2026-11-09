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

#include "reverbprocessor.h"

#include <cassert>
#include <cmath>
#include <complex>
#include <memory>
#include <new>
#include <vector>

#include "allpassdispersion.h"
#include "allpassmodulateddelay.h"
#include "iirbiquadfilter.h"
#include "ivndecorrelation.h"
#include "reverbfilters.h"
#include "reverbmatrices.h"
#include "sampledelay.h"
#include "simdtypes.h"
#include "smoothgain.h"

namespace muse::audio::fx {
float from_decibel(float d_b) { return std::pow(10.f, d_b / 20.f); }

struct SamplesFloat {
    ~SamplesFloat() {
        for (int ch = 0; ch < num_channels; ch++) {
            dealloc(ch);
        }
    }

    void set_size(int32_t input_num_channels, int32_t samples) {
        for (int ch = 0; ch < num_channels; ch++) {
            dealloc(ch);
        }

        // Size the pointer table before publishing the channel count: a throwing
        // resize leaves the old (all-null) table and count, safe to destroy.
        data.resize(input_num_channels);
        num_channels = input_num_channels;
        num_samples = samples;
        for (int ch = 0; ch < num_channels; ch++) {
            alloc(ch, num_samples);
        }
    }

    float **get_ptrs() { return data.data(); }

    float *get_ptr(int32_t channel) {
        assert(channel < num_channels);
        assert(data[channel]);
        return data[channel];
    }

    const float *get_ptr(int32_t channel) const {
        assert(channel < num_channels);
        assert(data[channel]);
        return data[channel];
    }

    void assign_samples(int32_t channel, const float *input) {
        assert(channel < num_channels);
        assert(data[channel]);
        vo::copy(input, data[channel], num_samples);
    }

    void assign_samples(const SamplesFloat &rhs) {
        assert(num_channels == rhs.num_channels);
        assert(num_samples == rhs.num_samples);
        for (int ch = 0; ch < num_channels; ch++) {
            assert(data[ch]);
            vo::copy(rhs.get_ptr(ch), get_ptr(ch), num_samples);
        }
    }

    void zero_out() {
        for (int ch = 0; ch < num_channels; ch++) {
            vo::set_to_zero(data[ch], num_samples);
        }
    }

private:
    int32_t num_channels{0};
    int32_t num_samples{0};
    std::vector<float *> data;

    void alloc(int32_t channel, int32_t samples) {
        assert(channel < num_channels);
        if (data[channel]) {
            dealloc(channel);
        }
        data[channel] = (float *) simd::aligned_malloc(samples * sizeof(float), 64);
    }

    void dealloc(int32_t channel) {
        assert(channel < num_channels);
        if (data[channel]) {
            simd::aligned_free(data[channel]);
            data[channel] = nullptr;
        }
    }
};

struct MultiPhaseSinLfo {
    void setup(int num_taps, float freq, float amplitude, double sample_rate) {
        phase = 0.f;
        phase_inc = float(6.28318530717958647692 * freq / sample_rate);

        rotations.resize(num_taps);
        amplitude_value = amplitude;
        float phase_delta = float(6.28318530717958647692 / num_taps);
        for (int i = 0; i < num_taps; ++i) {
            rotations[i] = {std::cos(i * phase_delta), std::sin(i * phase_delta)};
        }
    }

    float get_next_main_value() {
        phase += phase_inc;
        if (phase > 6.28318530717958647692f) {
            phase -= 6.28318530717958647692f;
        }
        main_val_sin = std::sin(phase) * amplitude_value;
        main_val_cos = std::cos(phase) * amplitude_value;
        return main_val_sin;
    }

    float get_tap_value(int n) const { return main_val_cos * rotations[n].imag() + main_val_sin * rotations[n].real(); }

private:
    float phase = 0.f;
    float phase_inc = 0.1f;
    float main_val_sin = 0.f, main_val_cos = 1.f;
    float amplitude_value = 1.f;
    std::vector<std::complex<float>> rotations;
};

static inline const int *delay_times_for_n(int n) {
    // these are ~logarithmically spaced primes
    static const int delay_times8[] = {839, 947, 1069, 1213, 1373, 1549, 1753, 1979};
    static const int delay_times12[] = {839, 911, 991, 1069, 1163, 1259, 1373, 1487, 1613, 1753, 1901, 2063};
    static const int delay_times16[] = {839, 887, 947, 1009, 1069, 1151, 1213, 1289, 1373, 1459, 1549, 1657, 1753, 1861, 1979, 2099};
    static const int delay_times24[] = {839,  877,  911,  947,  991,  1031, 1069, 1117, 1163, 1213, 1259, 1319,
                                        1373, 1427, 1487, 1549, 1613, 1693, 1753, 1823, 1901, 1979, 2063, 2143};

    switch (n) {
        case 24: return delay_times24;
        case 16: return delay_times16;
        case 12: return delay_times12;
        default: return delay_times8;
    }
}

static inline float sqrt_sign(float x) { return std::copysign(std::sqrt(std::abs(x)), x); }

struct ReverbProcessor::Impl {
    // members requiring alignment first
    IirBiquadFilter::Coeffs<simd::FloatX4> damping_cf1_x4[max_num_delays / 4];
    IirBiquadFilter::Coeffs<simd::FloatX4> damping_cf2_x4[max_num_delays / 4];
    IirBiquadFilter::DF2State<simd::FloatX4> damping_state1_x4[max_num_delays / 4];
    IirBiquadFilter::DF2State<simd::FloatX4> damping_state2_x4[max_num_delays / 4];
    reverbfilters::OnePoleFilter<simd::FloatX4> ag_filter_x4[max_num_delays / 4];

    AllPassModulatedDelay mod_delay[max_num_delays];
    AllPassDispersion disp_ap;

    ImprovedVelvetNoiseDecorrelation ivnd_in[max_num_delays];
    ImprovedVelvetNoiseDecorrelation ivnd_out[max_num_delays];
    ImprovedVelvetNoiseDecorrelation ivnd_er[2];

    dsp::SmoothLinearValue<float> dry_gain_smooth;
    dsp::SmoothLinearValue<float> late_gain_smooth;
    dsp::SmoothLinearValue<float> er_gain_smooth;

    SamplesFloat ivnd_in_buffer;
    SamplesFloat delay_out_buffer;
    SamplesFloat er_buffer;
    SamplesFloat work_buffer;
    SamplesFloat late_buffer;
    MultiPhaseSinLfo sin_lfo;

    IirBiquadFilter::DF1Processor<double> lo_cut_filter, hi_cut_filter, peak_filter;

    SparseFirFilter er_fir[2];
    SampleDelay<float, 2> pre_delay;

    int mod_step = 32;
    int mod_counter = 0;
};

ReverbProcessor::ReverbProcessor(const AudioFxParams &params) : params_(params) {
    d = simd::aligned_new<Impl>(64);

    processor_.allocate_parameters(NUM_PARAMS);
    processor_.setup_parameter(QUALITY, "Quality", {1.f, 4.f}, 4);

    processor_.setup_parameter(DRY_LEVEL, "DryLevel", {-60.f, 20.f}, -60);
    processor_.setup_parameter(LATE_LEVEL, "LateLevel", {-60.f, 20.f}, -20);
    processor_.setup_parameter(ER_LEVEL, "ERDirect", {-60.f, 20.f}, -20);
    processor_.setup_parameter(E_RTO_LATE, "ERtoLate", {-60.f, 20.f}, -20);

    processor_.setup_parameter(PRE_DELAY_MS, "PreDelay", {0.f, 500.f}, 10);
    processor_.setup_parameter(STEREO_SPREAD, "Stereo", {0.f, 150.f}, 100);

    processor_.setup_parameter(REVERB_TIME_MS, "ReverbTimeMs", {100.f, 10000.f}, 2200);
    processor_.setup_parameter(LATE_ROOM_SCALE, "LateRoomScale", {0.5f, 4.f}, 0.8f);
    processor_.setup_parameter(FEEDBACK_TOP, "FeedbackTop", {500.f, 20000.f}, 8000);

    processor_.setup_parameter(TIME_LOW, "TimeLow", {50.f, 200.f}, 100);
    processor_.setup_parameter(TIME_MID, "TimeMid", {50.f, 200.f}, 100);
    processor_.setup_parameter(TIME_HIGH, "TimeHigh", {50.f, 200.f}, 60);
    processor_.setup_parameter(XOVER_LOW_MID, "LowMidFreq", {50.f, 500.f}, 400);
    processor_.setup_parameter(XOVER_MID_HIGH, "MidHighFreq", {1000.f, 10000.f}, 4000);

    processor_.setup_parameter(MOD_DELAY_FREQ, "ModFreq", {0.f, 10.f}, 1.f);
    processor_.setup_parameter(MOD_DELAY_AMP, "ModAmp", {0.f, 1.f}, 0.2f);
    processor_.setup_parameter(MOD_TYPE, "ModType", {0.f, 1.f}, 0);

    processor_.setup_parameter(VELVET_IN, "VelvetIn", {0.f, 1.f}, 0);
    processor_.setup_parameter(VELVET_OUT, "VelvetOut", {0.f, 1.f}, 1);

    processor_.setup_parameter(LOW_CUT_FREQ, "LowCut", {10.f, 25000.f}, 10.f);
    processor_.setup_parameter(HIGH_CUT_FREQ, "HighCut", {10.f, 25000.f}, 25000.f);

    processor_.setup_parameter(PEAK_FREQ, "PeakFreq", {10.f, 25000.f}, 375.f);
    processor_.setup_parameter(PEAK_GAIN, "PeakGain", {-24.f, +24.f}, 0.f);
    processor_.setup_parameter(PEAK_Q, "PeakQ", {0.05f, 16.f}, 1);

    // Refresh parameters that need initialisation
    set_parameter(Params::DRY_LEVEL, get_parameter(Params::DRY_LEVEL));
    set_parameter(Params::LATE_LEVEL, get_parameter(Params::LATE_LEVEL));
    set_parameter(Params::ER_LEVEL, get_parameter(Params::ER_LEVEL));
    set_parameter(Params::E_RTO_LATE, get_parameter(Params::E_RTO_LATE));
    set_parameter(Params::MOD_DELAY_AMP, get_parameter(Params::MOD_DELAY_AMP));
    set_parameter(Params::LATE_ROOM_SCALE, get_parameter(Params::LATE_ROOM_SCALE));
    set_parameter(Params::REVERB_TIME_MS, get_parameter(Params::REVERB_TIME_MS));
    set_parameter(Params::LOW_CUT_FREQ, get_parameter(Params::LOW_CUT_FREQ));
    set_parameter(Params::HIGH_CUT_FREQ, get_parameter(Params::HIGH_CUT_FREQ));
    set_parameter(Params::PEAK_FREQ, get_parameter(Params::PEAK_FREQ));
    set_parameter(Params::QUALITY, get_parameter(Params::QUALITY));
    set_parameter(Params::PRE_DELAY_MS, get_parameter(Params::PRE_DELAY_MS));
    set_parameter(Params::FEEDBACK_TOP, get_parameter(Params::FEEDBACK_TOP));
}

void ReverbProcessor::init(const OutputSpec &spec) { set_output_spec(spec); }

ReverbProcessor::~ReverbProcessor() {
    simd::aligned_delete(d);
    delete_signal_buffers();
}

AudioFxType ReverbProcessor::type() const { return AudioFxType::MUSE_FX; }

std::string ReverbProcessor::name() const { return "Muse/Reverb"; }

const AudioFxParams &ReverbProcessor::params() const { return params_; }

void ReverbProcessor::set_output_spec(const OutputSpec &spec) {
    IF_ASSERT_FAILED(spec.is_valid()) { return; }

    set_format(spec.audio_channel_count, spec.sample_rate, spec.samples_per_channel);
}

bool ReverbProcessor::active() const { return params_.active; }

void ReverbProcessor::set_active(bool active) { params_.active = active; }

void ReverbProcessor::set_mode(const ProcessMode) {}

bool ReverbProcessor::should_process_during_silence() const { return false; }

void ReverbProcessor::process(float *buffer, samples_t sample_count, samples_t) {
    // A failed earlier resize leaves no signal buffers; retry the allocation.
    if (processor_.block_size != static_cast<int>(sample_count) || !signal_buffers_) {
        set_format(processor_.audio_channels_count_value, processor_.sample_rate_value, sample_count);
    }

    for (samples_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        size_t offset = sample_index * processor_.audio_channels_count_value;

        for (audioch_t audio_channel_index = 0; audio_channel_index < processor_.audio_channels_count_value; ++audio_channel_index) {
            signal_buffers_[audio_channel_index][sample_index] = buffer[offset + audio_channel_index];
        }
    }

    switch (delays_) {
        case 24: process_lines<24>(signal_buffers_, static_cast<int32_t>(sample_count)); break;
        case 16: process_lines<16>(signal_buffers_, static_cast<int32_t>(sample_count)); break;
        case 12: process_lines<12>(signal_buffers_, static_cast<int32_t>(sample_count)); break;
        default: process_lines<8>(signal_buffers_, static_cast<int32_t>(sample_count)); break;
    }

    for (samples_t sample_index = 0; sample_index < sample_count; ++sample_index) {
        size_t offset = sample_index * processor_.audio_channels_count_value;

        for (audioch_t audio_channel_index = 0; audio_channel_index < processor_.audio_channels_count_value; ++audio_channel_index) {
            buffer[offset + audio_channel_index] = signal_buffers_[audio_channel_index][sample_index];
        }
    }
}

void ReverbProcessor::get_parameter_info(int32_t index, ParameterInfo &info) {
    assert(index < NUM_PARAMS);
    info.name = processor_.param[index].name;
    info.range = processor_.param[index].value_range;
}

float ReverbProcessor::get_parameter(int32_t index) {
    assert(index < NUM_PARAMS);
    return processor_.param[index].current_value;
}

void ReverbProcessor::calculate_tail_params() {
    const int *delay_times = delay_times_for_n(delays_);
    float scale = float(get_parameter(LATE_ROOM_SCALE) * processor_.sample_rate_value / 44100.);

    const float log_db60 = std::log(from_decibel(-60.f));

    float time_smp = float(get_parameter(REVERB_TIME_MS) * 0.001 * processor_.sample_rate_value);

    auto decay_per_sample = log_db60 / (time_smp);
    auto decay_per_sample_l = decay_per_sample / (get_parameter(TIME_LOW) * 0.01f);
    auto decay_per_sample_m = decay_per_sample / (get_parameter(TIME_MID) * 0.01f);
    auto decay_per_sample_h = decay_per_sample / (get_parameter(TIME_HIGH) * 0.01f);

    // correct gain to keep the level of the first late output sample constant
    late_gain_correction_ = 1.f / std::exp(delay_times[0] * scale * decay_per_sample);
    d->late_gain_smooth.set_target_value(late_gain_ * late_gain_correction_);

    for (int i = 0; i < delays_; ++i) {
        float delay_time = delay_times[i] * scale;

        float gain_l = std::exp(delay_time * decay_per_sample_l);
        float gain_m = std::exp(delay_time * decay_per_sample_m);
        float gain_h = std::exp(delay_time * decay_per_sample_h);
        IirBiquadFilter::Coeffs<float> cf1, cf2;
        IirBiquadFilter::create3_band_tone_control4_p<float>(get_parameter(XOVER_LOW_MID), get_parameter(XOVER_MID_HIGH), gain_l, gain_m,
                                                             gain_h, (float) processor_.sample_rate_value, cf1, cf2);

        float ag_gain = std::exp(delay_time * decay_per_sample_h * 0.5f); // at feedbacktop use half the 'high' delay time
        auto ag_cf =
            reverbfilters::one_pole_coeffs_lowpass1_point<float>(get_parameter(FEEDBACK_TOP), ag_gain, processor_.sample_rate_value);

        // copy coefficients to simd-friendly structures
        int x = i >> 2, y = i & 3;
        d->damping_cf1_x4[x].a1[y] = cf1.a1;
        d->damping_cf1_x4[x].a2[y] = cf1.a2;
        d->damping_cf1_x4[x].b0[y] = cf1.b0;
        d->damping_cf1_x4[x].b1[y] = cf1.b1;
        d->damping_cf1_x4[x].b2[y] = cf1.b2;

        d->damping_cf2_x4[x].a1[y] = cf2.a1;
        d->damping_cf2_x4[x].a2[y] = cf2.a2;
        d->damping_cf2_x4[x].b0[y] = cf2.b0;
        d->damping_cf2_x4[x].b1[y] = cf2.b1;
        d->damping_cf2_x4[x].b2[y] = cf2.b2;

        d->ag_filter_x4[x].cf.b0[y] = ag_cf.b0;
        d->ag_filter_x4[x].cf.b1[y] = ag_cf.b1;
        d->ag_filter_x4[x].cf.a1[y] = ag_cf.a1;
    }
}

void ReverbProcessor::calculate_mod_params() {
    const int *delay_times = delay_times_for_n(delays_);

    float depth_ms = get_parameter(MOD_DELAY_AMP) * get_parameter(LATE_ROOM_SCALE);
    float freq_hz = get_parameter(MOD_DELAY_FREQ);

    float mod_depth_smp = float(depth_ms * 0.001f * processor_.sample_rate_value);
    d->sin_lfo.setup(delays_, freq_hz * d->mod_step, mod_depth_smp, processor_.sample_rate_value);
    float scale = float(get_parameter(LATE_ROOM_SCALE) * processor_.sample_rate_value / 44100.);
    for (int i = 0; i < delays_; ++i) {
        d->mod_delay[i].set_base_delay(delay_times[i] * scale, mod_depth_smp * 2.f);
    }
}

void ReverbProcessor::set_parameter(int32_t index, float new_value) {
    // Store
    assert(index >= 0 && index <= static_cast<int32_t>(processor_.param.size()));
    auto clamped_value =
        std::min(processor_.param[index].value_range.second, std::max(processor_.param[index].value_range.first, new_value));
    processor_.param[index].current_value = clamped_value;

    // Update
    switch (index) {
        case Params::MOD_DELAY_FREQ:
        case Params::MOD_DELAY_AMP: calculate_mod_params(); break;
        case Params::LATE_ROOM_SCALE:
            calculate_tail_params();
            calculate_mod_params();
            break;
        case Params::REVERB_TIME_MS:
        case Params::TIME_LOW:
        case Params::TIME_MID:
        case Params::TIME_HIGH:
        case Params::XOVER_LOW_MID:
        case Params::XOVER_MID_HIGH:
        case Params::FEEDBACK_TOP: calculate_tail_params(); break;

        case Params::LOW_CUT_FREQ:
            d->lo_cut_filter.set_target_coefficients(
                IirBiquadFilter::create_high_pass1_p_matched((double) get_parameter(LOW_CUT_FREQ), processor_.sample_rate_value));
            break;

        case Params::HIGH_CUT_FREQ:
            d->hi_cut_filter.set_target_coefficients(
                IirBiquadFilter::create_low_pass1_p_matched((double) get_parameter(HIGH_CUT_FREQ), processor_.sample_rate_value));
            break;

        case Params::PEAK_FREQ:
        case Params::PEAK_GAIN:
        case Params::PEAK_Q:
            d->peak_filter.set_target_coefficients(IirBiquadFilter::create_band_shelf_matched(
                get_parameter(PEAK_FREQ), get_parameter(PEAK_Q), from_decibel(get_parameter(PEAK_GAIN)), processor_.sample_rate_value));
            break;

        case Params::DRY_LEVEL:
            d->dry_gain_smooth.set_target_value(
                (new_value > processor_.param[Params::DRY_LEVEL].value_range.first) ? from_decibel(new_value) : 0.f);
            break;
        case Params::LATE_LEVEL:
            late_gain_ = (new_value > processor_.param[Params::LATE_LEVEL].value_range.first) ? from_decibel(new_value) : 0.f;
            d->late_gain_smooth.set_target_value(late_gain_ * late_gain_correction_);
            break;
        case Params::ER_LEVEL:
            d->er_gain_smooth.set_target_value(
                (new_value > processor_.param[Params::ER_LEVEL].value_range.first) ? from_decibel(new_value) : 0.f);
            break;
        case Params::E_RTO_LATE:
            er_to_late_gain_ = (new_value > processor_.param[Params::E_RTO_LATE].value_range.first) ? from_decibel(new_value) : 0.f;
            break;
        case Params::QUALITY:
            switch (int(new_value)) {
                case 4: delays_ = 24; break;
                case 3: delays_ = 16; break;
                case 2: delays_ = 12; break;
                default: delays_ = 8; break;
            }
            calculate_tail_params();
            calculate_mod_params();
            reset();
            break;
        case Params::PRE_DELAY_MS: {
            int smp = int(new_value * 0.001 * processor_.sample_rate_value + 0.5);
            d->pre_delay.set_delay_samples(smp);
            break;
        }
    }
}

#ifdef ENGRAVE_PLAYBACK_HARNESS
thread_local int ReverbProcessor::fail_signal_allocation = -1;
#endif

bool ReverbProcessor::set_format(audioch_t audio_channels_count, double sample_rate, int32_t maximum_block_size) {
    IF_ASSERT_FAILED(audio_channels_count > 0 && audio_channels_count <= 2) { return false; }

    if (signal_buffers_) {
        if (audio_channels_count > processor_.audio_channels_count_value || maximum_block_size > processor_.block_size) {
            delete_signal_buffers();
        }
    }

    // Store base information
    processor_.set_format(audio_channels_count, sample_rate, maximum_block_size);

    if (!signal_buffers_) {
        // Publish only complete storage. Sample arrays retain default (uninitialized)
        // allocation, preserving the DSP's existing initialization and contents.
        const auto checkpoint = [](int allocation) {
#ifdef ENGRAVE_PLAYBACK_HARNESS
            if (fail_signal_allocation == allocation) throw std::bad_alloc();
#else
            (void) allocation;
#endif
        };
        checkpoint(0);
        auto pointers = std::make_unique<float *[]>(processor_.audio_channels_count_value);
        std::unique_ptr<float[]> channels[2];
        for (audioch_t i = 0; i < processor_.audio_channels_count_value; ++i) {
            checkpoint(i + 1);
            channels[i].reset(new float[maximum_block_size]);
            pointers[i] = channels[i].get();
        }
        signal_buffer_channels_ = processor_.audio_channels_count_value;
        signal_buffers_ = pointers.release();
        for (auto &channel : channels) channel.release();
    }

    d->work_buffer.set_size(2, maximum_block_size);
    d->er_buffer.set_size(2, maximum_block_size);
    d->late_buffer.set_size(2, maximum_block_size);

    int max_predelay_smp = int(sample_rate * 0.001 * processor_.param[Params::PRE_DELAY_MS].value_range.second + 0.5);
    d->pre_delay.allocate_for_max_delay_samples(max_predelay_smp);
    for (int i = 0; i < max_num_delays; ++i) {
        d->ivnd_in[i].configure(i, sample_rate, maximum_block_size);
        d->ivnd_out[i].configure(32 + i, sample_rate, maximum_block_size);
    }
    d->ivnd_er[0].configure(31, sample_rate, maximum_block_size);
    d->ivnd_er[1].configure(63, sample_rate, maximum_block_size);
    d->ivnd_in_buffer.set_size(max_num_delays, maximum_block_size);
    d->delay_out_buffer.set_size(max_num_delays, maximum_block_size);

    for (int i = 0; i < 2; ++i) {
        d->er_fir[i].set_format(maximum_block_size, int(0.15 * sample_rate));
        d->er_fir[i].clear_impulses();
    }
    d->er_fir[0].append_impulse(int(0.0201 * sample_rate), 1.f);
    d->er_fir[1].append_impulse(int(0.0343 * sample_rate), 1.f);
    d->er_fir[0].append_impulse(int(0.0382 * sample_rate), 0.4f);
    d->er_fir[1].append_impulse(int(0.0548 * sample_rate), 0.4f);
    d->er_fir[0].append_impulse(int(0.0633 * sample_rate), 0.16f);
    d->er_fir[1].append_impulse(int(0.0775 * sample_rate), 0.16f);
    d->er_fir[0].append_impulse(int(0.0961 * sample_rate), 0.064f);
    d->er_fir[1].append_impulse(int(0.1100 * sample_rate), 0.064f);

    auto smooth_steps = int(0.02f * processor_.sample_rate_value + 0.5f);
    d->lo_cut_filter.set_smoothing_steps(smooth_steps);
    d->hi_cut_filter.set_smoothing_steps(smooth_steps);
    d->peak_filter.set_smoothing_steps(smooth_steps);
    d->dry_gain_smooth.set_steps(smooth_steps);
    d->late_gain_smooth.set_steps(smooth_steps);
    d->er_gain_smooth.set_steps(smooth_steps);

    calculate_mod_params();

    // Refresh dependent params
    set_parameter(Params::PRE_DELAY_MS, get_parameter(Params::PRE_DELAY_MS));
    set_parameter(Params::MOD_DELAY_FREQ, get_parameter(Params::MOD_DELAY_FREQ));
    set_parameter(Params::REVERB_TIME_MS, get_parameter(Params::REVERB_TIME_MS));
    set_parameter(Params::VELVET_IN, get_parameter(Params::VELVET_IN));
    set_parameter(Params::LOW_CUT_FREQ, get_parameter(Params::LOW_CUT_FREQ));
    set_parameter(Params::HIGH_CUT_FREQ, get_parameter(Params::HIGH_CUT_FREQ));
    set_parameter(Params::QUALITY, get_parameter(Params::QUALITY));

    return true;
}

void ReverbProcessor::delete_signal_buffers() {
    for (audioch_t i = 0; i < signal_buffer_channels_; ++i) {
        delete[] signal_buffers_[i];
    }

    delete[] signal_buffers_;
    signal_buffers_ = nullptr;
    signal_buffer_channels_ = 0;
}

void ReverbProcessor::reset() {
    d->disp_ap.reset();
    d->pre_delay.reset();
    for (int ch = 0; ch < 2; ++ch) {
        d->er_fir[ch].reset();
        d->ivnd_er[ch].reset();
    }
    for (int i = 0; i < delays_; ++i) {
        d->ivnd_in[i].reset();
        d->ivnd_out[i].reset();
        d->mod_delay[i].reset();
    }
    for (int j = 0; j < (delays_ >> 2); ++j) {
        IirBiquadFilter::reset_state(d->damping_state1_x4[j]);
        IirBiquadFilter::reset_state(d->damping_state2_x4[j]);
        d->ag_filter_x4[j].reset();
    }
    d->lo_cut_filter.reset();
    d->hi_cut_filter.reset();
    d->peak_filter.reset();

    d->dry_gain_smooth.set_to_target();
    d->late_gain_smooth.set_to_target();
    d->er_gain_smooth.set_to_target();
}

template <int num_lines>
void ReverbProcessor::process_lines(float **signal_ptr, int32_t num_samples) {
    static_assert(num_lines == 8 || num_lines == 12 || num_lines == 16 || num_lines == 24);

    d->work_buffer.set_size(2, num_samples);
    auto **work_ptr = d->work_buffer.get_ptrs();
    d->er_buffer.set_size(2, num_samples);
    auto **er_ptr = d->er_buffer.get_ptrs();
    d->late_buffer.set_size(2, num_samples);
    auto **late_ptr = d->late_buffer.get_ptrs();

    // handle mono by using the same buffer twice
    float *signal_in[] = {signal_ptr[0], processor_.audio_channels_count_value == 2 ? signal_ptr[1] : signal_ptr[0]};
    float *signal_out[] = {signal_ptr[0], processor_.audio_channels_count_value == 2 ? signal_ptr[1] : signal_ptr[0]};

    // pre-delay, dispersion and velvet-input
    d->work_buffer.assign_samples(0, signal_in[0]);
    d->work_buffer.assign_samples(1, signal_in[1]);
    d->pre_delay.process_block(work_ptr, 2, num_samples);

    const bool velvet_input = !real_is_null(get_parameter(VELVET_IN));

    bool er_muted = d->er_gain_smooth.is_static_at_value(0.f);
    bool late_muted = d->late_gain_smooth.is_static_at_value(0.f);

    // early reflections
    if (!er_muted || (er_to_late_gain_ > 0.f && !late_muted)) {
        for (int ch = 0; ch < 2; ++ch) {
            d->er_fir[ch].process_block(work_ptr[ch], er_ptr[ch], num_samples);
            // add to late feedback input (work buffer)
            vo::constant_multiply_and_add(er_ptr[ch], er_to_late_gain_, work_ptr[ch], num_samples);
        }

        if (velvet_input) {
            for (int ch = 0; ch < 2; ++ch) {
                d->ivnd_er[ch].process_block(er_ptr[ch], er_ptr[ch], num_samples);
            }
        }
    }

    if (!late_muted) {
        // construct input buffers for all delay lines.
        const float *mirrored_input[num_lines]; // a helper pointer array for non-disp input
        const float **delay_in_ptr;
        if (velvet_input) {
            for (int i = 0; i < num_lines; ++i) {
                d->ivnd_in[i].process_block(work_ptr[i & 1], d->ivnd_in_buffer.get_ptr(i), num_samples);
            }
            delay_in_ptr = (const float **) d->ivnd_in_buffer.get_ptrs();
        } else {
            for (int i = 0; i < num_lines; ++i) {
                mirrored_input[i] = work_ptr[i & 1];
            }
            delay_in_ptr = mirrored_input;
        }
        auto delay_out_ptr = d->delay_out_buffer.get_ptrs();

        // feedback loop
        for (int cnt = 0; cnt < num_samples; ++cnt) {
            // update delay modulation offsets
            if (d->mod_counter++ >= d->mod_step) {
                d->mod_counter = 0;

                int mod_type = int(get_parameter(Params::MOD_TYPE));
                switch (mod_type) {
                    case 0: // phase distributed sine waves
                        d->mod_delay[0].set_mod_offset(d->sin_lfo.get_next_main_value());
                        for (int i = 1; i < num_lines; ++i) {
                            d->mod_delay[i].set_mod_offset(d->sin_lfo.get_tap_value(i));
                        }
                        break;
                    case 1: {
                        auto offset = d->sin_lfo.get_next_main_value();
                        for (int i = 0; i < num_lines; ++i) {
                            d->mod_delay[i].set_mod_offset(offset);
                        }
                        break;
                    }
                }
            }

            // delay line outputs / decay filters
            float mat_in[num_lines];
            for (int i = 0; i < num_lines; i += 4) {
                int j = i >> 2;
                simd::FloatX4 s = {d->mod_delay[i].read_sample(), d->mod_delay[i + 1].read_sample(), d->mod_delay[i + 2].read_sample(),
                                   d->mod_delay[i + 3].read_sample()};

                s = d->ag_filter_x4[j].process_sample(s);
                s = IirBiquadFilter::process_sample_d_f2(s, d->damping_cf1_x4[j], d->damping_state1_x4[j]);
                s = IirBiquadFilter::process_sample_d_f2(s, d->damping_cf2_x4[j], d->damping_state2_x4[j]);

                mat_in[i] = delay_out_ptr[i][cnt] = s[0];
                mat_in[i + 1] = delay_out_ptr[i + 1][cnt] = s[1];
                mat_in[i + 2] = delay_out_ptr[i + 2][cnt] = s[2];
                mat_in[i + 3] = delay_out_ptr[i + 3][cnt] = s[3];
            }
            // Applying the Matrix
            float mat_res[num_lines];
            reverb_matrices::hadamard<num_lines>(mat_in, mat_res);

            // feeding matrix results and input buffers to the delay lines
            for (int i = 0; i < num_lines; ++i) {
                d->mod_delay[i].write_sample_and_advance(mat_res[i] + delay_in_ptr[i][cnt]);
            }
        } // end of feedback loop

        // output velvet decorrelation
        if (!real_is_null(get_parameter(VELVET_OUT))) {
            for (int i = 0; i < num_lines; ++i) {
                d->ivnd_out[i].process_block(delay_out_ptr[i], delay_out_ptr[i], num_samples);
            }
        }

        // stereo late reverb sum
        vo::subtract(delay_out_ptr[0], delay_out_ptr[3], work_ptr[0], num_samples); // a[0] - a[3]
        vo::subtract(delay_out_ptr[2], delay_out_ptr[1], work_ptr[1], num_samples); // a[2] - a[1]
        for (int i = 4; i < num_lines; i += 4) {
            vo::add(work_ptr[0], delay_out_ptr[i], work_ptr[0], num_samples);          // + a[j + 0]
            vo::subtract(work_ptr[0], delay_out_ptr[i + 3], work_ptr[0], num_samples); // - a[j + 3]
            vo::add(work_ptr[1], delay_out_ptr[i + 2], work_ptr[1], num_samples);      // + a[j + 2]
            vo::subtract(work_ptr[1], delay_out_ptr[i + 1], work_ptr[1], num_samples); // - a[j + 1]
        }
        vo::add(work_ptr[0], work_ptr[1], late_ptr[0], num_samples);      // a[0] - a[1] + a[2] - a[3] ...
        vo::subtract(work_ptr[0], work_ptr[1], late_ptr[1], num_samples); // a[0] + a[1] - a[2] - a[3] ...

        apply_smooth_gain(d->late_gain_smooth, late_ptr, late_ptr, 2, num_samples);
    } else {
        d->late_buffer.zero_out();
    }

    // mix late and er
    if (!er_muted) {
        apply_smooth_gain(d->er_gain_smooth, er_ptr, er_ptr, 2, num_samples);
        for (int ch = 0; ch < 2; ++ch) {
            vo::add(late_ptr[ch], er_ptr[ch], late_ptr[ch], num_samples);
        }
    }

    // stereo spread
    float stereo_spread_fact = get_parameter(STEREO_SPREAD) * 0.01f;
    auto stereo_1 = std::sqrt(0.5f * (1 + stereo_spread_fact));
    auto stereo_2 = sqrt_sign(0.5f * (1 - stereo_spread_fact));

    d->work_buffer.assign_samples(d->late_buffer);
    vo::constant_multiply(work_ptr[0], stereo_1, late_ptr[0], num_samples);
    vo::constant_multiply_and_add(work_ptr[1], stereo_2, late_ptr[0], num_samples);
    vo::constant_multiply(work_ptr[1], stereo_1, late_ptr[1], num_samples);
    vo::constant_multiply_and_add(work_ptr[0], stereo_2, late_ptr[1], num_samples);

    // filtering
    if (!real_is_null(get_parameter(PEAK_GAIN))) {
        d->peak_filter.process(late_ptr, 2, num_samples);
    }

    if (get_parameter(LOW_CUT_FREQ) > processor_.param[Params::LOW_CUT_FREQ].value_range.first) {
        d->lo_cut_filter.process(late_ptr, 2, num_samples);
    }
    if (get_parameter(HIGH_CUT_FREQ) < processor_.param[Params::HIGH_CUT_FREQ].value_range.second) {
        d->hi_cut_filter.process(late_ptr, 2, num_samples);
    }

    // extra dispersion on the entire reverb signal
    d->disp_ap.process(work_ptr, 2, num_samples);

    // output mix
    apply_smooth_gain(d->dry_gain_smooth, signal_in, work_ptr, 2, num_samples);
    vo::add(work_ptr[0], late_ptr[0], signal_out[0], num_samples);
    vo::add(work_ptr[1], late_ptr[1], signal_out[1], num_samples);
}

void ReverbProcessor::Processor::allocate_parameters(int num) { param.resize(num); }

void ReverbProcessor::Processor::setup_parameter(int index, const std::string &name, std::pair<float, float> value_range,
                                                 float initial_value) {
    if (index >= static_cast<int>(param.size())) {
        param.resize(index + 1);
    }

    param[index].name = name;
    param[index].value_range = value_range;
    param[index].current_value = initial_value;
}

bool ReverbProcessor::Processor::set_format(audioch_t audio_channels_count, double sample_rate, int32_t maximum_block_size) {
    assert(sample_rate > 0.0);
    sample_rate_value = sample_rate;
    sample_t = 1.0 / sample_rate_value;
    audio_channels_count_value = audio_channels_count;
    block_size = maximum_block_size;
    return true;
}
} // namespace muse::audio::fx
